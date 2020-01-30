#include "stdafx.h"
#include "../gameplay_interface.h"
#include "systems/renderer/renderer_interface.h"
#include "systems/render_data_manager/render_data_manager_interface.h"

using namespace IGameplay;

class GameplaySystem;
static GameplaySystem* gameplay_inst = nullptr;

struct Entity
{
	IRenderDataManager::MeshHandle mesh;

	template<typename F>
	void for_each_component(F& func)
	{
		func(mesh);
	}
};

class GameplaySystem : public BaseSystemImpl<GP_MSG>
{
public:
	GameplaySystem() { assert(!gameplay_inst); gameplay_inst = this; }
	virtual ~GameplaySystem() { assert(gameplay_inst); gameplay_inst = nullptr; }

protected:
	Entity triangle_instance_;
	
	void operator()(GP_MSG_MeshLoaded) {}
	void operator()(GP_MSG_MaterialLoaded) {}

	void HandleSingleMessage(GP_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(arg); }, msg); }
	void Tick() override {}
	void ThreadInitialize() override 
	{
		std::shared_ptr<Mesh> triangle_mesh_ = std::make_shared<Mesh>();
		{
			std::unique_ptr<MeshDataCPU> data = std::make_unique<MeshDataCPU>();
			data->lod[0].indices = { 0, 1, 2 };
			data->lod[0].vertexes = {
					{ { 0.0f, 0.25f, 0.0f    }, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f} },
					{ { 0.25f, -0.25f, 0.0f  }, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f} },
					{ { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f} } };
			triangle_mesh_->mesh_data = std::move(data);
			triangle_mesh_->radius = 0.4f;
			triangle_mesh_->additional_lod_num = 0;
			triangle_mesh_->max_distance[0] = -1;
		}
		Transform transfrom;
		triangle_instance_.mesh.Initialize(std::move(triangle_mesh_), std::move(transfrom));
	}
	void ThreadCleanUp() override 
	{
		EntityUtils::Cleanup(triangle_instance_);
	}
};

void IGameplay::EnqueueMsg(GP_MSG&& msg) { assert(gameplay_inst); gameplay_inst->EnqueueMsg(std::forward<GP_MSG>(msg)); }
IBaseSystem* IGameplay::CreateSystem() { return new GameplaySystem(); }