#pragma once
#include "resolver.hpp"

extern "C" NTSTATUS DoIndirectSyscall(DWORD ssn, PVOID gadget, DWORD numArgs, ...);

namespace syscalls
{
    #define SAFE_SYSCALL(name, numArgs, ...) \
        static const uint32_t hash = hash_fnv1a(name); \
        auto& entry = g_syscallMap[hash]; \
        if (entry.ssn == 0 || entry.gadget == nullptr) { \
            LogToFile("SAFE_SYSCALL Error: " name " SSN or gadget is null! SSN: " + std::to_string(entry.ssn) + ", Gadget: " + std::to_string(reinterpret_cast<uintptr_t>(entry.gadget))); \
            return 0xC0000001; /* STATUS_UNSUCCESSFUL */ \
        } \
        return DoIndirectSyscall(entry.ssn, entry.gadget, numArgs, __VA_ARGS__);

    inline NTSTATUS NtOpenProcess(
        PHANDLE ProcessHandle,
        ACCESS_MASK DesiredAccess,
        PVOID ObjectAttributes,
        PVOID ClientId
    ) {
        SAFE_SYSCALL("NtOpenProcess", 4, ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
    }

    inline NTSTATUS NtAllocateVirtualMemory(
        HANDLE ProcessHandle,
        PVOID* BaseAddress,
        ULONG_PTR ZeroBits,
        PSIZE_T RegionSize,
        ULONG AllocationType,
        ULONG Protect
    ) {
        SAFE_SYSCALL("NtAllocateVirtualMemory", 6, ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);
    }

    inline NTSTATUS NtFreeVirtualMemory(
        HANDLE ProcessHandle,
        PVOID* BaseAddress,
        PSIZE_T RegionSize,
        ULONG FreeType
    ) {
        SAFE_SYSCALL("NtFreeVirtualMemory", 4, ProcessHandle, BaseAddress, RegionSize, FreeType);
    }

    inline NTSTATUS NtWriteVirtualMemory(
        HANDLE ProcessHandle,
        PVOID BaseAddress,
        PVOID Buffer,
        SIZE_T NumberOfBytesToWrite,
        PSIZE_T NumberOfBytesWritten
    ) {
        SAFE_SYSCALL("NtWriteVirtualMemory", 5, ProcessHandle, BaseAddress, Buffer, NumberOfBytesToWrite, NumberOfBytesWritten);
    }

    inline NTSTATUS NtReadVirtualMemory(
        HANDLE ProcessHandle,
        PVOID BaseAddress,
        PVOID Buffer,
        SIZE_T NumberOfBytesToRead,
        PSIZE_T NumberOfBytesRead
    ) {
        SAFE_SYSCALL("NtReadVirtualMemory", 5, ProcessHandle, BaseAddress, Buffer, NumberOfBytesToRead, NumberOfBytesRead);
    }

    inline NTSTATUS NtProtectVirtualMemory(
        HANDLE ProcessHandle,
        PVOID* BaseAddress,
        PSIZE_T Size,
        ULONG NewProtect,
        PULONG OldProtect
    ) {
        SAFE_SYSCALL("NtProtectVirtualMemory", 5, ProcessHandle, BaseAddress, Size, NewProtect, OldProtect);
    }

    inline NTSTATUS NtCreateThreadEx(
        PHANDLE ThreadHandle,
        ACCESS_MASK DesiredAccess,
        PVOID ObjectAttributes,
        HANDLE ProcessHandle,
        PVOID StartAddress,
        PVOID Parameter,
        ULONG CreateFlags,
        SIZE_T ZeroBits,
        SIZE_T StackSize,
        SIZE_T MaximumStackSize,
        PVOID AttributeList
    ) {
        SAFE_SYSCALL("NtCreateThreadEx", 11, ThreadHandle, DesiredAccess, ObjectAttributes, ProcessHandle, StartAddress, Parameter, CreateFlags, ZeroBits, StackSize, MaximumStackSize, AttributeList);
    }

    inline NTSTATUS NtQueueApcThread(
        HANDLE ThreadHandle,
        PVOID ApcRoutine,
        PVOID ApcArgument1,
        PVOID ApcArgument2,
        PVOID ApcArgument3
    ) {
        SAFE_SYSCALL("NtQueueApcThread", 5, ThreadHandle, ApcRoutine, ApcArgument1, ApcArgument2, ApcArgument3);
    }

    inline NTSTATUS NtQuerySystemInformation(
        int SystemInformationClass,
        PVOID SystemInformation,
        ULONG SystemInformationLength,
        PULONG ReturnLength
    ) {
        SAFE_SYSCALL("NtQuerySystemInformation", 4, SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
    }

    inline NTSTATUS NtQueryInformationProcess(
        HANDLE ProcessHandle,
        int ProcessInformationClass,
        PVOID ProcessInformation,
        ULONG ProcessInformationLength,
        PULONG ReturnLength
    ) {
        SAFE_SYSCALL("NtQueryInformationProcess", 5, ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
    }

    inline NTSTATUS NtSetInformationThread(
        HANDLE ThreadHandle,
        int ThreadInformationClass,
        PVOID ThreadInformation,
        ULONG ThreadInformationLength
    ) {
        SAFE_SYSCALL("NtSetInformationThread", 4, ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength);
    }

    inline NTSTATUS NtClose(
        HANDLE Handle
    ) {
        SAFE_SYSCALL("NtClose", 1, Handle);
    }

    inline NTSTATUS NtTerminateProcess(
        HANDLE ProcessHandle,
        NTSTATUS ExitStatus
    ) {
        SAFE_SYSCALL("NtTerminateProcess", 2, ProcessHandle, ExitStatus);
    }

    inline void hide_current_thread() {
        // NtSetInformationThread(GetCurrentThread(), 0x11 /* ThreadHideFromDebugger */, NULL, 0);
    }
}
