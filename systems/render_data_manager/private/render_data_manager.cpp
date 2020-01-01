#include "stdafx.h"
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

struct RDM_MSG_AddComponent		{ MeshComponent* component = nullptr; };
struct RDM_MSG_RemoveComponent	{ MeshComponent* component = nullptr; };
using RDM_MSG = std::variant<RDM_MSG_AddComponent, RDM_MSG_RemoveComponent>;

class RenderDataManager* render_data_manager = nullptr;

enum class EUpdateResult
{
	NoUpdateRequired,
	Updated,
	UpdateStillNeeded,
};

class MeshManager
{
	Bitset2::bitset2<Const::kMeshCapacity> free_mesh_slots_;
	std::vector<std::shared_ptr<Mesh>> pending_remove_;
	std::vector<std::shared_ptr<Mesh>> pending_add_;

	bool AddMeshGPU(StructBuffer& mesh_buffer, Mesh& mesh, ID3D12GraphicsCommandList* command_list, UploadBuffer& upload_buffer)
	{
		const auto reserved_mem = upload_buffer.reserve_space(sizeof(MeshDataGPU), alignof(MeshDataGPU));
		if (reserved_mem.has_value())
		{
			auto [offset, dst_ptr] = *reserved_mem;
			{
				MeshDataGPU& data = *reinterpret_cast<MeshDataGPU*>(dst_ptr);
				data.index_buffer = mesh.index_buffer.get_index_view();
				data.vertex_buffer = mesh.vertex_buffer.get_vertex_view();
				assert(mesh.mesh_data.has_value());
				data.radius = mesh.radius;
				data.texture_idx[0] = data.texture_idx[1] = 0;
			}
			command_list->CopyBufferRegion(mesh_buffer.get_resource(), mesh.index * sizeof(MeshDataGPU)
				, upload_buffer.get_resource(), offset, sizeof(MeshDataGPU));
		}
		return reserved_mem.has_value();
	}

public:
	MeshManager() { free_mesh_slots_.set(); }

	bool Add(std::shared_ptr<Mesh> mesh)
	{
		assert(mesh);
		const bool new_mesh = mesh->index == Const::kInvalid;
		if (new_mesh)
		{
			const auto free_slot = free_mesh_slots_.find_first();
			assert(free_slot != Bitset2::bitset2<Const::kMeshCapacity>::npos);
			free_mesh_slots_.set(free_slot, false);
			mesh->index = free_slot;

			pending_add_.push_back(mesh);
		}
		mesh->instances++;
		return new_mesh;
	}

	void Remove(std::shared_ptr<Mesh> mesh)
	{
		assert(mesh->index != Const::kInvalid);
		assert(mesh->instances);
		assert(mesh->is_ready());
		mesh->instances--;
		if (!mesh->instances)
		{
			pending_remove_.push_back(mesh);
		}
	}

	void FlushPendingRemove()
	{
		for (auto mesh : pending_remove_)
		{
			assert(mesh->index != Const::kInvalid);
			assert(!free_mesh_slots_[mesh->index]);
			free_mesh_slots_.set(mesh->index, true);
			mesh->index = Const::kInvalid;
			mesh->added_in_batch = Const::kInvalid;
		}
		pending_remove_.clear();
	}

	EUpdateResult Update(StructBuffer& mesh_buffer, ID3D12GraphicsCommandList* command_list
		, UploadBuffer& upload_buffer, DescriptorHeap& desc_heap)
	{
		if (!pending_add_.size())
			return EUpdateResult::NoUpdateRequired;
		const IRenderer::RendererCommon& render_common = IRenderer::GetRendererCommon();
		for (uint32_t idx = 0; idx < pending_add_.size();)
		{
			auto mesh = pending_add_[idx];
			assert(mesh);
			bool added = true;
			if (!mesh->vertex_buffer.is_ready())
			{
				assert(mesh->mesh_data);
				std::vector<Vertex>& vertexes = mesh->mesh_data->vertexes;
				added = Construct<Vertex, VertexBuffer>(mesh->vertex_buffer, 
					&(vertexes[0]), vertexes.size(), render_common.device.Get(),
					command_list, upload_buffer, desc_heap);
			}
			if (added && !mesh->index_buffer.is_ready())
			{
				assert(mesh->mesh_data);
				std::vector<uint32_t>& indices = mesh->mesh_data->indices;
				added = Construct<uint32_t, IndexBuffer32>(mesh->index_buffer,
					&(indices[0]), indices.size(), render_common.device.Get(),
					command_list, upload_buffer, desc_heap);
			}
			if (added)
			{
				added = AddMeshGPU(mesh_buffer , *mesh, command_list, upload_buffer);
			}
			if (added)
			{
				pending_add_.erase(pending_add_.begin() + idx);
				mesh->mesh_data.reset();
			}
			else
			{
				return EUpdateResult::UpdateStillNeeded;
			}
		}
		return EUpdateResult::Updated;
	}
};

