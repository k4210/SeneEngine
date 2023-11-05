#pragma once

#include "common_msg.h"

class BaseApp
{
public:
	BaseApp();
	virtual ~BaseApp() = default;

    virtual void OnInit(HWND hWnd, UINT width, UINT height) = 0;
	virtual void Tick() = 0;
    virtual void OnDestroy() = 0;
	virtual void ToggleFullscreenWindow() = 0;
	virtual void OnKeyDown(UINT8 /*key*/)	{}
	virtual void OnKeyUp(UINT8 /*key*/)		{}
	virtual void ReceiveMsgToBroadcast(CommonMsg::Message) = 0; // Thread Safe

	const WCHAR* GetTitle() const { return L"SceneEngine"; }
	void SetCustomWindowText(LPCWSTR text);

protected:    
    std::wstring assets_path_;

};
