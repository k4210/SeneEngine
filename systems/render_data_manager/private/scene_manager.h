#pragma once

#include "rdm_base.h"

class SceneManager
{
	// <== only safe operations allowed
	preallocated_container<MeshComponent, Const::kStaticInstancesCapacity> instances_; //SIZE: 48 * 16 * 4096 = 3MB

	preallocated_container<BoundingSphere, Const::kStaticNodesCapacity> nodes_; //SIZE: 20 * 4096 = 80KB
	std::array<InstancesInNodeGPU, Const::kStaticNodesCapacity> inst_in_node_gpu_; //SIZE: 32 * 4096 = 128KB

	std::vector<MeshComponent*> pending_instances_; //wait until mesh is ready
	std::vector<MeshComponent*> pending_gpu_instances_updates_;
	uint32_t num_nodes_ = 0;
	bool dirty_ = false;

	bool IsNodeEmpty(uint32_t node_idx) const
	{
		for (int idx = 0; idx < Const::kMaxInstancesPerNode; idx++)
		{
			if (inst_in_node_gpu_[node_idx].instances[idx] != InstancesInNodeGPU::kInvalid)
				return false;
		}
		return true;
	}

	std::optional<uint32_t> FreeSlotInNode(uint32_t node_idx) const
	{
		for (int idx = 0; idx < Const::kMaxInstancesPerNode; idx++)
		{
			if (inst_in_node_gpu_[node_idx].instances[idx] == InstancesInNodeGPU::kInvalid)
				return { idx };
		}
		return {};
	}

	void AddToHierarchy(MeshComponent& instance)
	{
		auto add_instance_to_node = [&](uint32_t node_idx, uint32_t slot_in_node)
		{
			BoundingSphere& node = nodes_[node_idx];
			const BoundingSphere inst_sphere = instance.get_bounding_sphere();
			if (!IsNodeEmpty(node_idx))
			{
				BoundingSphere::CreateMerged(node, node, inst_sphere);
			}
			assert(!instance.is_sync_gpu());
			instance.node_idx = node_idx;
			assert(inst_in_node_gpu_[node_idx].instances[slot_in_node] == InstancesInNodeGPU::kInvalid);
			inst_in_node_gpu_[node_idx].instances[slot_in_node] = static_cast<InstancesInNodeGPU::TIndex>(instances_.safe_get_index(&instance));
			pending_gpu_instances_updates_.push_back(&instance);
		};

		assert(!instance.is_sync_gpu());
		dirty_ = true;
		uint32_t best_node = Const::kInvalid32;
		uint32_t best_node_slot = Const::kInvalid32;
		{
			const float max_wanted_dist = 8.0f * instance.mesh->radius * instance.transform.scale;
			const float min_wanted_radius = instance.mesh->radius / 4.0f;
			const float max_wanted_radius = instance.mesh->radius * 4.0f;
			float best_dist_sq = -1.0f;
			auto find_best_node = [&](const BoundingSphere& node)
			{
				auto distance_sq = [&](const XMFLOAT3& a, const XMFLOAT3& b) -> float
				{
					const XMFLOAT3 diff(a.x - b.x, a.y - b.y, a.z - b.z);
					return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
				};
				const uint32_t local_idx = nodes_.safe_get_index(&node);
				const float dist_sq = distance_sq(instance.transform.translate, node.Center);
				const bool good = (max_wanted_dist > dist_sq)
					&& (min_wanted_radius < node.Radius)
					&& (max_wanted_radius > node.Radius)
					&& ((best_dist_sq < 0) || (dist_sq < best_dist_sq));
				if (!good)
					return;
				auto free_slot = FreeSlotInNode(local_idx);
				if (!free_slot)
					return;
				best_dist_sq = dist_sq;
				best_node = local_idx;
				best_node_slot = *free_slot;
			};
			nodes_.for_each(find_best_node);
		}
		if ((best_node != Const::kInvalid32) && (best_node_slot != Const::kInvalid32))
		{
			add_instance_to_node(best_node, best_node_slot);
		}
		else
		{
			BoundingSphere* node = nodes_.allocate(instance.transform.translate, instance.mesh->radius);
			assert(node);
			num_nodes_++;
			add_instance_to_node(nodes_.safe_get_index(node), 0);
		}
	}