class SceneManager
{
	struct SceneNode
	{
		BoundingSphere bounding_sphere;
		uint32_t idx = Const::kInvalid;
		uint32_t instances_start = Const::kInvalid;
		uint32_t instances_num = Const::kInvalid;
		std::array<MeshComponent*, Const::kMaxInstancesPerNode> instances;
	};

	preallocated_container<MeshComponent, Const::kStaticInstancesCapacity> instances_; // <== only safe operations
	preallocated_container<SceneNodeGPU, Const::kStaticNodesCapacity> nodes_; //Assumption: Valid elements are in consecutive space
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
			const uint32_t new_instance_group_space = free_instance_grups_.find_first();
			free_instance_grups_.set(new_instance_group_space, false);
			SceneNodeGPU* node = nodes_.allocate(BoundingSphere(instance.transform.translate, instance.mesh->radius)
				, new_instance_group_space * Const::kMaxInstancesPerNode, 0);
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
			node.size++;
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

	bool RemoveFromHierarchy(MeshComponent& instance)
	{
		assert(instance.is_sync_gpu());
		dirty_ = true;

		//remove from node
		//if last instance in the node -> remove node
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
		memcpy(dst_ptr, nodes_.get_data(), num_nodes_ * sizeof(SceneNodeGPU));
		command_list->CopyBufferRegion(nodes_buffer.get_resource(), 0
			, upload_buffer.get_resource(), offset, num_nodes_ * sizeof(SceneNodeGPU));
	}

	// returns if all updates were submitted
	EUpdateResult UpdateInstancesBuffer(StructBuffer& instances_buffer, ID3D12GraphicsCommandList* command_list, UploadBuffer& upload_buffer)
	{

	}

	void Add(MeshComponent& instance)
	{
		assert(instance.mesh);
		if (instance.mesh->ready_to_render())
		{
			AddToHierarchy(instance);
		}
		pending_instances_.push_back(&instance);
	}

	void RemoveAndFree(MeshComponent& instance)
	{
		if (RemoveFromHierarchy(instance))
			return;
		auto found = std::find(pending_instances_.begin(), pending_instances_.end(), &instance);
		assert((found != pending_instances_.end()) && *found);
		pending_instances_.erase(found);
	}

	void CompactNodes()
	{

	}

	bool NeedsNodesUpdate() const { return dirty_; }

	void SetUpToDate() { dirty_ = false; }

	bool AddPendingInstances()
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

class RenderDataManager : public BaseSystemImpl<RDM_MSG>
{
protected:
	DescriptorHeap buffers_heap_;
	UploadBuffer upload_buffer_;
	ComPtr<ID3D12GraphicsCommandList> command_list_;

	SceneManager scene_;
	MeshManager meshes_;
	
	StructBuffer mesh_buffer_;
	StructBuffer instances_buffer_;
	StructBuffer nodes_buffers_[2];
	uint32_t active_node_buff_ = 0;
	std::future<IRenderer::SyncGPU> rt_future_;
	HANDLE fence_event_ = 0;
	std::atomic_uint32_t actual_batch_ = 0;

public:
	RenderDataManager() { assert(!render_data_manager); render_data_manager = this; }
	virtual ~RenderDataManager() { assert(render_data_manager); render_data_manager = nullptr; }
	MeshComponent* Allocate(std::shared_ptr<Mesh>&& mesh, Transform&& transform)
	{
		return scene_.Allocate(std::forward<std::shared_ptr<Mesh>>(mesh), std::forward<Transform>(transform));
	}
	uint32_t GetActualBatch() const { return actual_batch_; }

protected:
	void operator()(RDM_MSG_AddComponent msg) 
	{
		assert(msg.component && instances_.is_set(msg.component));
		MeshComponent& instance = *msg.component;
		scene_.Add(instance);
		assert(instance.mesh);
		const bool new_mesh = meshes_.Add(instance.mesh);
		if (new_mesh)
		{
			assert(instance.mesh->added_in_batch == Const::kInvalid);
			instance.mesh->added_in_batch.store(actual_batch_ + 1);
		}
	}

