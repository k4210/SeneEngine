#include "stdafx.h"
#include "../render_data_manager_interface.h"
#include "systems/renderer/renderer_interface.h"

namespace IRenderDataManager
{
	struct MeshComponent
	{
		std::shared_ptr<Mesh> mesh;
		Transform transform;
		uint32_t node_idx = Const::kInvalid;

		MeshComponent(std::shared_ptr<Mesh> in_mesh, Transform in_transform)
			: mesh(in_mesh), transform(in_transform)
		{}
	};
}
using namespace IRenderDataManager;

struct RDM_MSG_AddComponent		{ MeshComponent* component = nullptr; };
struct RDM_MSG_UpdateTransform	{ MeshComponent* component = nullptr; Transform transform; };
struct RDM_MSG_RemoveComponent	{ MeshComponent* component = nullptr; };
using RDM_MSG = std::variant<RDM_MSG_AddComponent, RDM_MSG_UpdateTransform, RDM_MSG_RemoveComponent>;

class RenderDataManager* render_data_manager = nullptr;

class RenderDataManager : public BaseSystemImpl<RDM_MSG>
{
	preallocated_container<MeshComponent, Const::kStaticInstancesCapacity> instances_;

	DescriptorHeap buffers_heap_;
	UploadBuffer upload_buffer_;

	//bitset for mesh

	StructBuffer mesh_buffer_;
	StructBuffer instances_buffer_;
	StructBuffer nodes_buffers_[2];

	std::vector<std::shared_ptr<Mesh>> mesh_pending_add_;
	std::vector<std::shared_ptr<Mesh>> mesh_pending_remove_;
	std::vector<MeshComponent*> instance_pending_add_;
	std::vector<MeshComponent*> instance_pending_remove_;



public:
	RenderDataManager() { assert(!render_data_manager); render_data_manager = this; }
	virtual ~RenderDataManager() { assert(render_data_manager); render_data_manager = nullptr; }

	MeshComponent* Allocate(std::shared_ptr<Mesh>&& mesh, Transform&& transform)
	{
		return instances_.allocate(std::forward<std::shared_ptr<Mesh>>(mesh), std::forward<Transform>(transform));
	}

protected:
	void operator()(RDM_MSG_AddComponent) 
	{
		// Add pending IB/VB, meshes, instances
		// Add pending node change (add/modify) 
	}

	void operator()(RDM_MSG_UpdateTransform) 
	{
		// TODO:
		// Remove + Add ?
	}

	void operator()(RDM_MSG_RemoveComponent) 
	{
		// Add pending node change (remove) 
		// Pending remove IB/VB
	}

	void HandleSingleMessage(RDM_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(arg); }, msg); }
	void Tick() override 
	{
		// Assumptions: no sync mesh/instances (common state).

		// 1. Wait for last update (5). Reopen CL, reset upload.
		// 2. Remove pending nodes/instances. Fill [n] nodes buffer. -> CL
		// 3. Execute CL.
		// 3* Wait for CL. Send new node buff to Render Thread. -> Get fence ?

		// 4. Add pending IB/VB -> CL
		// 5. Prepare Command List with added/updated/removed meshes and instances. -> execute CL
		// if Upload full	-> wait -> 4 
		// else				-> 6

		// 6. Add new nodes (locally on CPU)
		// 7. Wait until [n-1] node buff is no longer used by RT. (3*)

		// 8. Remove pending IB/VB and meshes.
	}

	void ThreadInitialize() override 
	{
	
	}

	void ThreadCleanUp() override 
	{
	
	}
};

IBaseSystem* IRenderDataManager::CreateSystem() { return new RenderDataManager(); }

void MeshHandle::Initialize(std::shared_ptr<Mesh>&& mesh, Transform&& transform)
{
	component_ = render_data_manager->Allocate(std::forward<std::shared_ptr<Mesh>>(mesh), std::forward<Transform>(transform));
	render_data_manager->EnqueueMsg(RDM_MSG{ RDM_MSG_AddComponent{ component_ } });
}

void MeshHandle::UpdateTransform(Transform transform)
{
	render_data_manager->EnqueueMsg(RDM_MSG{ RDM_MSG_UpdateTransform{ component_, transform } });
}

void MeshHandle::Cleanup()
{
	render_data_manager->EnqueueMsg(RDM_MSG{ RDM_MSG_RemoveComponent{ component_ } });
}