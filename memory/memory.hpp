#pragma once
#include "utils/utils.hpp"
#include <set>

namespace mem
{
	struct CompareProc {
		bool operator()(const std::pair<std::uint32_t, std::wstring>& lhs, const std::pair<std::uint32_t, std::wstring>& rhs) const {
			return lhs.second < rhs.second;
		}
	};

	inline bool isSystemProcess(std::wstring name) {
		name = string::toLower(name);
		static const std::set<std::wstring> systemProcesses = {
			L"system", L"svchost.exe", L"csrss.exe", L"smss.exe", L"wininit.exe", L"services.exe"
		};
		return systemProcesses.find(name) != systemProcesses.end();
	}

	struct MY_SYSTEM_PROCESS_INFORMATION {
		ULONG NextEntryOffset;
		ULONG NumberOfThreads;
		LARGE_INTEGER WorkingSetPrivateSize;
		ULONG HardFaultCount;
		ULONG NumberOfThreadsHighWatermark;
		ULONGLONG CycleTime;
		FILETIME CreateTime;
		FILETIME UserTime;
		FILETIME KernelTime;
		UNICODE_STRING ImageName;
		LONG BasePriority;
		HANDLE UniqueProcessId;
		HANDLE InheritedFromUniqueProcessId;
	};

	inline std::set<std::pair<std::uint32_t, std::wstring>, CompareProc> getProcList() {
		std::set<std::pair<std::uint32_t, std::wstring>, CompareProc> procList;

		ULONG size = 1 << 18; // 256KB
		std::vector<BYTE> buffer(size);
		NTSTATUS status;

		while ((status = syscalls::NtQuerySystemInformation(5, buffer.data(), size, &size)) == 0xC0000004) { // STATUS_INFO_LENGTH_MISMATCH
			buffer.resize(size);
		}

		if (!NT_SUCCESS(status)) {
			syscalls::LogToFile("getProcList: NtQuerySystemInformation failed with status " + std::to_string(status));
			return {};
		}

		uintptr_t bufStart = reinterpret_cast<uintptr_t>(buffer.data());
		uintptr_t bufEnd = bufStart + buffer.size();

		auto pInfo = reinterpret_cast<MY_SYSTEM_PROCESS_INFORMATION*>(buffer.data());
		while (true) {
			uintptr_t currentPtr = reinterpret_cast<uintptr_t>(pInfo);
			if (currentPtr < bufStart || currentPtr + sizeof(MY_SYSTEM_PROCESS_INFORMATION) > bufEnd) {
				syscalls::LogToFile("getProcList: Error: pInfo structure out of bounds!");
				break;
			}

			std::wstring procName;
			if (pInfo->ImageName.Buffer && pInfo->ImageName.Length > 0) {
				uintptr_t strPtr = reinterpret_cast<uintptr_t>(pInfo->ImageName.Buffer);
				if (strPtr >= bufStart && strPtr + pInfo->ImageName.Length <= bufEnd) {
					procName = std::wstring(pInfo->ImageName.Buffer, pInfo->ImageName.Length / sizeof(wchar_t));
				} else {
					syscalls::LogToFile("getProcList: Warning: ImageName.Buffer pointer out of bounds!");
				}
			}

			if (!procName.empty() && !isSystemProcess(procName)) {
				procList.insert(std::make_pair(static_cast<std::uint32_t>(reinterpret_cast<uintptr_t>(pInfo->UniqueProcessId)), procName));
			}

			if (pInfo->NextEntryOffset == 0) break;
			
			if (pInfo->NextEntryOffset < sizeof(MY_SYSTEM_PROCESS_INFORMATION)) {
				syscalls::LogToFile("getProcList: Warning: NextEntryOffset is invalid (" + std::to_string(pInfo->NextEntryOffset) + ")!");
				break;
			}

			uintptr_t nextPtr = currentPtr + pInfo->NextEntryOffset;
			if (nextPtr < bufStart || nextPtr + sizeof(MY_SYSTEM_PROCESS_INFORMATION) > bufEnd) {
				syscalls::LogToFile("getProcList: Error: Next entry pointer out of bounds!");
				break;
			}

			pInfo = reinterpret_cast<MY_SYSTEM_PROCESS_INFORMATION*>(reinterpret_cast<BYTE*>(pInfo) + pInfo->NextEntryOffset);
		}

		return procList;
	}

	inline DWORD getProcID(std::wstring_view procname) {
		const auto procList = getProcList();
		if (procList.empty() || procname.empty()) return NULL;

		auto targetName = string::toLower(procname.data());

		for (const auto& proc : procList)
		{
			auto curprocname = string::toLower(proc.second);

			if (curprocname == targetName)
			{
				return proc.first;
			}
		}

		return NULL;
	}
}