	void operator()(RDM_MSG_RemoveComponent msg) 
	{
		assert(msg.component && instances_.is_set(msg.component));
		std::shared_ptr<Mesh> mesh = msg.component->mesh;
		scene_.RemoveAndFree(*msg.component);
		assert(mesh);
		meshes_.Remove(mesh);
	}

	void ReopenCL()
	{

	}

	void ExecuteCL()
	{

	}

	void WaitForGPU()
	{

	}

	void HandleSingleMessage(RDM_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(arg); }, msg); }
	void Tick() override 
	{
		// Assumptions: no sync mesh/instances (common state).
		// Assumptions: Upload buffer is not filled during msg handling.

		// 1. Wait for last update (5). Reopen CL, reset upload.
		WaitForGPU();
		ReopenCL();
		upload_buffer_.reset();
		actual_batch_++;

		// 2. Update Nodes
		scene_.CompactNodes();
		const uint32_t nodes_num = scene_.NumNodes();
		assert(nodes_num < Const::kStaticNodesCapacity);
		const bool nodes_update_requiried = scene_.NeedsNodesUpdate();
		if(nodes_update_requiried)
		{
			active_node_buff_ = 1 - active_node_buff_;
			scene_.UpdateNodes(nodes_buffers_[active_node_buff_], command_list_.Get(), upload_buffer_);
		}

		//3. Update instances
		bool must_sync_rt = false;
		while(true)
		{
			const EUpdateResult local_status = scene_.UpdateInstancesBuffer(instances_buffer_, command_list_.Get(), upload_buffer_);
			if (local_status == EUpdateResult::NoUpdateRequired && !nodes_update_requiried)
				break;
			ExecuteCL();
			WaitForGPU();
			ReopenCL();
			upload_buffer_.reset();
			must_sync_rt = true;
			if(local_status != EUpdateResult::UpdateStillNeeded)
				break;
		}

		// 4. Send new node buff to Render Thread. -> Get fence ?
		if(must_sync_rt)
		{
			assert(!rt_future_.valid());
			std::promise<IRenderer::SyncGPU> rt_promise;
			rt_future_ = rt_promise.get_future();
			IRenderer::EnqueueMsg({ IRenderer::RT_MSG_StaticBuffers {
					nodes_buffers_[active_node_buff_].get_srv_handle_gpu(),
					instances_buffer_.get_srv_handle_gpu(),
					nodes_num, std::move(rt_promise)} });
		}

		// 5. Update meshes/IB/VB -> CL
		while (true)
		{
			const EUpdateResult all_meshes_updated = meshes_.Update(mesh_buffer_, command_list_.Get(), upload_buffer_, buffers_heap_);
			if (all_meshes_updated == EUpdateResult::NoUpdateRequired)
				break;
			ExecuteCL();
			if (all_meshes_updated == EUpdateResult::Updated)
				break;
			WaitForGPU(); // we'll wait for it in 1st step
			ReopenCL();
			upload_buffer_.reset();
		}

		// 6. Add new nodes (locally on CPU)
		scene_.AddPendingInstances();

		// 7. Wait until [n-1] node buff is no longer used by RT. (fence from 4th step)
		if (must_sync_rt)
		{
			rt_future_.wait();
			const auto [rt_fence, rt_fence_value] = rt_future_.get();
			ThrowIfFailed(rt_fence->SetEventOnCompletion(rt_fence_value, fence_event_));
			WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);
		}

		// 8. Remove pending IB/VB and meshes.
		meshes_.FlushPendingRemove();


	}

	void ThreadInitialize() override 
	{
	
	}

	void ThreadCleanUp() override 
	{
	
	}
};

IBaseSystem* IRenderDataManager::CreateSystem() { return new RenderDataManager(); }

uint32_t GetDoneBatch() { assert(render_data_manager); return render_data_manager->GetDoneBatch(); }

void MeshHandle::Initialize(std::shared_ptr<Mesh>&& mesh, Transform&& transform)
{
	component_ = render_data_manager->Allocate(std::forward<std::shared_ptr<Mesh>>(mesh), std::forward<Transform>(transform));
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

uint32_t MeshHandle::ActualBatch()
{
	assert(render_data_manager);
	return render_data_manager->GetActualBatch();
}

bool MeshHandle::IsMeshLoaded(Mesh& mesh)
{
	const uint32_t mesh_batch = mesh.added_in_batch;
	return (mesh_batch != Const::kInvalid) && (mesh_batch < ActualBatch());
}