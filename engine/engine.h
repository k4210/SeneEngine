#pragma once

#include "../utils/base_app.h"
#include "../renderer/renderer.h"

struct Mesh
{
	std::string name;
	std::shared_ptr<RawMeshData> data;
	uint32_t index = RendererCommon::kInvalid;
	FenceValue batch = 0;
};

class SceneEngine : public BaseApp
{
	Renderer renderer_;
	std::vector<Mesh> meshes_;


public:
	SceneEngine();
	virtual ~SceneEngine();

	uint32_t LoadMeshesFromFile(char* path);

protected:
	void OnInit(HWND hWnd, UINT width, UINT height) override;
	void Tick() override {}
	void OnDestroy() override;
	virtual void OnKeyDown(UINT8 /*key*/) override {}
	virtual void OnKeyUp(UINT8 /*key*/) override {}
	virtual bool IsTearingSupported() const override { return renderer_.GetRendererCommon().tearing_supported_; }
	virtual IDXGISwapChain3* GetSwapChain() const { return renderer_.GetRendererCommon().swap_chain_.Get(); }
};