	void RemoveFromHierarchy(MeshComponent& instance)
	{
		dirty_ = true;
		InstancesInNodeGPU& local_instances = inst_in_node_gpu_[instance.node_idx];
		const InstancesInNodeGPU::TIndex inst_idx = static_cast<InstancesInNodeGPU::TIndex>(instances_.safe_get_index(&instance));
		auto found = std::find(local_instances.instances.begin(), local_instances.instances.end(), inst_idx);
		assert(found != local_instances.instances.end());
		*found = InstancesInNodeGPU::kInvalid;
		if (IsNodeEmpty(instance.node_idx))
		{
			nodes_.free(&nodes_[instance.node_idx]);
			num_nodes_--;
		}
		else
		{
			//TODO: recalculate Bounding Sphere size
		}
		instances_.safe_free(&instance);
	}

public:
	void UpdateNodes(StructBuffer& nodes_buffer, ID3D12GraphicsCommandList* command_list, UploadBuffer& upload_buffer) const
	{
		const auto upload_mem = upload_buffer.reserve_space(num_nodes_ * sizeof(BoundingSphere), alignof(BoundingSphere));
		assert(upload_mem);
		const auto [offset, dst_ptr] = *upload_mem;
		memcpy(dst_ptr, nodes_.safe_get_data(), num_nodes_ * sizeof(BoundingSphere));
		command_list->CopyBufferRegion(nodes_buffer.get_resource(), 0
			, upload_buffer.get_resource(), offset, num_nodes_ * sizeof(BoundingSphere));
	}
	void UpdateInstancesInNodes(StructBuffer& inst_in_node_buffer, ID3D12GraphicsCommandList* command_list, UploadBuffer& upload_buffer) const
	{
		const auto upload_mem = upload_buffer.reserve_space(num_nodes_ * sizeof(InstancesInNodeGPU), alignof(InstancesInNodeGPU));
		assert(upload_mem);
		const auto [offset, dst_ptr] = *upload_mem;
		memcpy(dst_ptr, nodes_.safe_get_data(), num_nodes_ * sizeof(InstancesInNodeGPU));
		command_list->CopyBufferRegion(inst_in_node_buffer.get_resource(), 0
			, upload_buffer.get_resource(), offset, num_nodes_ * sizeof(InstancesInNodeGPU));
	}

	EUpdateResult UpdateInstancesBuffer(StructBuffer& instances_buffer, ID3D12GraphicsCommandList* command_list, UploadBuffer& upload_buffer)
	{
		const int32_t local_size = static_cast<int32_t>(pending_gpu_instances_updates_.size());
		if (!local_size)
			return EUpdateResult::NoUpdateRequired;
		for (int32_t idx = local_size - 1; idx >= 0; idx--)
		{
			MeshComponent* inst = pending_gpu_instances_updates_[idx];
			assert(inst && instances_.is_set(inst) && inst->mesh);
			const auto reserved_mem = upload_buffer.reserve_space(sizeof(MeshInstanceGPU), alignof(MeshInstanceGPU));
			if (!reserved_mem.has_value())
				return EUpdateResult::UpdateStillNeeded;
			auto [offset, dst_ptr] = *reserved_mem;
			{
				Mesh& mesh = *inst->mesh;
				MeshInstanceGPU& data = *reinterpret_cast<MeshInstanceGPU*>(dst_ptr);
				data.mesh_index = static_cast<uint16_t>(mesh.index);
				data.radius = mesh.radius;
				data.transform = inst->transform;
				const float max_draw_distance = mesh.get_max_draw_distance();
				data.max_distance = ((max_draw_distance > 0xFFFF) || (max_draw_distance <= 0.0f)) 
					? 0xFFFF : static_cast<uint16_t>(max_draw_distance);
			}
			command_list->CopyBufferRegion(instances_buffer.get_resource()
				, instances_.safe_get_index(inst) * sizeof(MeshInstanceGPU)
				, upload_buffer.get_resource(), offset, sizeof(MeshInstanceGPU));
		}
		return EUpdateResult::Updated;
	}

