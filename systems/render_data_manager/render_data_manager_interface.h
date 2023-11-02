#pragma once

#include "utils/base_system.h"
#include "utils/graphics/gpu_containers.h"
#include "mathfu/mathfu.h"
#include "primitives/mesh_data.h"

namespace IRenderDataManager
{
	IBaseSystem* CreateSystem();

	struct MeshComponent;

	class MeshHandle
	{
		MeshComponent* component_ = nullptr;
	public:
		void Initialize(std::shared_ptr<Mesh>&& mesh, mathfu::Transform&& transform);
		void UpdateTransform(Transform transform);
		void Cleanup();

		bool IsValid() const { return !!component_; }
		MeshHandle() = default;
		MeshHandle(MeshHandle&& other) : MeshHandle() { std::swap(component_, other.component_); }
		MeshHandle& operator=(MeshHandle&& other) { std::swap(component_, other.component_); }
		MeshHandle(const MeshHandle&) = delete;
		MeshHandle& operator=(const MeshHandle&) = delete;
		~MeshHandle() { Cleanup(); }

		static uint32_t ActualBatch();
		static bool IsMeshLoaded(Mesh& mesh);
	};
}