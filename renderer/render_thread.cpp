#include "stdafx.h"
#include "render_thread.h"

struct RTPimpl
{
	RendererCommon& render_common_;
	//
	CD3DX12_VIEWPORT viewport_;
	CD3DX12_RECT scissor_rect_;
	ComPtr<ID3D12RootSignature> root_signature_;
	ComPtr<ID3D12PipelineState> pipeline_state_;
	ComPtr<ID3D12GraphicsCommandList> command_list_;
	ComPtr<ID3D12Resource> back_buffer_rt[RendererCommon::kFrameCount];
	ComPtr<ID3D12CommandAllocator> command_allocators[RendererCommon::kFrameCount];
	ComPtr<ID3D12DescriptorHeap> rtv_heap;
	//


	//
	HANDLE fence_event_ = nullptr;
	UINT64 fence_values_[RendererCommon::kFrameCount];
	uint8_t buffer_idx_ = 0;

	RTPimpl(RendererCommon& render_common) : render_common_(render_common)
		, viewport_(0.0f, 0.0f, static_cast<float>(render_common_.width), static_cast<float>(render_common_.height))
		, scissor_rect_(0, 0, render_common_.width, render_common_.height)
	{}

	~RTPimpl() {}

	void SetFrameIndex(FenceValue frame_idx) 
	{ 
		buffer_idx_ = static_cast<uint8_t>(frame_idx % RendererCommon::kFrameCount);
	}
	void SetCamera(const RT_MSG_UpdateCamera&) {}
	void SetMeshBuff(const RT_MSG_MeshBuffer&) {}
	void SetStaticInstances(const RT_MSG_StaticInstances&) {}
	void SetStaticNodes(const RT_MSG_StaticNodes&) {}
	
	void WaitForAllocator() {}
	void PrepareCommandList() {}
	void ExecuteAndPresent() {}
};

void RenderThread::Create()
{
	assert(!pimpl_);
	pimpl_ = new RTPimpl(renderer_common_);
}

void RenderThread::Destroy()
{
	assert(!thread_.joinable());
	assert(pimpl_);
	delete pimpl_;
	pimpl_ = nullptr;
}

void RenderThread::Tick()
{
	assert(pimpl_);

	//Wait for gpu to finish -2 frame

	pimpl_->SetFrameIndex(tick_index_);

	auto hangle_msg = [&]()
	{
		RT_MSG msg;
		while (message_queue_.try_dequeue(msg))
		{
			const ERT_MSG msg_type = static_cast<ERT_MSG>(msg.index());
			switch (msg_type)
			{
			case ERT_MSG::UpdateCamera:		pimpl_->SetCamera(std::get<RT_MSG_UpdateCamera>(msg));				break;
			case ERT_MSG::MeshBuffer:		pimpl_->SetMeshBuff(std::get<RT_MSG_MeshBuffer>(msg));				break;
			case ERT_MSG::StaticInstances:	pimpl_->SetStaticInstances(std::get<RT_MSG_StaticInstances>(msg));	break;
			default: assert(false); break;
			}
		}
	};
	hangle_msg();
	const FenceValue current_frame_index = tick_index_.fetch_add(1);
	hangle_msg();

	pimpl_->WaitForAllocator();
	pimpl_->PrepareCommandList();
	pimpl_->ExecuteAndPresent();
}
