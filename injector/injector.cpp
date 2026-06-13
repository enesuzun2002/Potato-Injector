#include "pch.h"
#include "injector.hpp"
#include <atomic>
#include <memory>
#include <sstream>

Injector* Injector::m_inst = nullptr;

void Injector::initialize()
{
	return;
}

static void SafeInject(Injector* injector, const char* dllPath)
{
	__try {
		injector->injectInner(dllPath);
	}
	__except (syscalls::LogSehException("inject", GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
	}
}

bool Injector::inject(std::string dllPath)
{
	syscalls::LogToFile("Injector::inject: Entry. Path: " + dllPath);
	SafeInject(this, dllPath.c_str());
	return true;
}

bool Injector::injectInner(const char* dllPath)
{
	syscalls::LogToFile("Injector::injectInner: started.");
	syscalls::hide_current_thread();
	if (g_menu->isDebugged)
	{
		syscalls::LogToFile("Injector::injectInner: Blocked due to debugger detection.");
		return false;
	}

	g_menu->isInjecting = true;
	std::vector<BYTE> buffer;
	if (!utils::readFileToMem(std::filesystem::absolute(dllPath), buffer))
	{
		syscalls::LogToFile("Injector::injectInner: Failed to read DLL file: " + std::string(dllPath));
		g_menu->isInjecting = false;
		return false;
	}
	syscalls::LogToFile("Injector::injectInner: DLL read successful. Size: " + std::to_string(buffer.size()) + " bytes.");

	bool result = false;
	if(this->isCustomProcess) {
		std::wstring customName = customProcessName;
		std::string customNameA(customName.begin(), customName.end());
		syscalls::LogToFile("Injector::injectInner: Custom target mode. Target: " + customNameA);
		result = this->map(customName, customName, buffer);
	}
	else {
		std::wstring gameName = vars::get_game_process_name();
		std::wstring gameMod = vars::get_game_mod_name();
		std::string gameNameA(gameName.begin(), gameName.end());
		std::string gameModA(gameMod.begin(), gameMod.end());
		syscalls::LogToFile("Injector::injectInner: Standard game mode. Target Proc: " + gameNameA + ", Target Mod: " + gameModA);
		result = this->map(gameName, gameMod, buffer);
	}

	g_menu->isInjecting = false;
	syscalls::LogToFile("Injector::injectInner: mapping returned: " + std::to_string(result));
	if (result && this->shouldAutoExit) {
		syscalls::LogToFile("Injector::injectInner: shouldAutoExit is true. Initiating menu shutdown.");
		g_menu->isMenuOn = false;
	}
	return result;
}

bool Injector::map(std::wstring_view procname, std::wstring_view modname, std::vector<BYTE> buffer, blackbone::eLoadFlags flags)
{
	std::string procnameA(procname.begin(), procname.end());
	std::string modnameA(modname.begin(), modname.end());
	
	syscalls::LogToFile("Injector::map: Searching for PID of " + procnameA + "...");
	auto mappingFinished = std::make_shared<std::atomic<bool>>(false);
	DWORD pID = NULL;
	
	// Avoid infinite loop if process isn't running by checking multiple times
	int attempts = 0;
	while (true) {
		pID = mem::getProcID(procname);
		if (pID != NULL) {
			break;
		}
		attempts++;
		if (attempts >= 10) {
			syscalls::LogToFile("Injector::map: Process " + procnameA + " was not found after 5 seconds. Aborting map.");
			return false;
		}
		std::this_thread::sleep_for(500ms);
	}

	syscalls::LogToFile("Injector::map: Found PID=" + std::to_string(pID) + ". Attaching...");
	blackbone::Process proc;
	NTSTATUS attachStatus = proc.Attach(pID, PROCESS_ALL_ACCESS);
	if (!NT_SUCCESS(attachStatus)) {
		syscalls::LogToFile("Injector::map: proc.Attach failed with NTSTATUS: " + string::toHex(attachStatus));
		return false;
	}
	syscalls::LogToFile("Injector::map: Attached successfully. Spawning exit-monitoring thread.");

	std::wstring procnameStr(procname);
	std::thread([mappingFinished, procnameStr] {
		syscalls::hide_current_thread();
		while (!mappingFinished->load()) {
			if (mem::getProcID(procnameStr) == NULL)
			{
				mappingFinished->store(true);         //When process exits before mod is ready, this will make sure mapping function aborts.
				break;
			}
			std::this_thread::sleep_for(500ms);
		}
		}).detach();

	syscalls::LogToFile("Injector::map: Waiting for target module " + modnameA + " to be loaded in target process...");
	bool modReady = false;
	int modWaitTime = 0;
	while (!modReady) {
		if (mappingFinished->load())
		{
			syscalls::LogToFile("Injector::map: Target process exited during module wait loop.");
			proc.Detach();
			return false;
		}
		auto mods = proc.modules().GetAllModules();

		auto toLower = [](const std::wstring& str) {
			std::wstring lowerStr = str;
			std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
				[](wchar_t c) { return std::towlower(c); });
			return lowerStr;
			};

		for (const auto& mod : mods) {
			if (toLower(mod.first.first) == toLower(modname.data()))
			{
				syscalls::LogToFile("Injector::map: Found loaded target module: " + modnameA);
				modReady = true;
				break;
			}
		}
		if (modReady) break;

		modWaitTime++;
		if (modWaitTime >= 20) {
			syscalls::LogToFile("Injector::map: Timeout (20s) waiting for target module " + modnameA + ". Proceeding anyway...");
			break;
		}
		std::this_thread::sleep_for(1s);
	}

	const auto modCallback = [](blackbone::CallbackType type, void* context, blackbone::Process& process, const blackbone::ModuleData& modInfo)
	{
		if (type == blackbone::PreCallback)
		{
			if (modInfo.name == L"user32.dll")
				return blackbone::LoadData(blackbone::MT_Native, blackbone::Ldr_Ignore);
		}

		return blackbone::LoadData(blackbone::MT_Default, blackbone::Ldr_Ignore);
	};

	syscalls::LogToFile("Injector::map: Invoking MapImage...");
	const auto result = proc.mmap().MapImage(buffer.size(), buffer.data(), false, flags, modCallback);
	if (!result.success())
	{
		syscalls::LogToFile("Injector::map: MapImage failed with status: " + string::toHex(result.status));
		proc.Detach();
		mappingFinished->store(true);
		return false;
	}

	syscalls::LogToFile("Injector::map: MapImage succeeded!");
	proc.Detach();
	mappingFinished->store(true);
	std::this_thread::sleep_for(1s);   //wait for its child thread to exit.
	return true;
}


