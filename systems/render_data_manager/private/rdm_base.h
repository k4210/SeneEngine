#pragma once
#include "../render_data_manager_interface.h"
#include "systems/renderer/renderer_interface.h"

class SceneManager;

namespace IRenderDataManager
{
	struct MeshComponent
	{
		std::shared_ptr<Mesh> mesh;
		Transform transform;

	private:
		friend SceneManager;

		uint32_t node_idx = Const::kInvalid32;

	public:
		MeshComponent(std::shared_ptr<Mesh> in_mesh, Transform in_transform)
			: mesh(in_mesh), transform(in_transform)
		{}

		bool is_sync_gpu() const
		{
			return node_idx != Const::kInvalid32;
		}

		BoundingSphere get_bounding_sphere() const
		{
			assert(mesh);
			return BoundingSphere(XMFLOAT3(transform.translate.x, transform.translate.y, transform.translate.z), transform.scale * mesh->radius);
		}
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