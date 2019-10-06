#pragma once

#include "render_common.h"

struct ST_MSG_AddMesh				{ uint32_t index; std::shared_ptr<RawMeshData> mesh; };
struct ST_MSG_RemoveMesh			{ uint32_t index; };

struct ST_MSG_AddStaticNode			{ ID node_id; BoundingSphere bounding_sphere_ws; std::vector<InstanceData> instances; };
struct ST_MSG_RemoveStaticNode		{ ID node_id; };

enum class EST_MSG : uint32_t
{
	AddMesh,				//Any time						, loading
	RemoveMesh,				//Waiting for direct			, loading
	AddStaticNode,			//Any time						, loading
	RemoveStaticNode,		//Waiting for direct			, loading
};

using ST_MSG = std::variant<
	ST_MSG_AddMesh,
	ST_MSG_RemoveMesh,
	ST_MSG_AddStaticNode,
	ST_MSG_RemoveStaticNode>;
	
struct LODLocalData
{
	//VB
	//IB
	//Tex + mip?
};

struct MeshLocalData
{
	uint32_t index_ = RendererCommon::kInvalid;
	uint32_t pending_kill_batch_ = RendererCommon::kInvalid;
	uint32_t added_batch_ = RendererCommon::kInvalid;
	float distances_for_lod[RendererCommon::kLodNum] = {}; //above last, just skip drawing
	LODLocalData lod_data_[RendererCommon::kLodNum] = {};
	BoundingSphere bounding_sphere_;
};

struct SceneNode
{
	uint32_t index_ = RendererCommon::kInvalid;
	uint32_t start_inst_index_ = RendererCommon::kInvalid;
	uint32_t num_instances_ = RendererCommon::kInvalid;
	BoundingSphere bounding_sphere_world_space_;
};

class SceneThread : public BaseOngoingThread<ST_MSG>
{
	std::unordered_map<ID, SceneNode> static_nodes_;
	std::vector<InstanceData> static_instances_;
	std::unordered_map<ID, MeshLocalData> meshes_;

	struct FreeSpace { uint32_t start = 0; uint32_t size = 0; };
	std::deque<FreeSpace> static_instances_free_;

	std::mutex mesh_slots_mutex;
	Bitset2::bitset2<RendererCommon::kMeshCapacity> mesh_free_slots_;

	std::deque<ST_MSG_AddStaticNode> pending_add_static_node_;			//wait until has free space?

	void Tick();
	void MainLoop() { while (!exit_requested_) { Tick(); } }

public:
	SceneThread(RendererCommon& renderer_common)
		: BaseOngoingThread<ST_MSG>(renderer_common)
	{
		tick_index_ = 1;
		mesh_free_slots_.set();
	}

	void Start() { InnerStart(&SceneThread::MainLoop, this); }
	void Create() {}
	void Destroy() {}

	uint32_t PopMeshFreeSpaceThreadSafe()
	{
		std::lock_guard<std::mutex> guard(mesh_slots_mutex);
		const auto idx = mesh_free_slots_.find_first();
		assert(idx != Bitset2::bitset2<RendererCommon::kMeshCapacity>::npos);
		mesh_free_slots_.set(idx, false);
		return static_cast<uint32_t>(idx);

	}
	void PushMeshFreeSpaceThreadSafe(uint32_t idx)
	{
		std::lock_guard<std::mutex> guard(mesh_slots_mutex);
		assert(!mesh_free_slots_[idx]);
		mesh_free_slots_.set(idx, true);
	}
};
