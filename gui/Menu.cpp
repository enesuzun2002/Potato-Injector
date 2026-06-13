#include "pch.h"
#include "Menu.hpp"

#include "dependency/imgui/imgui.h"
#include "dependency/imgui/imgui_internal.h"
#include "dependency/imgui/backend/imgui_impl_dx9.h"
#include "dependency/imgui/backend/imgui_impl_win32.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void LogRaw(const char* message) {
	syscalls::LogToFile(message);
}

static void LogRawHwnd(const char* prefix, HWND hwnd) {
	char buf[128];
	sprintf_s(buf, "%s%llu", prefix, reinterpret_cast<uintptr_t>(hwnd));
	syscalls::LogToFile(buf);
}

static void LogRawInt(const char* prefix, int value) {
	char buf[128];
	sprintf_s(buf, "%s%d", prefix, value);
	syscalls::LogToFile(buf);
}

static HWND CreateAndSetupWindow(WNDCLASSEXW& wc)
{
	HWND hwnd = NULL;
	__try
	{
		LogRaw("CreateAndSetupWindow: Calling CreateWindow...");
		hwnd = ::CreateWindow(wc.lpszClassName, L"Potato Injector",
			WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
			100, 100, 200, 200, NULL, NULL, wc.hInstance, NULL);
		LogRawHwnd("CreateAndSetupWindow: CreateWindow returned hwnd=", hwnd);
		if (hwnd == NULL) return NULL;

		LogRaw("CreateAndSetupWindow: Calling SetWindowLong...");
		::SetWindowLong(hwnd, GWL_STYLE, ::GetWindowLong(hwnd, GWL_STYLE)
			& WS_CAPTION & ~WS_THICKFRAME);
		LogRaw("CreateAndSetupWindow: SetWindowLong done.");

		LogRaw("CreateAndSetupWindow: Calling SetWindowPos...");
		::SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
		LogRaw("CreateAndSetupWindow: SetWindowPos done.");
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LogRawInt("CreateAndSetupWindow: Caught SEH exception! Code: ", GetExceptionCode());
	}
	return hwnd;
}

bool Menu::initialize()
{
	syscalls::LogToFile("Menu::initialize: Started.");
	
	bool initResult = syscalls::init_syscalls();
	syscalls::LogToFile("Menu::initialize: init_syscalls returned " + std::to_string(initResult));

	// Hide main thread from debugger
	syscalls::LogToFile("Menu::initialize: Hiding main thread from debugger...");
	syscalls::hide_current_thread();
	syscalls::LogToFile("Menu::initialize: Main thread hidden.");

	// Check if debugger is attached
	syscalls::LogToFile("Menu::initialize: Querying ProcessDebugPort...");
	DWORD_PTR debugPort = 0;
	ULONG returnLength = 0;
	NTSTATUS status = syscalls::NtQueryInformationProcess(
		GetCurrentProcess(),
		7, // ProcessDebugPort
		&debugPort,
		sizeof(debugPort),
		&returnLength
	);
	syscalls::LogToFile("Menu::initialize: NtQueryInformationProcess status=" + std::to_string(status) + ", debugPort=" + std::to_string(debugPort));
	if (status >= 0 && debugPort != 0)
	{
		this->isDebugged = true;
		syscalls::LogToFile("Menu::initialize: Debugger detected via ProcessDebugPort.");
	}

	// Create application window
	syscalls::LogToFile("Menu::initialize: Registering window class...");
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, Menu::WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"WC", NULL };
	ATOM registerResult = ::RegisterClassEx(&wc);
	syscalls::LogToFile("Menu::initialize: RegisterClassEx returned " + std::to_string(registerResult));

	this->hwnd = CreateAndSetupWindow(wc);
	syscalls::LogToFile("Menu::initialize: Window helper returned hwnd=" + std::to_string(reinterpret_cast<uintptr_t>(this->hwnd)));

	// Initialize Direct3D
	syscalls::LogToFile("Menu::initialize: Creating D3D9 device...");
	if (!createD3D9Device(hwnd))
	{
		syscalls::LogToFile("Menu::initialize: Failed to create D3D9 device.");
		cleanupD3D9Device();
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}
	syscalls::LogToFile("Menu::initialize: D3D9 device created.");

	// Show the window
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	// Setup Dear ImGui context
	syscalls::LogToFile("Menu::initialize: Initializing Dear ImGui...");
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.WantSaveIniSettings = false;

	// Setup Dear ImGui style
	setupMenuStyle(true, 1);

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(this->d3dDevice);
	syscalls::LogToFile("Menu::initialize: Dear ImGui initialized.");

	this->isMenuOn = true;
	syscalls::LogToFile("Menu::initialize: Detaching detectGame and updateFiles threads...");
	std::thread(&Menu::detectGame, this).detach();
	std::thread(&Menu::updateFiles, this).detach();
	syscalls::LogToFile("Menu::initialize: Threads detached. Initialization complete.");

	return true;
}

