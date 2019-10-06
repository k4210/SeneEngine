#pragma once

#include "render_common.h"

struct RT_MSG_UpdateCamera { XMFLOAT3 position; XMFLOAT3 direction; };
struct RT_MSG_MeshBuffer { CD3DX12_GPU_DESCRIPTOR_HANDLE meshes_buff; uint32_t num_elements = 0; };
struct RT_MSG_StaticInstances { CD3DX12_GPU_DESCRIPTOR_HANDLE static_instances; uint32_t num_elements = 0; };
struct RT_MSG_StaticNodes { CD3DX12_GPU_DESCRIPTOR_HANDLE static_nodes; uint32_t num_elements = 0; };

enum class ERT_MSG : uint32_t
{
	UpdateCamera,
	MeshBuffer,
	StaticInstances,
	StaticNodes,
};

using RT_MSG = std::variant< 
	RT_MSG_UpdateCamera, 
	RT_MSG_MeshBuffer, 
	RT_MSG_StaticInstances, 
	RT_MSG_StaticNodes>;

class RenderThread : public BaseOngoingThread<RT_MSG>
{
	struct RTPimpl* pimpl_ = nullptr;

	HANDLE fence_event_ = nullptr;

	void Tick();
	void MainLoop() { while (!exit_requested_) { Tick(); } }

public:
	RenderThread(RendererCommon& renderer_common) : BaseOngoingThread<RT_MSG>(renderer_common) {}
	~RenderThread() { assert(!pimpl_); }

	void Create();
	void Start() { InnerStart(&RenderThread::MainLoop, this); }
	void Destroy();
};
