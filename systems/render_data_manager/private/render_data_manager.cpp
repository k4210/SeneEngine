#include "stdafx.h"
#include "../render_data_manager_interface.h"
#include "systems/renderer/renderer_interface.h"

namespace IRenderDataManager
{
	struct MeshComponent
	{
		std::shared_ptr<Mesh> mesh;
		Transform transform;

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
public:
	preallocated_container<MeshComponent, Const::kStaticInstancesCapacity> instances;

	//meshes buffer
	
	//instances buffer

	RenderDataManager() { assert(!render_data_manager); render_data_manager = this; }
	virtual ~RenderDataManager() { assert(render_data_manager); render_data_manager = nullptr; }

protected:
	void operator()(RDM_MSG_AddComponent) 
	{
	
	}
	void operator()(RDM_MSG_UpdateTransform) 
	{

	}
	void operator()(RDM_MSG_RemoveComponent) 
	{
	
	}

	void HandleSingleMessage(RDM_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(arg); }, msg); }
	void Tick() override {}
	void ThreadInitialize() override 
	{
	
	}

	void ThreadCleanUp() override 
	{
	
	}
};

IBaseSystem* IRenderDataManager::CreateSystem() { return new RenderDataManager(); }

void MeshHandle::Initialize(std::shared_ptr<Mesh> mesh, Transform transform)
{
	component_ = render_data_manager->instances.allocate(mesh, transform);
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