void Menu::loop()
{
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Main loop
	while (this->isMenuOn)
	{
		MSG msg;
		while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				this->isMenuOn = false;
		}
		if (!this->isMenuOn)
			break;

		// Start the Dear ImGui frame
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		static float f = 0.0f;
		static int counter = 0;
		ImGui::SetNextWindowSize({ 200, 180 });
		ImGui::SetNextWindowPos({ 0, 0 });
		ImGui::Begin("Menu", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
		
		static int cnt = 0;
		cnt = cnt + 13 >= 2 * 255 ? 0 : cnt + 13;
		int alpha = cnt >= 255 ? cnt : 2 * 255 - cnt;

		ImGui::Text("CS2 Status: ");
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, g_injector->csgoRunning ? (this->isInjecting ? IM_COL32(255, 255, 0, alpha) : IM_COL32(0, 255, 0, 255)) : IM_COL32(255, 0, 0, 255));
		g_injector->csgoRunning ? (this->isInjecting ? ImGui::Text("[INJECTING]") : ImGui::Text("[RUNNING]")) : ImGui::Text("[OFFLINE]");
		ImGui::PopStyleColor();

		ImGui::Text("Auto: ");
		ImGui::SameLine();
		ImGui::Checkbox("Exit", &g_injector->shouldAutoExit);    //Whether to auto exit after injection
		
		ImGui::Checkbox("Custom process", &g_injector->isCustomProcess);  // Enable injection for other processes
		
		static int selectedProcess = 0;
		if (g_injector->isCustomProcess) {
			std::string procNames = "";
			auto procs = mem::getProcList();
			::std::vector<::std::wstring> nameArr;
			for (const auto& p : procs)
			{
				for (wchar_t wc : p.second)
					procNames += char(wc);
				procNames += '\0';
				nameArr.push_back(p.second);
			}
			if (!nameArr.empty()) {
				if (selectedProcess >= static_cast<int>(nameArr.size())) {
					selectedProcess = 0;
				}
				if (ImGui::Combo("##Processes", &selectedProcess, procNames.c_str())) {
					if (selectedProcess >= 0 && selectedProcess < static_cast<int>(nameArr.size())) {
						g_injector->customProcessName = nameArr[selectedProcess];
					}
				}
			} else {
				ImGui::Text("No processes found");
			}
		}

		static int selectedDLL = 0;
		this->mtx.lock();
		std::vector<std::string> paths = this->filePaths;
		this->mtx.unlock();
		std::string comboPaths = "";
		for (const auto& path : paths)
		{
			comboPaths += path.substr(path.find_last_of('\\') + 1) + '\0';
		}
		
		if (!paths.empty()) {
			if (selectedDLL >= static_cast<int>(paths.size())) {
				selectedDLL = 0;
			}
			ImGui::Combo("DLLS", &selectedDLL, comboPaths.c_str());
		} else {
			ImGui::Text("No DLLs found in ./dlls");
		}
		
		if (ImGui::Button("Inject"))
		{
			if (!this->isInjecting)
			{
				bool valid = true;
				if (g_injector->isCustomProcess)
				{
					auto pid = mem::getProcID(g_injector->customProcessName);
					if (pid == NULL) {
						MessageBox(hwnd, L"Custom process not found...", nullptr, 0);
						valid = false;
					}
				}
				if (valid && !paths.empty() && selectedDLL >= 0 && selectedDLL < static_cast<int>(paths.size()))
					std::thread(&Injector::inject, g_injector.get(), paths[selectedDLL]).detach();
			}
		}

		if (this->isInjecting)
		{
			static int counter = 0;
			std::string s = "Injecting DLL";
			for (int i = 0; i < counter / 10; i++) s += ".";
			counter = counter >= 30 ? 0 : counter + 1;
			ImGui::Text(s.c_str());
		}

		ImGui::End();

		// Rendering
		ImGui::EndFrame();

		this->d3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		this->d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		this->d3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * clear_color.w * 255.0f), (int)(clear_color.y * clear_color.w * 255.0f), (int)(clear_color.z * clear_color.w * 255.0f), (int)(clear_color.w * 255.0f));
		this->d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
		if (this->d3dDevice->BeginScene() >= 0)
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			this->d3dDevice->EndScene();
		}
		HRESULT result = this->d3dDevice->Present(NULL, NULL, NULL, NULL);

	}
}

