#pragma once

#include "../utils/base_app.h"
#include "../renderer/renderer.h"

class SceneEngine : public BaseApp
{
	Renderer renderer;

public:
	SceneEngine();
	virtual ~SceneEngine();

protected:
	void OnInit(HWND hWnd, UINT width, UINT height) override;
	void Tick() override {}
	void OnDestroy() override;
	virtual void OnKeyDown(UINT8 /*key*/) override {}
	virtual void OnKeyUp(UINT8 /*key*/) override {}
	virtual bool IsTearingSupported() const override { return renderer.GetRendererCommon().tearing_supported_; }
	virtual IDXGISwapChain3* GetSwapChain() const { return renderer.GetRendererCommon().swap_chain_.Get(); }
};

