#pragma once

#include "utils/base_system.h"
#include "primitives/mesh_data.h"

namespace IGameplay
{
	using Microsoft::WRL::ComPtr;

	struct GP_MSG_MeshLoaded { std::shared_ptr<Mesh> mesh; };
	struct GP_MSG_MaterialLoaded { };

	using GP_MSG = std::variant<GP_MSG_MeshLoaded, GP_MSG_MaterialLoaded>;

	void EnqueueMsg(GP_MSG&&);
	IBaseSystem* CreateSystem();
}