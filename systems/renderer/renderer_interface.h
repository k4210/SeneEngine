#include "utils/base_system.h"
#include <DirectXMath.h>

struct RendererComponent // <= static mesh
{
	EntityId entity_id;
	//bounding box
	//ptr to asset vertex buffer
	//material data
	//flags
};

namespace IRenderer
{
	using Microsoft::WRL::ComPtr;
	struct Const
	{
		static constexpr uint32_t kInvalid = 0xFFFFFFFF;
		static constexpr uint64_t kInvalid64 = 0xFFFFFFFFFFFFFFFF;
		static constexpr uint32_t kFrameCount = 2;
		static constexpr uint32_t kLodNum = 3;
		static constexpr uint32_t kMeshCapacity = 4096;
		static constexpr uint32_t kStaticNodesCapacity = 4096;
		static constexpr uint32_t kStaticInstancesCapacity = 12 * 4096;
		static constexpr uint32_t kMoveableInstancesCapacity = 4 * 4096;
		static constexpr uint32_t kMaxInstancesPerNode = 32;
		static constexpr uint32_t kBuffHeapSize = 4 * kMeshCapacity;
	};

	struct RT_MSG_UpdateCamera { DirectX::XMFLOAT3 position; DirectX::XMFLOAT3 direction; };
	struct RT_MSG_MeshBuffer { CD3DX12_GPU_DESCRIPTOR_HANDLE meshes_buff; uint32_t num_elements = 0; };
	struct RT_MSG_StaticInstances { CD3DX12_GPU_DESCRIPTOR_HANDLE static_instances; uint32_t num_elements = 0; };
	struct RT_MSG_StaticNodes { CD3DX12_GPU_DESCRIPTOR_HANDLE static_nodes; uint32_t num_elements = 0; };
	struct RT_MSG_ToogleFullScreen { std::optional<bool> forced_mode; };
	//Animated stuff..
	//Post processes..
	//loading screen..
	//full screen..
	//video player..

	using RT_MSG = std::variant<
		RT_MSG_UpdateCamera,
		RT_MSG_MeshBuffer,
		RT_MSG_StaticInstances,
		RT_MSG_StaticNodes,
		RT_MSG_ToogleFullScreen>;

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
		ComPtr<ID3D12CommandAllocator> command_allocators[Const::kFrameCount];
		ComPtr<ID3D12DescriptorHeap> rtv_heap;

		
		uint32_t rtv_descriptor_size = 0;
		/*
			ComPtr<ID3D12Fence> direct_queue_fence_;
			ComPtr<ID3D12Fence> update_queue_fence_;
			ComPtr<ID3D12Fence> loading_queue_fence_;

			ComPtr<ID3D12Resource> counter_reset_resource_;

			DescriptorHeap buff_heap_;

			const class RenderThread* render_thread_ = nullptr;
			const class SceneThread* scene_thread_ = nullptr;
			const class MoveableThread* moveable_thread_ = nullptr;



			std::wstring base_shader_path;

			FenceValue GetLastUnfinishedFrameRT() const;
			FenceValue GetLastUnfinishedFrameMT() const;
			FenceValue GetLastUnfinishedBatch() const;
			FenceValue GetLastLoadedBatch() const;
		*/
	};

	const RendererCommon* GetRendererCommon();
	void EnqueueMsg(RT_MSG&&);
	IBaseSystem* CreateSystem(HWND hWnd, uint32_t width, uint32_t height);
	RendererComponent* AllocateComponent(EntityId);
	void FreeComponent(RendererComponent*);
};

struct ComponentHandle
{
private:
	RendererComponent* component = nullptr;

public:
	bool IsValid() const { return component; }
	EntityId GetEntityId() const;
	//void Initialize();
	void Cleanup();
	void UpdateTransform(Transform);
};