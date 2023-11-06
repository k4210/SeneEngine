#pragma once

#include "../renderer_interface.h"

using Microsoft::WRL::ComPtr;
using namespace IRenderer;

struct SyncPerFrame
{
	ComPtr<ID3D12Fence> fence;
	HANDLE fence_event = 0;
	uint64_t fence_value = 0;
	bool need_to_wait = false;

	void Initialize(const ComPtr<ID3D12Device>& device, uint32 index)
	{
		assert(!need_to_wait);
		ThrowIfFailed(device->CreateFence(fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
		SetNameIndexed(fence.Get(), L"fence", index);
		fence_value++;

		// Create an event handle to use for frame synchronization.
		fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fence_event == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
	}

	void StartSync(const ComPtr<ID3D12CommandQueue>& direct_command_queue)
	{
		assert(!need_to_wait);
		ThrowIfFailed(direct_command_queue->Signal(fence.Get(), fence_value));

		need_to_wait = fence->GetCompletedValue() < fence_value;
		if (need_to_wait)
		{
			ThrowIfFailed(fence->SetEventOnCompletion(fence_value, fence_event));
		}
		else
		{
			fence_value++;
		}
	}

	void Wait()
	{
		if (need_to_wait)
		{
			WaitForSingleObjectEx(fence_event, INFINITE, FALSE);
			fence_value++;
			need_to_wait = false;
		}
	}

	void Close()
	{
		assert(!need_to_wait);
		CloseHandle(fence_event);
		fence.Reset();
	}
};

class BaseRenderer : public BaseSystemImpl<RT_MSG>
{
protected:
	RendererCommon common_;

	ComPtr<IDXGISwapChain3> swap_chain_;
	ComPtr<ID3D12CommandQueue> direct_command_queue_;
	ComPtr<ID3D12CommandAllocator> command_allocators[Const::kFrameCount];
	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data_ = { D3D_ROOT_SIGNATURE_VERSION_1 };

	uint32_t width_ = 1280;
	uint32_t height_ = 720;
	bool fullscreen_ = false;
	CD3DX12_VIEWPORT viewport_;
	CD3DX12_RECT scissor_rect_;

	ComPtr<ID3D12Resource> render_targets_[Const::kFrameCount];
	ComPtr<ID3D12Resource> depth_stencil_;
	ComPtr<ID3D12DescriptorHeap> rtv_heap_;
	ComPtr<ID3D12DescriptorHeap> dsv_heap_;
	uint32_t rtv_descriptor_size_ = 0;
	uint32_t dsv_descriptor_size_ = 0;

	BOOL tearing_supported_ = false;
	HWND hwnd_ = nullptr;
	RECT window_rect_ = { 0, 0, 1280, 720 };

	std::array<SyncPerFrame, Const::kFrameCount> sync_;
	uint32_t frame_index_ = 0;

	SyncPerFrame& GetSync()
	{
		return sync_[frame_index_];
	}

public:
	BaseRenderer(HWND hWnd, uint32_t width, uint32_t height);
	virtual ~BaseRenderer();
	static const RendererCommon& GetCommon();
	static void StaticEnqueueMsg(RT_MSG&& msg);

protected:
	static void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter);

	void SetupFullscreen(const std::optional<bool> forced_mode);

	void WaitForGpu();

	//bool StartMoveToNextFrame();
	//void EndMoveToNextFrame(bool need_to_wait);
	//SyncGPU PrevFrameSync();

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