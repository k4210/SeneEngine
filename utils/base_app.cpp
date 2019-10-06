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

using namespace Microsoft::WRL;

class Win32Application
{
public:
	static int Run(BaseApp& app, HINSTANCE hInstance, int nCmdShow, UINT width, UINT height);
	static void ToggleFullscreenWindow(IDXGISwapChain* pOutput);
	static void SetCustomWindowText(LPCWSTR text);
protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	static HWND hwnd_;
	static bool fullscreen_;
};

int run_application(BaseApp& app, HINSTANCE hInstance, int nCmdShow, UINT width, UINT height)
{
	return Win32Application::Run(app, hInstance, nCmdShow, width, height);
}

BaseApp::BaseApp()
{
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    assets_path_ = assetsPath;
	assets_path_ += L"../../SceneEngine/";
}

std::wstring BaseApp::GetAssetFullPath(LPCWSTR asset_name) const
{
    return assets_path_ + asset_name;
}

void BaseApp::SetCustomWindowText(LPCWSTR text)
{
	std::wstring full_title = GetTitle();
	full_title += L": ";
	full_title += text;
	Win32Application::SetCustomWindowText(full_title.c_str());
}

HWND Win32Application::hwnd_ = nullptr;
bool Win32Application::fullscreen_ = false;


void Win32Application::SetCustomWindowText(LPCWSTR text)
{
	SetWindowText(hwnd_, text);
}

int Win32Application::Run(BaseApp& app, HINSTANCE hInstance, int nCmdShow, UINT width, UINT height)
{
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
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
		hInstance,
		&app);

	app.OnInit(hwnd_, width, height);
	ShowWindow(hwnd_, nCmdShow);
	MSG msg = {};
	while (true)
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				app.OnDestroy();
				return static_cast<char>(msg.wParam);
			}
		}
		app.Tick();
	}

	return -1;
}

LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	BaseApp* app = reinterpret_cast<BaseApp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

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

	case WM_PAINT:
		//if (app) app->Tick();
		return 0; //Ignore we redraw all whenever possible

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SYSKEYDOWN:
		if ((wParam == VK_RETURN) && (lParam & (1 << 29))
			&& app && app->IsTearingSupported()) // Handle ALT+ENTER:
		{
			ToggleFullscreenWindow(app->GetSwapChain());
			return 0;
		}
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void Win32Application::ToggleFullscreenWindow(IDXGISwapChain* swap_chain)
{
	const UINT window_style = WS_OVERLAPPEDWINDOW;
	static RECT window_rect = { 0, 0, 1280, 720 };
	if (fullscreen_)
	{
		// Restore the window's attributes and size.
		SetWindowLong(hwnd_, GWL_STYLE, window_style);

		SetWindowPos(
			hwnd_,
			HWND_NOTOPMOST,
			window_rect.left,
			window_rect.top,
			window_rect.right - window_rect.left,
			window_rect.bottom - window_rect.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);

		ShowWindow(hwnd_, SW_NORMAL);
	}
	else
	{
		// Save the old window rect so we can restore it when exiting fullscreen mode.
		GetWindowRect(hwnd_, &window_rect);

		// Make the window borderless so that the client area can fill the screen.
		SetWindowLong(hwnd_, GWL_STYLE, window_style & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

		RECT fullscreen_rect;
		try
		{
			if (swap_chain)
			{
				// Get the settings of the display on which the app's window is currently displayed
				ComPtr<IDXGIOutput> output;
				ThrowIfFailed(swap_chain->GetContainingOutput(&output));
				DXGI_OUTPUT_DESC desc;
				ThrowIfFailed(output->GetDesc(&desc));
				fullscreen_rect = desc.DesktopCoordinates;
			}
			else
			{
				// Fallback to EnumDisplaySettings implementation
				throw HrException(S_FALSE);
			}
		}
		catch (HrException& e)
		{
			UNREFERENCED_PARAMETER(e);

			// Get the settings of the primary display
			DEVMODE devMode = {};
			devMode.dmSize = sizeof(DEVMODE);
			EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);

			fullscreen_rect = {
				devMode.dmPosition.x,
				devMode.dmPosition.y,
				devMode.dmPosition.x + static_cast<LONG>(devMode.dmPelsWidth),
				devMode.dmPosition.y + static_cast<LONG>(devMode.dmPelsHeight)
			};
		}

		SetWindowPos(
			hwnd_,
			HWND_TOPMOST,
			fullscreen_rect.left,
			fullscreen_rect.top,
			fullscreen_rect.right,
			fullscreen_rect.bottom,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);
		ShowWindow(hwnd_, SW_MAXIMIZE);
	}

	fullscreen_ = !fullscreen_;
}
