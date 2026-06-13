#pragma once
#include <windows.h>
#include <winternl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include "strcrypt.hpp"

namespace syscalls
{
    // FNV-1a 32-bit hashing
    constexpr uint32_t hash_fnv1a(const char* str) {
        uint32_t hash = 2166136261u;
        while (*str) {
            hash ^= static_cast<uint8_t>(*str++);
            hash *= 16777619u;
        }
        return hash;
    }

    struct SyscallEntry {
        DWORD ssn;
        PVOID gadget;
    };

    inline std::unordered_map<uint32_t, SyscallEntry> g_syscallMap;
    inline PVOID g_syscallGadget = nullptr;

    inline void LogToFileRaw(const char* message) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring exeStr(exePath);
        size_t pos = exeStr.find_last_of(L"\\/");
        std::wstring logPath = (pos != std::wstring::npos ? exeStr.substr(0, pos + 1) : L"") + L"potato_injector.log";
        
        std::ofstream logFile(logPath, std::ios::app);
        if (logFile.is_open()) {
            logFile << message << std::endl;
        }
    }

    inline void LogToFile(const std::string& message) {
        LogToFileRaw(message.c_str());
    }

    template<typename T>
    inline std::string toHex(T val) {
        char buf[32];
        sprintf_s(buf, "0x%llX", static_cast<unsigned long long>(val));
        return std::string(buf);
    }

    inline void LogSehException(const char* threadName, DWORD code) {
        char buf[128];
        sprintf_s(buf, "EXCEPTION: %s crashed with SEH code 0x%lX", threadName, code);
        LogToFileRaw(buf);
    }

    inline bool init_syscalls() {
        LogToFile("init_syscalls: Starting in-memory sorted RVA initialization...");
        if (!g_syscallMap.empty()) {
            LogToFile("init_syscalls: Already initialized.");
            return true;
        }

        constexpr auto ntdll_str = crypt::encrypted("ntdll.dll");
        constexpr auto text_sec_str = crypt::encrypted(".text");

        HMODULE hNtdll = GetModuleHandleW(crypt::decrypt_w(ntdll_str).c_str());
        if (!hNtdll) {
            LogToFile("init_syscalls: Failed to get ntdll module handle.");
            return false;
        }

        PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(hNtdll);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
            LogToFile("init_syscalls: Invalid DOS signature in memory.");
            return false;
        }

        PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(hNtdll) + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) {
            LogToFile("init_syscalls: Invalid NT signature in memory.");
            return false;
        }

        // 1. Find syscall; ret gadget in .text section of in-memory ntdll
        PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
        PIMAGE_SECTION_HEADER textSection = nullptr;
        std::string targetSectionName = crypt::decrypt(text_sec_str);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
            if (strcmp(reinterpret_cast<const char*>(section[i].Name), targetSectionName.c_str()) == 0) {
                textSection = &section[i];
                break;
            }
        }

        if (!textSection) {
            LogToFile("init_syscalls: .text section not found in memory.");
            return false;
        }

        DWORD gadgetRva = 0;
        const uint8_t gadgetBytes[] = { 0x0F, 0x05, 0xC3 }; // syscall; ret
        const uint8_t* textStart = reinterpret_cast<const uint8_t*>(reinterpret_cast<BYTE*>(hNtdll) + textSection->VirtualAddress);
        for (DWORD i = 0; i < textSection->Misc.VirtualSize - 3; ++i) {
            if (textStart[i] == gadgetBytes[0] && textStart[i+1] == gadgetBytes[1] && textStart[i+2] == gadgetBytes[2]) {
                gadgetRva = textSection->VirtualAddress + i;
                break;
            }
        }

        if (gadgetRva == 0) {
            LogToFile("init_syscalls: syscall; ret gadget not found in memory.");
            return false;
        }
        g_syscallGadget = reinterpret_cast<PVOID>(reinterpret_cast<uintptr_t>(hNtdll) + gadgetRva);
        LogToFile("init_syscalls: In-memory gadget address: " + std::to_string(reinterpret_cast<uintptr_t>(g_syscallGadget)));

        // 2. Parse export address table in memory and collect Zw* functions
        IMAGE_DATA_DIRECTORY exportDirData = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (exportDirData.Size == 0) {
            LogToFile("init_syscalls: Export directory size is 0 in memory.");
            return false;
        }

        PIMAGE_EXPORT_DIRECTORY exports = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(
            reinterpret_cast<BYTE*>(hNtdll) + exportDirData.VirtualAddress
        );

        PDWORD names = reinterpret_cast<PDWORD>(reinterpret_cast<BYTE*>(hNtdll) + exports->AddressOfNames);
        PWORD ordinals = reinterpret_cast<PWORD>(reinterpret_cast<BYTE*>(hNtdll) + exports->AddressOfNameOrdinals);
        PDWORD functions = reinterpret_cast<PDWORD>(reinterpret_cast<BYTE*>(hNtdll) + exports->AddressOfFunctions);

        struct ZwEntry {
            std::string name;
            DWORD rva;
        };
        std::vector<ZwEntry> zwList;

        for (DWORD i = 0; i < exports->NumberOfNames; ++i) {
            const char* name = reinterpret_cast<const char*>(reinterpret_cast<BYTE*>(hNtdll) + names[i]);
            // Collect Zw* functions
            if (name[0] == 'Z' && name[1] == 'w') {
                DWORD funcRva = functions[ordinals[i]];
                ZwEntry entry;
                entry.name = name;
                entry.rva = funcRva;
                zwList.push_back(entry);
            }
        }

        // Sort by RVA in ascending order
        std::sort(zwList.begin(), zwList.end(), [](const ZwEntry& a, const ZwEntry& b) {
            return a.rva < b.rva;
        });

        LogToFile("init_syscalls: Collected and sorted " + std::to_string(zwList.size()) + " Zw* exports.");

        // Map both Zw* and Nt* names to their SSNs (since they share the same SSN index)
        for (size_t ssn = 0; ssn < zwList.size(); ++ssn) {
            std::string zwName = zwList[ssn].name;
            std::string ntName = "Nt" + zwName.substr(2);

            SyscallEntry entry;
            entry.ssn = static_cast<DWORD>(ssn);
            entry.gadget = g_syscallGadget;

            g_syscallMap[hash_fnv1a(zwName.c_str())] = entry;
            g_syscallMap[hash_fnv1a(ntName.c_str())] = entry;
        }

        LogToFile("init_syscalls: Mapped " + std::to_string(g_syscallMap.size()) + " Nt*/Zw* functions to SSNs.");
        return !g_syscallMap.empty();
    }
}
