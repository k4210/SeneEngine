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
	void operator()(GP_MSG_MeshLoaded) {}
	void operator()(GP_MSG_MaterialLoaded) {}

	void HandleSingleMessage(GP_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(arg); }, msg); }
	void Tick() override {}
	void ThreadInitialize() override {}
	void ThreadCleanUp() override {}
};

void IGameplay::EnqueueMsg(GP_MSG&& msg) { assert(gameplay_inst); gameplay_inst->EnqueueMsg(std::forward<GP_MSG>(msg)); }
IBaseSystem* IGameplay::CreateSystem() { return new GameplaySystem(); }