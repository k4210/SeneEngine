#pragma once

#include "rdm_base.h"

class SceneManager
{
	struct SceneNodeInstances
	{
		std::array<MeshComponent*, Const::kMaxInstancesPerNode> instances = { nullptr };
	};

	// <== only safe operations allowed
	preallocated_container<MeshComponent, Const::kStaticInstancesCapacity> instances_; //SIZE: 48 * 16 * 4096 = 3MB

	//Assumption: Valid elements are in consecutive space
	preallocated_container<SceneNodeGPU, Const::kStaticNodesCapacity> nodes_; //SIZE: 20 * 4096 = 80KB
	std::array<SceneNodeInstances, Const::kStaticNodesCapacity> inst_per_node_; //SIZE: 16 * 8 * 4096 = 512KB

	Bitset2::bitset2<Const::kStaticNodesCapacity> free_instance_grups_;
	std::vector<MeshComponent*> pending_instances_;
	std::vector<MeshComponent*> pending_gpu_instances_updates_;
	uint32_t num_nodes_ = 0;
	bool dirty_ = false;

	void AddToHierarchy(MeshComponent& instance)
	{
		auto distance_sq = [&](const XMFLOAT3 instance_pos, const uint32_t idx) -> float
		{
			const XMFLOAT3& a = instance_pos;
			const XMFLOAT3& b = nodes_[idx].bounding_sphere.Center;
			const XMFLOAT3 diff(a.x - b.x, a.y - b.y, a.z - b.z);
			return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
		};

		auto add_new_node = [&]() -> uint32_t
		{
			const uint32_t new_instance_group_space = static_cast<uint32_t>(free_instance_grups_.find_first());
			free_instance_grups_.set(new_instance_group_space, false);
			SceneNodeGPU* node = nodes_.allocate(SceneNodeGPU{ BoundingSphere(instance.transform.translate, instance.mesh->radius)
				, new_instance_group_space * Const::kMaxInstancesPerNode, 0 });
			assert(node);
			num_nodes_++;
			return nodes_.safe_get_index(node);
		};

		auto add_instance_to_node = [&](uint32_t node_idx)
		{
			SceneNodeGPU& node = nodes_[node_idx];
			const BoundingSphere inst_sphere = instance.get_bounding_sphere();
			BoundingSphere::CreateMerged(node.bounding_sphere, node.bounding_sphere, inst_sphere);
			assert(!instance.is_sync_gpu());
			instance.instance_idx = node.start + node.size;
			instance.node_idx = node_idx;
			pending_gpu_instances_updates_.push_back(&instance);
			inst_per_node_[node_idx].instances[node.size] = &instance;
			node.size++;
			assert(instance.is_sync_gpu());
		};

		auto find_node_for_instance = [&](const XMFLOAT3 instance_pos) -> std::optional<std::tuple<uint32_t, float>>
		{
			auto availible_node = [&](const uint32_t start_idx) -> uint32_t
			{
				for (uint32_t idx = start_idx; idx < num_nodes_; idx++)
				{
					assert(nodes_.is_set(idx));
					if (nodes_[idx].size < Const::kMaxInstancesPerNode)
					{
						return idx;
					}
				}
				return Const::kInvalid;
			};

			uint32_t best_idx = availible_node(0);
			if (best_idx == Const::kInvalid)
				return {};
			float best_dist_sq = distance_sq(instance_pos, best_idx);
			for (uint32_t idx = availible_node(best_idx + 1); idx < num_nodes_; idx = availible_node(idx))
			{
				const float local_dist_sq = distance_sq(instance_pos, idx);
				if (local_dist_sq < best_dist_sq)
				{
					best_dist_sq = local_dist_sq;
					best_idx = idx;
				}
			}
			return { {best_idx, best_dist_sq} };
		};

		assert(!instance.is_sync_gpu());
		dirty_ = true;
		const auto best_node = find_node_for_instance(instance.transform.translate);
		if (best_node.has_value())
		{
			const auto [idx, dist_sq] = *best_node;
			const bool place_for_new_node = num_nodes_ < Const::kStaticNodesCapacity;
			const float max_wanted_dist = 8.0f * instance.mesh->radius * instance.transform.scale;
			const bool good_node = dist_sq < (max_wanted_dist * max_wanted_dist);
			if (good_node || !place_for_new_node)
			{
				add_instance_to_node(idx);
				return;
			}
		}
		add_instance_to_node(add_new_node());
	}

