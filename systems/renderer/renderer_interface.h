#pragma once

#include "base_system.h"
#include "graphics/gpu_containers.h"
#include "primitives/mesh_data.h"

class BaseApp;

namespace Const
{
	constexpr uint32_t kFrameCount = 2;
	constexpr uint32_t kMeshCapacity = 4096;
	constexpr uint32_t kStaticNodesCapacity = 4096;
	constexpr uint32_t kStaticInstancesCapacity = kStaticNodesCapacity * kMaxInstancesPerNode;
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
		DescriptorHeapElementRef nodes;
		DescriptorHeapElementRef instances;
		DescriptorHeapElementRef instances_in_node;
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
		ComPtr<ID3D12Device> device;
	};

	const RendererCommon& GetRendererCommon();
	void EnqueueMsg(RT_MSG&&);
	IBaseSystem* CreateSystem(HWND hWnd, uint32_t width, uint32_t height, BaseApp& app);
};