bool Menu::createD3D9Device(HWND hWnd)
{
	if ((this->pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL) return false;

	ZeroMemory(&this->d3dpp, sizeof(this->d3dpp));
	this->d3dpp.Windowed = TRUE;
	this->d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	this->d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; 
	this->d3dpp.EnableAutoDepthStencil = TRUE;
	this->d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	this->d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;       
	this->d3dpp.hDeviceWindow = hWnd;
	auto result = this->pD3D->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		hwnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&this->d3dpp, &this->d3dDevice);
	if (result != S_OK) return false;

	return true;
}

void Menu::cleanupD3D9Device()
{
	if (this->d3dDevice != nullptr)
	{
		this->d3dDevice->Release();
		this->d3dDevice = nullptr;
	}

	if (this->pD3D != nullptr)
	{
		this->pD3D->Release();
		this->pD3D = nullptr;
	}
}

LRESULT __stdcall Menu::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DESTROY) {
		syscalls::LogToFile("WndProc: WM_DESTROY received.");
	}
	if (ImGui::GetCurrentContext() != nullptr)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return true;
	}

	switch (msg)
	{
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

void Menu::setupMenuStyle(bool isDarkTheme, float alpha)
{
	ImGuiStyle& style = ImGui::GetStyle();

	// light style from Pacôme Danhiez (user itamago) https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
	style.Alpha = 1.0f;
	style.FrameRounding = 3.0f;
	style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);

	if (isDarkTheme)
	{
		for (int i = 0; i <= ImGuiCol_COUNT; i++)
		{
			ImVec4& col = style.Colors[i];
			float H, S, V;
			ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, H, S, V);

			if (S < 0.1f)
			{
				V = 1.0f - V;
			}
			ImGui::ColorConvertHSVtoRGB(H, S, V, col.x, col.y, col.z);
			if (col.w < 1.00f)
			{
				col.w *= alpha;
			}
		}
	}
	else
	{
		for (int i = 0; i <= ImGuiCol_COUNT; i++)
		{
			ImVec4& col = style.Colors[i];
			if (col.w < 1.00f)
			{
				col.x *= alpha;
				col.y *= alpha;
				col.z *= alpha;
				col.w *= alpha;
				col.w *= alpha;
			}
		}
	}
}



void Menu::detectGameInner()
{
	while (this->isMenuOn)
	{
		DWORD pID = mem::getProcID(vars::get_game_process_name());
		g_injector->csgoRunning = (pID != NULL);
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void Menu::detectGame() {
	__try {
		syscalls::hide_current_thread();
		this->detectGameInner();
	}
	__except (syscalls::LogSehException("detectGame", GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
	}
}

void Menu::updateFilesInner()
{
	std::wstring dllDir = vars::get_dll_dir_path();
	if (!std::filesystem::is_directory(dllDir) || !std::filesystem::exists(dllDir)) {
		std::filesystem::create_directory(dllDir);
	}
	
	while (this->isMenuOn)
	{
		this->mtx.lock();
		this->filePaths.clear();
		for (const auto& file : std::filesystem::directory_iterator(dllDir))
		{
			if (!std::filesystem::is_directory(file) && (file.path().string().substr(file.path().string().find_last_of(".") + 1) == "dll"))
				this->filePaths.push_back(file.path().string());
		}
		this->mtx.unlock();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void Menu::updateFiles() {
	__try {
		syscalls::hide_current_thread();
		this->updateFilesInner();
	}
	__except (syscalls::LogSehException("updateFiles", GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
	}
}
