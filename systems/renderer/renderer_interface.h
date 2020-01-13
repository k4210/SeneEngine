#pragma once

#include "utils/base_system.h"
#include "utils/graphics/gpu_containers.h"
#include "utils/math/Transform.h"
#include "primitives/mesh_data.h"

namespace Const
{
	constexpr uint32_t kFrameCount = 2;
	constexpr uint32_t kLodNum = 3;
	constexpr uint32_t kMeshCapacity = 4096;
	constexpr uint32_t kStaticNodesCapacity = 4096;
	constexpr uint32_t kMaxInstancesPerNode = 16;
	constexpr uint32_t kStaticInstancesCapacity = kStaticNodesCapacity * kMaxInstancesPerNode;
	constexpr uint32_t kMoveableInstancesCapacity = 4 * 4096;
	constexpr uint32_t kBuffHeapSize = 4 * kMeshCapacity;
};

namespace IRenderer
{
	using Microsoft::WRL::ComPtr;
	using SyncGPU = std::pair<ComPtr<ID3D12Fence>, uint64_t>;

	struct RT_MSG_UpdateCamera { DirectX::XMFLOAT3 position; DirectX::XMFLOAT3 direction; };
	struct RT_MSG_ToogleFullScreen { std::optional<bool> forced_mode; };
	
	struct RT_MSG_MeshBuffer { DescriptorHeapElementRef meshes_buff; }; // update fragment
	struct RT_MSG_StaticBuffers 
	{ 
		DescriptorHeapElementRef static_nodes;
		DescriptorHeapElementRef static_instances;
		uint32_t num_nodes = 0; 
		std::promise<SyncGPU> promise_previous_nodes_not_used;
	};

	struct RT_MSG_RegisterMeshes { std::vector<std::shared_ptr<Mesh>> to_register; };

	using RT_MSG = std::variant<
		RT_MSG_UpdateCamera,
		RT_MSG_MeshBuffer,
		RT_MSG_StaticBuffers,
		RT_MSG_ToogleFullScreen,
		RT_MSG_RegisterMeshes>;

	struct RendererCommon
	{
		ComPtr<IDXGISwapChain3> swap_chain;
		ComPtr<ID3D12Device> device;
		ComPtr<ID3D12CommandQueue> direct_command_queue;

		BOOL tearing_supported = false;
		uint32_t width = 1280;
		uint32_t height = 720;
		HWND hwnd = nullptr;

		D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = { D3D_ROOT_SIGNATURE_VERSION_1 };

		RECT window_rect = { 0, 0, 1280, 720 };
		bool fullscreen = false;

		uint32_t frame_index = 0;
		HANDLE fence_event = 0;
		ComPtr<ID3D12Fence> fence;
		uint64_t fence_values[Const::kFrameCount] = { 0 };

		CD3DX12_VIEWPORT viewport;
		CD3DX12_RECT scissor_rect;
		ComPtr<ID3D12Resource> render_targets[Const::kFrameCount];
		ComPtr<ID3D12Resource> depth_stencil;

		ComPtr<ID3D12CommandAllocator> command_allocators[Const::kFrameCount];
		ComPtr<ID3D12DescriptorHeap> rtv_heap;
		ComPtr<ID3D12DescriptorHeap> dsv_heap;
		
		uint32_t rtv_descriptor_size = 0;
		uint32_t dsv_descriptor_size = 0;
	};

	const RendererCommon& GetRendererCommon();
	void EnqueueMsg(RT_MSG&&);
	IBaseSystem* CreateSystem(HWND hWnd, uint32_t width, uint32_t height);
};

