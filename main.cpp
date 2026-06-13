#include "pch.h"

static void SafeMainInner()
{
	if (g_menu->initialize())
	{
		syscalls::LogToFileRaw("wWinMain: Entering loop...");
		g_menu->loop();
		syscalls::LogToFileRaw("wWinMain: Loop finished normally.");
	}
	else
	{
		syscalls::LogToFileRaw("wWinMain: Initialization failed!");
	}
}

static void SafeMain()
{
	__try
	{
		SafeMainInner();
	}
	__except (syscalls::LogSehException("wWinMain", GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	syscalls::LogToFileRaw("wWinMain: Application startup.");
	try
	{
		SafeMain();
	}
	catch (const std::exception& e)
	{
		syscalls::LogToFile("wWinMain: Caught C++ exception: " + std::string(e.what()));
	}
	catch (...)
	{
		syscalls::LogToFileRaw("wWinMain: Caught unknown exception.");
	}
	syscalls::LogToFileRaw("wWinMain: Application shutdown.");
	return 0;
}