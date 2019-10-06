#include "stdafx.h"
#include "render_thread.h"
#include "utils/base_app_helper.h"

struct RTPimpl
{
	RendererCommon& render_common_;
	
	CD3DX12_VIEWPORT viewport_;
	CD3DX12_RECT scissor_rect_;
	ComPtr<ID3D12RootSignature> root_signature_;
	ComPtr<ID3D12GraphicsCommandList> command_list_;
	ComPtr<ID3D12Resource> back_buffer_rt_[RendererCommon::kFrameCount];
	ComPtr<ID3D12CommandAllocator> command_allocators_[RendererCommon::kFrameCount];
	ComPtr<ID3D12DescriptorHeap> rtv_heap_;
	ComPtr<ID3D12PipelineState> pipeline_state_;
	uint32_t rtv_descriptor_size_ = 0;
	uint32_t buffer_idx_ = 0;

	RTPimpl(RendererCommon& render_common) : render_common_(render_common)
		, viewport_(0.0f, 0.0f, static_cast<float>(render_common_.width), static_cast<float>(render_common_.height))
		, scissor_rect_(0, 0, render_common_.width, render_common_.height) 
	{
		buffer_idx_ = render_common.swap_chain_->GetCurrentBackBufferIndex();
		{
			D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
			rtv_heap_desc.NumDescriptors = RendererCommon::kFrameCount;
			rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(render_common.device_->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap_)));
			rtv_descriptor_size_ = render_common.device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap_->GetCPUDescriptorHandleForHeapStart());
			for (UINT n = 0; n < RendererCommon::kFrameCount; n++)
			{
				ThrowIfFailed(render_common.swap_chain_->GetBuffer(n, IID_PPV_ARGS(&back_buffer_rt_[n])));
				render_common.device_->CreateRenderTargetView(back_buffer_rt_[n].Get(), nullptr, rtv_handle);
				rtv_handle.Offset(1, rtv_descriptor_size_);
				ThrowIfFailed(render_common.device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators_[n])));
			}
		}

		{
			CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc;
			root_signature_desc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			ThrowIfFailed(D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
			ThrowIfFailed(render_common.device_->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_)));
		}

		{
			ComPtr<ID3DBlob> vertexShader;
			ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif

			ThrowIfFailed(D3DCompileFromFile((render_common.base_shader_path + L"shaders/shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
			ThrowIfFailed(D3DCompileFromFile((render_common.base_shader_path + L"shaders/shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

			D3D12_INPUT_ELEMENT_DESC input_element_descs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
			pso_desc.InputLayout = { input_element_descs, _countof(input_element_descs) };
			pso_desc.pRootSignature = root_signature_.Get();
			pso_desc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
			pso_desc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
			pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			pso_desc.DepthStencilState.DepthEnable = FALSE;
			pso_desc.DepthStencilState.StencilEnable = FALSE;
			pso_desc.SampleMask = UINT_MAX;
			pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pso_desc.NumRenderTargets = 1;
			pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			pso_desc.SampleDesc.Count = 1;
			ThrowIfFailed(render_common.device_->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state_)));
		}
		ThrowIfFailed(render_common.device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators_[buffer_idx_].Get(), pipeline_state_.Get(), IID_PPV_ARGS(&command_list_)));
		ThrowIfFailed(command_list_->Close());
	}

	~RTPimpl() {}

	void SetFrameIndex(uint32_t frame_idx) 
	{ 
		assert(frame_idx < RendererCommon::kFrameCount);
		buffer_idx_ = frame_idx;
	}

	void SetCamera(const RT_MSG_UpdateCamera&) {}
	void SetMeshBuff(const RT_MSG_MeshBuffer&) {}
	void SetStaticInstances(const RT_MSG_StaticInstances&) {}
	void SetStaticNodes(const RT_MSG_StaticNodes&) {}
	
	void PrepareCommandList() 
	{
		ThrowIfFailed(command_allocators_[buffer_idx_]->Reset());
		ThrowIfFailed(command_list_->Reset(command_allocators_[buffer_idx_].Get(), pipeline_state_.Get()));

		auto barrier_rt = CD3DX12_RESOURCE_BARRIER::Transition(back_buffer_rt_[buffer_idx_].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		command_list_->ResourceBarrier(1, &barrier_rt);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtv_heap_->GetCPUDescriptorHandleForHeapStart(), buffer_idx_, rtv_descriptor_size_);
		command_list_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		// Record commands.
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		command_list_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		auto barrier_present = CD3DX12_RESOURCE_BARRIER::Transition(back_buffer_rt_[buffer_idx_].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		command_list_->ResourceBarrier(1, &barrier_present);

		ThrowIfFailed(command_list_->Close());
	}

	void ExecuteAndPresent() 
	{
		ID3D12CommandList* ppCommandLists[] = { command_list_.Get() };
		render_common_.direct_command_queue_->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		ThrowIfFailed(render_common_.swap_chain_->Present(1, 0));
	}
};

void RenderThread::Create()
{
	assert(!pimpl_);
	fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event_ == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	{
		ThrowIfFailed(renderer_common_.direct_command_queue_->Signal(renderer_common_.direct_queue_fence_.Get(), tick_index_));
		ThrowIfFailed(renderer_common_.direct_queue_fence_->SetEventOnCompletion(tick_index_, fence_event_));
		WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);
		tick_index_++;
	}
	pimpl_ = new RTPimpl(renderer_common_);
}

void RenderThread::Destroy()
{
	assert(!thread_.joinable());
	/*
	if (tick_index_ > 1)
	{
		const FenceValue needed_frame = tick_index_ - 1;
		const FenceValue current_fence_val = renderer_common_.direct_queue_fence_->GetCompletedValue();
		if (current_fence_val < needed_frame)
		{
			ThrowIfFailed(renderer_common_.direct_queue_fence_->SetEventOnCompletion(needed_frame, fence_event_));
			WaitForSingleObject(fence_event_, INFINITE);
		}
	}
	*/
	CloseHandle(fence_event_);
	assert(pimpl_);

	exit(0);
	
	//delete pimpl_;
	//pimpl_ = nullptr;
}

void RenderThread::Tick()
{
	assert(pimpl_);
	if(tick_index_ > 1)
	{
		const FenceValue needed_frame = tick_index_ - 2;
		const FenceValue current_fence_val = renderer_common_.direct_queue_fence_->GetCompletedValue();
		if (current_fence_val < needed_frame)
		{
			ThrowIfFailed(renderer_common_.direct_queue_fence_->SetEventOnCompletion(needed_frame, fence_event_));
			WaitForSingleObject(fence_event_, INFINITE);
		}
		assert(pimpl_->buffer_idx_ != renderer_common_.swap_chain_->GetCurrentBackBufferIndex());
	}

	pimpl_->SetFrameIndex(renderer_common_.swap_chain_->GetCurrentBackBufferIndex());

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

	pimpl_->PrepareCommandList();
	pimpl_->ExecuteAndPresent();

	ThrowIfFailed(renderer_common_.direct_command_queue_->Signal(renderer_common_.direct_queue_fence_.Get(), current_frame_index));
}
