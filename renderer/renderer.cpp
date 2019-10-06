#include "stdafx.h"
#include "renderer.h"
#include "../utils/base_app_helper.h"

namespace {
	static void GetHardwareAdapter(IDXGIFactory2 * pFactory, IDXGIAdapter1 * *ppAdapter)
	{
		ComPtr<IDXGIAdapter1> adapter;
		*ppAdapter = nullptr;

		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				// If you want a software adapter, pass in "/warp" on the command line.
				continue;
			}

			// Check to see if the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}

		*ppAdapter = adapter.Detach();
	}
}

void Renderer::Init(HWND hwnd, UINT width, UINT height, std::wstring base_shader_path)
{
	common_.base_shader_path = base_shader_path;

	common_.width = width;
	common_.height = height;

	UINT dxgi_flags = 0;
#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			dxgi_flags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif
	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgi_flags, IID_PPV_ARGS(&factory)));

	{
		ComPtr<IDXGIFactory6> factory6;
		if (SUCCEEDED(factory.As(&factory6)))
		{
			BOOL allow_tearing = FALSE;
			HRESULT hr = factory6->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
			common_.tearing_supported_ = SUCCEEDED(hr) && allow_tearing;
		}
	}

	{
		ComPtr<IDXGIAdapter1> adapter;
		GetHardwareAdapter(factory.Get(), &adapter);
		ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&common_.device_)));
	}

	{
		D3D12_COMMAND_QUEUE_DESC direct_queue_desc = {};
		direct_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		direct_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ThrowIfFailed(common_.device_->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&common_.direct_command_queue_)));
	}

	{
		DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
		swap_chain_desc.BufferCount = common_.kFrameCount;
		swap_chain_desc.Width = width;
		swap_chain_desc.Height = height;
		swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_desc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swap_chain;
		ThrowIfFailed(factory->CreateSwapChainForHwnd(common_.direct_command_queue_.Get(), hwnd, &swap_chain_desc, nullptr, nullptr, &swap_chain));
		ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
		ThrowIfFailed(swap_chain.As(&common_.swap_chain_));
	}

	ThrowIfFailed(common_.device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&common_.direct_queue_fence_ )));
	ThrowIfFailed(common_.device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&common_.update_queue_fence_ )));
	ThrowIfFailed(common_.device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&common_.loading_queue_fence_)));

	render_thread_.Create();
	moveable_thread_.Create();
	scene_thread_.Create();
}

void Renderer::Destroy()
{
	if (!common_.tearing_supported_)
	{
		ThrowIfFailed(common_.swap_chain_->SetFullscreenState(FALSE, nullptr));
	}

	render_thread_.Destroy();
	moveable_thread_.Destroy();
	scene_thread_.Destroy();
}

void Renderer::Start()
{
	//scene_thread_.Start();
	//moveable_thread_.Start();
	render_thread_.Start();
}

void Renderer::Stop()
{
	render_thread_.RequestStop();
	moveable_thread_.RequestStop();
	scene_thread_.RequestStop();

	render_thread_.WaitForStop();
	moveable_thread_.WaitForStop();
	scene_thread_.WaitForStop();
}

void Renderer::SetCamera(XMFLOAT3 position, XMFLOAT3 direction)
{
	render_thread_.ReceiveMsg(RT_MSG(RT_MSG_UpdateCamera{ position, direction }));
}

Renderer::AddedMesh Renderer::AddMesh(std::shared_ptr<RawMeshData> mesh)
{
	const uint32_t idx = scene_thread_.PopMeshFreeSpaceThreadSafe();
	scene_thread_.ReceiveMsg(ST_MSG(ST_MSG_AddMesh{ idx, mesh }));

	return { idx, common_.GetLastUnfinishedBatch() };
}

void Renderer::RemoveMesh(uint32_t index)
{
	scene_thread_.ReceiveMsg(ST_MSG(ST_MSG_RemoveMesh{ index }));
}

ID Renderer::AddStaticSceneNode(BoundingSphere bounding_sphere_world_space
	, std::vector<InstanceData>&& instances)
{
	const ID id = ID::New();
	scene_thread_.ReceiveMsg(ST_MSG(ST_MSG_AddStaticNode{ id
		, bounding_sphere_world_space, std::move(instances) }));
	return id;
}

void Renderer::RemoveStaticSceneNode(ID id)
{
	scene_thread_.ReceiveMsg(ST_MSG(ST_MSG_RemoveStaticNode{ id }));
}

ID Renderer::AddMoveableInstance(InstanceData instance, uint32_t needed_batch)
{
	const ID id = ID::New();
	moveable_thread_.ReceiveMsg(MT_MSG(MT_MSG_Add{ id, instance, needed_batch }));
	return id;
}

void Renderer::UpdateMoveableInstance(ID id
	, XMFLOAT4X4 model_to_world
	, BoundingSphere bounding_sphere_world_space)
{
	moveable_thread_.ReceiveMsg(MT_MSG(MT_MSG_Update{ id
		, model_to_world, bounding_sphere_world_space }));
}

void Renderer::RemoveMoveableInstance(ID instance_id)
{
	moveable_thread_.ReceiveMsg(MT_MSG(MT_MSG_Remove{ instance_id }));
}

FenceValue RendererCommon::GetLastUnfinishedFrameRT() const
{
	assert(render_thread_);
	return render_thread_->LastUnfinishedTick();
}

FenceValue RendererCommon::GetLastUnfinishedFrameMT() const
{
	assert(moveable_thread_);
	return moveable_thread_->LastUnfinishedTick();
}

FenceValue RendererCommon::GetLastUnfinishedBatch() const
{
	assert(scene_thread_);
	return scene_thread_->LastUnfinishedTick();
}

FenceValue RendererCommon::GetLastLoadedBatch() const
{
	assert(loading_queue_fence_);
	return loading_queue_fence_->GetCompletedValue();
}