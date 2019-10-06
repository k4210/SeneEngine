#pragma once

#include "render_common.h"

struct MT_MSG_Add { ID instance_id; InstanceData instance; uint32_t needed_batch; };
struct MT_MSG_Update{ ID instance_id; XMFLOAT4X4 model_to_world; BoundingSphere bounding_sphere_ws; };
struct MT_MSG_Remove{ ID instance_id; };

enum class EMT_MSG : uint32_t
{
	Add,
	Update,
	Remove,
};

using MT_MSG = std::variant<MT_MSG_Add, MT_MSG_Update, MT_MSG_Remove>;

class MoveableThread : public BaseOngoingThread<MT_MSG>
{
	std::unordered_map<ID, InstanceData> instances_;
	std::deque<MT_MSG_Add> pending_add_;	//wait until mesh is added

	void Tick();
	void MainLoop() { while (!exit_requested_) { Tick(); } }

public:
	MoveableThread(RendererCommon& renderer_common)
		: BaseOngoingThread<MT_MSG>(renderer_common) {}

	void Start() { InnerStart(&MoveableThread::MainLoop, this); }
	void Create() {}
	void Destroy() {}
};