	void RemoveFromHierarchy(MeshComponent& instance)
	{
		dirty_ = true;
		SceneNodeGPU& node_gpu = nodes_[instance.node_idx];
		SceneNodeInstances& local_instances = inst_per_node_[instance.node_idx];
		auto found = std::find(local_instances.instances.begin(), local_instances.instances.end(), &instance);
		assert(found != local_instances.instances.end());
		const uint32_t idx = static_cast<uint32_t>(std::distance(local_instances.instances.begin(), found));
		assert(node_gpu.size > 0);
		const auto last_idx = node_gpu.size - 1;
		assert(local_instances.instances[idx] != nullptr);
		if (idx != last_idx)
		{
			assert(local_instances.instances[last_idx] != nullptr);
			local_instances.instances[idx] = std::move(local_instances.instances[last_idx]);
			local_instances.instances[idx]->instance_idx = idx;
			pending_gpu_instances_updates_.push_back(local_instances.instances[idx]);
		}
		instances_.safe_free(local_instances.instances[last_idx]);
		local_instances.instances[last_idx] = nullptr;
		node_gpu.size--;
		if (0 == node_gpu.size)
		{
			const uint32_t instance_group_space = node_gpu.start / Const::kMaxInstancesPerNode;
			free_instance_grups_.set(instance_group_space, true);
			nodes_.free(&node_gpu);
			num_nodes_--;
		}
	}

public:
	SceneManager()
	{
		free_instance_grups_.set();
	}

	void UpdateNodes(StructBuffer& nodes_buffer, ID3D12GraphicsCommandList* command_list, UploadBuffer& upload_buffer) const
	{
		const auto upload_mem = upload_buffer.reserve_space(num_nodes_ * sizeof(SceneNodeGPU), alignof(SceneNodeGPU));
		assert(upload_mem);
		const auto [offset, dst_ptr] = *upload_mem;
		memcpy(dst_ptr, nodes_.safe_get_data(), num_nodes_ * sizeof(SceneNodeGPU));
		command_list->CopyBufferRegion(nodes_buffer.get_resource(), 0
			, upload_buffer.get_resource(), offset, num_nodes_ * sizeof(SceneNodeGPU));
	}

	// returns if all updates were submitted
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
				data.mesh_index = mesh.index;
				data.radius = mesh.radius;
				data.transform = inst->transform;
			}
			command_list->CopyBufferRegion(instances_buffer.get_resource(), inst->instance_idx * sizeof(MeshInstanceGPU)
				, upload_buffer.get_resource(), offset, sizeof(MeshInstanceGPU));
		}
		return EUpdateResult::Updated;
	}

	void Add(MeshComponent& instance)
	{
		assert(instances_.is_set(&instance));
		assert(instance.mesh);
		if (instance.mesh->ready_to_render())
		{
			AddToHierarchy(instance);
		}
		else
		{
			pending_instances_.push_back(&instance);
		}
	}

	void RemoveAndFree(MeshComponent& instance)
	{
		assert(instances_.is_set(&instance));
		if (instance.is_sync_gpu())
		{
			RemoveFromHierarchy(instance);
			return;
		}
		auto found = std::find(pending_instances_.begin(), pending_instances_.end(), &instance);
		assert((found != pending_instances_.end()) && *found);
		pending_instances_.erase(found);
	}

	void CompactNodes()
	{
		constexpr auto lnpos = Bitset2::bitset2<Const::kStaticNodesCapacity>::npos;
		for(std::size_t first_free = nodes_.find_first_free(); 
			(first_free > num_nodes_) && (first_free != lnpos);
			first_free = nodes_.find_first_free())
		{
			dirty_ = true;

			const std::size_t taken_idx = nodes_.find_next_taken(num_nodes_ - 1);
			assert(taken_idx != lnpos);
			assert(nodes_.is_set(taken_idx));

			SceneNodeGPU& new_node = *nodes_.allocate();
			new_node = std::move(nodes_[taken_idx]);
			nodes_.free(&nodes_[taken_idx]);
			const uint32_t new_node_idx = nodes_.safe_get_index(&new_node);
			assert(new_node_idx < num_nodes_);
			inst_per_node_[new_node_idx] = std::move(inst_per_node_[taken_idx]);
			for (uint32_t idx = 0; idx < new_node.size; idx++)
			{
				 MeshComponent* inst = inst_per_node_[new_node_idx].instances[idx];
				 assert(inst && instances_.is_set(inst));
				 assert(inst->node_idx == taken_idx);
				 inst->node_idx = new_node_idx;
			}
		}
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
