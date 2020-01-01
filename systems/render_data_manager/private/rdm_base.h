#pragma once
#include "../render_data_manager_interface.h"
#include "systems/renderer/renderer_interface.h"

namespace IRenderDataManager
{
	struct MeshComponent
	{
		std::shared_ptr<Mesh> mesh;
		Transform transform;

		uint32_t node_idx = Const::kInvalid;
		uint32_t instance_idx = Const::kInvalid;

		bool is_sync_gpu() const
		{
			assert((node_idx == Const::kInvalid) == (instance_idx == Const::kInvalid));
			return node_idx != Const::kInvalid;
		}

		BoundingSphere get_bounding_sphere() const
		{
			assert(mesh);
			return BoundingSphere(transform.translate, transform.scale * mesh->radius);
		}

		MeshComponent(std::shared_ptr<Mesh> in_mesh, Transform in_transform)
			: mesh(in_mesh), transform(in_transform)
		{}
	};
}
using namespace IRenderDataManager;

struct RDM_MSG_AddComponent { MeshComponent* component = nullptr; };
struct RDM_MSG_RemoveComponent { MeshComponent* component = nullptr; };
using RDM_MSG = std::variant<RDM_MSG_AddComponent, RDM_MSG_RemoveComponent>;

enum class EUpdateResult
{
	NoUpdateRequired,
	Updated,
	UpdateStillNeeded,
};