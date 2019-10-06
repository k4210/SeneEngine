#pragma once

class BaseApp
{
public:
	BaseApp();
	virtual ~BaseApp() = default;

    virtual void OnInit(HWND hWnd, UINT width, UINT height) = 0;
	virtual void Tick() = 0;
    virtual void OnDestroy() = 0;

    virtual void OnKeyDown(UINT8 /*key*/) = 0;
    virtual void OnKeyUp(UINT8 /*key*/)   = 0;

	virtual bool IsTearingSupported()		const = 0;
	virtual IDXGISwapChain3* GetSwapChain() const = 0;

	const WCHAR* GetTitle() const { return L"SceneEngine"; }
	std::wstring GetAssetFullPath(LPCWSTR asset_name) const;
	void SetCustomWindowText(LPCWSTR text);

protected:    
    std::wstring assets_path_;
};
