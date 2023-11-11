//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "base_app.h"
#include "base_app_helper.h"
#include "stat/stat.h"
#include "common/utils.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"

using namespace Microsoft::WRL;

class Win32Application
{
public:
	static int Run(BaseApp& app, int nCmdShow, UINT width, UINT height);
	static void ToggleFullscreenWindow(IDXGISwapChain* pOutput);
	static void SetCustomWindowText(LPCWSTR text);
protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	static HWND hwnd_;
};

int run_application(BaseApp& app, UINT width, UINT height)
{
	return Win32Application::Run(app, SW_SHOWDEFAULT, width, height);
}

BaseApp::BaseApp()
{
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    assets_path_ = assetsPath;
	assets_path_ += L"../../SceneEngine/";
}

void BaseApp::SetCustomWindowText(LPCWSTR text)
{
	std::wstring full_title = GetTitle();
	full_title += L": ";
	full_title += text;
	Win32Application::SetCustomWindowText(full_title.c_str());
}

HWND Win32Application::hwnd_ = nullptr;


void Win32Application::SetCustomWindowText(LPCWSTR text)
{
	SetWindowText(hwnd_, text);
}

int Win32Application::Run(BaseApp& app, int nCmdShow, UINT width, UINT height)
{
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = 0;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = app.GetTitle();
	RegisterClassEx(&windowClass);

	RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	hwnd_ = CreateWindow(
		windowClass.lpszClassName,
		app.GetTitle(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,        // We have no parent window.
		nullptr,        // We aren't using menus.
		0,
		&app);

	app.OnInit(hwnd_, width, height);
	ShowWindow(hwnd_, nCmdShow);

	MSG msg = {};
	while (true)
	{
		ImGui_ImplWin32_NewFrame();
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				return static_cast<char>(msg.wParam);
			}
		}
		app.Tick();
	}

	return -1;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	BaseApp* app = reinterpret_cast<BaseApp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;


	switch (message)
	{
	case WM_CREATE:
	{
		LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
	}
	return 0;

	case WM_KEYDOWN:
		if (app) app->OnKeyDown(static_cast<UINT8>(wParam));
		return 0;

	case WM_KEYUP:
		if (app) app->OnKeyUp(static_cast<UINT8>(wParam));
		return 0;

	case WM_DESTROY:
		app->OnDestroy();
		PostQuitMessage(0);
		return 0;

	case WM_SYSKEYDOWN:
		if ((wParam == VK_RETURN) && (lParam & (1 << 29)) && app) // Handle ALT+ENTER:
		{
			app->ToggleFullscreenWindow();
			return 0;
		}
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}
