#pragma once

#include "utils/base_system.h"
#include "utils/gpu_containers.h"
#include "utils/math.h"
#include "primitives/mesh_data.h"

namespace IRenderDataManager
{
	IBaseSystem* CreateSystem();

	struct MeshComponent;

	class MeshHandle
	{
		MeshComponent* component_ = nullptr;
	public:
		void Initialize(std::shared_ptr<Mesh>&& mesh, Transform&& transform);
		void UpdateTransform(Transform transform);
		void Cleanup();

		bool IsValid() const { return !!component_; }
		MeshHandle() = default;
		MeshHandle(MeshHandle&& other) : MeshHandle() { std::swap(component_, other.component_); }
		MeshHandle& operator=(MeshHandle&& other) { std::swap(component_, other.component_); }
		MeshHandle(const MeshHandle&) = delete;
		MeshHandle& operator=(const MeshHandle&) = delete;
		~MeshHandle() { Cleanup(); }
	};
}