	void Add(MeshComponent& instance)
	{
		assert(instances_.is_set(&instance));
		assert(instance.mesh);
		instance.mesh->instances++;
		if (instance.mesh->ready_to_render())
		{
			AddToHierarchy(instance);
		}
		else
		{
			pending_instances_.push_back(&instance);
		}
	}

	std::shared_ptr<Mesh> RemoveAndFree(MeshComponent& instance, bool remove_from_pending = true)
	{
		assert(instances_.is_set(&instance));
		assert(instance.mesh);
		assert(instance.mesh->instances);
		std::shared_ptr<Mesh> mesh = instance.mesh;
		instance.mesh->instances--;
		if (instance.is_sync_gpu())
		{
			RemoveFromHierarchy(instance);
		}
		else if (remove_from_pending)
		{
			auto found = std::find(pending_instances_.begin(), pending_instances_.end(), &instance);
			assert((found != pending_instances_.end()) && *found);
			pending_instances_.erase(found);
		}
		return !mesh->instances ? mesh : nullptr;
	}

	void CompactNodes()
	{
		constexpr auto lnpos = Bitset2::bitset2<Const::kStaticNodesCapacity>::npos;
		for(std::size_t first_free = nodes_.find_first_free(); 
			(first_free < num_nodes_) && (first_free != lnpos);
			first_free = nodes_.find_first_free())
		{
			dirty_ = true;

			const std::size_t taken_idx = nodes_.find_next_taken(num_nodes_ - 1);
			assert(taken_idx != lnpos);
			assert(nodes_.is_set(taken_idx));

			BoundingSphere& new_node = *nodes_.allocate();
			new_node = std::move(nodes_[taken_idx]);
			nodes_.free(&nodes_[taken_idx]);

			const uint32_t new_node_idx = nodes_.safe_get_index(&new_node);
			assert(new_node_idx < num_nodes_);

			inst_in_node_gpu_[new_node_idx] = std::move(inst_in_node_gpu_[taken_idx]);
			for (uint32_t idx = 0; idx < Const::kMaxInstancesPerNode; idx++)
			{
				const InstancesInNodeGPU::TIndex inst_idx = inst_in_node_gpu_[new_node_idx].instances[idx];
				if (inst_idx == InstancesInNodeGPU::kInvalid)
					continue;
				MeshComponent& inst = instances_[inst_idx];
				assert(inst.node_idx == taken_idx);
				inst.node_idx = new_node_idx;
			}
		}
	}

	void Clear(std::function<void(std::shared_ptr<Mesh>)> remove_mesh)
	{
		auto conditional_free = [&](MeshComponent& instance)
		{
			if (instance.mesh->instances)
			{
				auto mesh = RemoveAndFree(instance, false);
				if (mesh)
				{
					remove_mesh(mesh);
				}
			}
		};
		instances_.for_each(conditional_free);
		pending_instances_.clear();
		pending_gpu_instances_updates_.clear();
		nodes_.reset();
		num_nodes_ = 0;
		dirty_ = false;
	}

	bool NeedsNodesUpdate() const { return dirty_; }

	void SetUpToDate() { dirty_ = false; }

	void AddPendingInstances()
	{
		for (uint32_t idx = 0; idx < pending_instances_.size();)
		{
			MeshComponent* it = pending_instances_[idx];
			assert(it && it->mesh);
			if (it->mesh->ready_to_render())
			{
				AddToHierarchy(*it);
				RemoveSwap(pending_instances_, idx);
			}
			else
			{
				idx++;
			}
		}
	}

	MeshComponent* Allocate(std::shared_ptr<Mesh>&& mesh, Transform&& transform)
	{
		return instances_.safe_allocate(std::forward<std::shared_ptr<Mesh>>(mesh), std::forward<Transform>(transform));
	}

	uint32_t NumNodes() const { return num_nodes_; }
};
