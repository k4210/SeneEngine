#pragma once

#include "base_app.h"
#include "../renderer_interface.h"

using Microsoft::WRL::ComPtr;
using namespace IRenderer;

struct SyncPerFrame
{
	ComPtr<ID3D12Fence> fence;
	HANDLE fence_event = 0;
	uint64_t fence_value = 0;
	bool need_to_wait = false;

	void Initialize(const ComPtr<ID3D12Device>& device, uint32 index);

	void StartSync(const ComPtr<ID3D12CommandQueue>& direct_command_queue);

	void Wait();

	void Close();
};

class BaseRenderer : public BaseSystemImpl<RT_MSG>
{
protected:
	RendererCommon common_;

	ComPtr<IDXGISwapChain3> swap_chain_;
	ComPtr<ID3D12CommandQueue> direct_command_queue_;
	ComPtr<ID3D12CommandAllocator> command_allocators[Const::kFrameCount];
	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data_ = { D3D_ROOT_SIGNATURE_VERSION_1 };

	HWND hwnd_ = nullptr;
	uint32 width_ = 1280;
	uint32 height_ = 720;
	CD3DX12_VIEWPORT viewport_;
	CD3DX12_RECT scissor_rect_;

	ComPtr<ID3D12Resource> render_targets_[Const::kFrameCount];
	ComPtr<ID3D12Resource> depth_stencil_;
	ComPtr<ID3D12DescriptorHeap> rtv_heap_;
	ComPtr<ID3D12DescriptorHeap> dsv_heap_;
	uint32 rtv_descriptor_size_ = 0;
	uint32 dsv_descriptor_size_ = 0;

	RECT window_rect_ = { 0, 0, 1280, 720 };

	std::array<SyncPerFrame, Const::kFrameCount> sync_;
	uint32 frame_index_ = 0;

	uint64 frame_counter = 0;
	Utils::TimeType time_;
	BaseApp& app_;

	BOOL tearing_supported_ = false;
	bool fullscreen_ = false;

	SyncPerFrame& GetSync()
	{
		return sync_[frame_index_];
	}

public:
	BaseRenderer(HWND hWnd, uint32_t width, uint32_t height, BaseApp& app);
	virtual ~BaseRenderer();
	static const RendererCommon& GetCommon();
	static void StaticEnqueueMsg(RT_MSG&& msg);

protected:
	static void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter);

	void SetupFullscreen(const std::optional<bool> forced_mode);

	void WaitForGpu();

	void Tick() override final;

	virtual void Draw() = 0;

	ID3D12Resource* GetRTResource() const
	{
		return render_targets_[frame_index_].Get();
	}

	void PrepareCommonDraw(ID3D12GraphicsCommandList* command_list);

	ID3D12CommandAllocator* GetActiveAllocator();

	D3D_ROOT_SIGNATURE_VERSION GetRootSignatureVersion() const;

	void Execute(ID3D12CommandList* comand_list);

	void Execute(uint32_t num, ID3D12CommandList* const* comand_lists);

	void Present();

	uint32_t GetFrameIndex() const { return frame_index_; }

protected:
	void ThreadInitialize() override;

	void ThreadCleanUp() override;

	void CustomOpen() override;

	void CustomClose() override;

	void Destroy() override;
};