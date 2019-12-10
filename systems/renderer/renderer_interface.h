#include "utils/base_system.h"
#include <DirectXMath.h>

namespace IRenderer
{
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

	void EnqueueMsg(RT_MSG&&);

	IBaseSystem* CreateSystem(HWND hWnd, uint32_t width, uint32_t height);
};