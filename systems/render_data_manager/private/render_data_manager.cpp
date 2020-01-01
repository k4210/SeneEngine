#include "stdafx.h"
#include "rdm_base.h"
#include "mesh_manager.h"
#include "scene_manager.h"


class RenderDataManager* render_data_manager = nullptr;
class RenderDataManager : public BaseSystemImpl<RDM_MSG>
{
protected:
	SceneManager scene_;
	MeshManager meshes_;
	std::vector<std::shared_ptr<Mesh>> unregister_meshes_[2];

	UploadBuffer upload_buffer_;
	DescriptorHeap buffers_heap_;

	ComPtr<ID3D12GraphicsCommandList> copy_command_list_;
	ComPtr<ID3D12CommandQueue> copy_queue_;
	ComPtr<ID3D12CommandAllocator> copy_command_allocator_;
	ComPtr<ID3D12Fence> fence_;

	StructBuffer mesh_buffer_;
	StructBuffer instances_buffer_;
	StructBuffer nodes_buffers_[2];

	HANDLE fence_event_ = 0;
	uint64_t fence_value_ = 0;

	std::future<IRenderer::SyncGPU> rt_future_;
	uint32_t active_node_buff_ = 0;
	uint32_t active_unregister_meshes_ = 0;
	std::atomic_uint32_t actual_batch_ = 0;
	bool command_list_open_ = false;

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
		assert(msg.component);
		MeshComponent& instance = *msg.component;
		scene_.Add(instance);
		assert(instance.mesh);
		const bool new_mesh = meshes_.Add(instance.mesh);
		if (new_mesh)
		{
			assert(instance.mesh->added_in_batch == Const::kInvalid);
			instance.mesh->added_in_batch.store(actual_batch_ + 1);
			unregister_meshes_[active_unregister_meshes_].push_back(instance.mesh);
		}
	}

	void operator()(RDM_MSG_RemoveComponent msg) 
	{
		assert(msg.component);
		std::shared_ptr<Mesh> mesh = msg.component->mesh;
		scene_.RemoveAndFree(*msg.component);
		assert(mesh);
		meshes_.Remove(mesh);
	}

	void ReopenCL()
	{
		if (command_list_open_)
			return;
		ThrowIfFailed(copy_command_allocator_->Reset());
		ThrowIfFailed(copy_command_list_->Reset(copy_command_allocator_.Get(), nullptr));
		command_list_open_ = true;
	}

	void ExecuteCL()
	{
		ThrowIfFailed(copy_command_list_->Close());
		ID3D12CommandList* ppCommandLists[] = { copy_command_list_.Get() };
		copy_queue_->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		command_list_open_ = false;
	}

	void WaitForGPU()
	{
		ThrowIfFailed(copy_queue_->Signal(fence_.Get(), fence_value_));
		ThrowIfFailed(fence_->SetEventOnCompletion(fence_value_, fence_event_));
		WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);
		fence_value_++;
	}

	void RegisterMeshes()
	{
		const uint32_t new_active_unregister_meshes = 1 - active_unregister_meshes_;
		auto& ready_to_register = unregister_meshes_[new_active_unregister_meshes];
		if (ready_to_register.size())
		{
			IRenderer::EnqueueMsg({ IRenderer::RT_MSG_RegisterMeshes { std::move(ready_to_register) } });
			ready_to_register.clear();
		}
		active_unregister_meshes_ = new_active_unregister_meshes;
	}

protected:
	void ThreadInitialize() override
	{
		fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fence_event_ == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		const IRenderer::RendererCommon& common = IRenderer::GetRendererCommon();
		upload_buffer_.initialize(common.device.Get(), 2 * 1024 * 1024);
		buffers_heap_.create(common.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 8);

		D3D12_COMMAND_QUEUE_DESC queue_desc = {};
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		ThrowIfFailed(common.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&copy_queue_)));
		NAME_D3D12_OBJECT(copy_queue_);

		ThrowIfFailed(common.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&copy_command_allocator_)));
		NAME_D3D12_OBJECT(copy_command_allocator_);

		ThrowIfFailed(common.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, copy_command_allocator_.Get(), nullptr, IID_PPV_ARGS(&copy_command_list_)));
		NAME_D3D12_OBJECT(copy_command_list_);
		command_list_open_ = true;

		ThrowIfFailed(common.device->CreateFence(fence_value_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
		NAME_D3D12_OBJECT(fence_);
		fence_value_++;
		
		Construct<MeshDataGPU,		StructBuffer>(mesh_buffer_,			nullptr, Const::kMeshCapacity,				common.device.Get(), copy_command_list_.Get(), upload_buffer_, &buffers_heap_, { D3D12_RESOURCE_STATE_COMMON });
		Construct<MeshInstanceGPU,	StructBuffer>(instances_buffer_,	nullptr, Const::kStaticInstancesCapacity,	common.device.Get(), copy_command_list_.Get(), upload_buffer_, &buffers_heap_, { D3D12_RESOURCE_STATE_COMMON });
		Construct<SceneNodeGPU,		StructBuffer>(nodes_buffers_[0],	nullptr, Const::kStaticNodesCapacity,		common.device.Get(), copy_command_list_.Get(), upload_buffer_, &buffers_heap_, { D3D12_RESOURCE_STATE_COMMON });
		Construct<SceneNodeGPU,		StructBuffer>(nodes_buffers_[1],	nullptr, Const::kStaticNodesCapacity,		common.device.Get(), copy_command_list_.Get(), upload_buffer_, &buffers_heap_, { D3D12_RESOURCE_STATE_COMMON });

		ExecuteCL();
		WaitForGPU();
		upload_buffer_.reset();

		IRenderer::EnqueueMsg({ IRenderer::RT_MSG_MeshBuffer { mesh_buffer_.get_srv_handle_gpu()} });
	}

	void ThreadCleanUp() override
	{
		WaitForGPU();
		CloseHandle(fence_event_);
	}

	void HandleSingleMessage(RDM_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(std::move(arg)); }, msg); }
	void Tick() override 
	{
		// Assumptions: no sync mesh/instances (common state).
		// Assumptions: Upload buffer is not filled during msg handling.

		// 1. Wait for last update (5). Reopen CL, reset upload.
		WaitForGPU();
		RegisterMeshes();
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
			scene_.UpdateNodes(nodes_buffers_[active_node_buff_], copy_command_list_.Get(), upload_buffer_);
		}

		//3. Update instances
		bool must_sync_rt = false;
		while(true)
		{
			const EUpdateResult local_status = scene_.UpdateInstancesBuffer(instances_buffer_, copy_command_list_.Get(), upload_buffer_);
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
			const EUpdateResult all_meshes_updated = meshes_.Update(mesh_buffer_, copy_command_list_.Get(), upload_buffer_);
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
			assert(rt_fence);
			ThrowIfFailed(rt_fence->SetEventOnCompletion(rt_fence_value, fence_event_));
			WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);
		}

		// 8. Remove pending IB/VB and meshes.
		meshes_.FlushPendingRemove();
	}
};

IBaseSystem* IRenderDataManager::CreateSystem() { return new RenderDataManager(); }

void MeshHandle::Initialize(std::shared_ptr<Mesh>&& mesh, Transform&& transform)
{
	assert(render_data_manager);
	component_ = render_data_manager->Allocate(std::forward<std::shared_ptr<Mesh>>(mesh), std::forward<Transform>(transform));
	render_data_manager->EnqueueMsg(RDM_MSG{ RDM_MSG_AddComponent{ component_ } });
}

void MeshHandle::UpdateTransform(Transform)
{
	//render_data_manager->EnqueueMsg(RDM_MSG{ RDM_MSG_UpdateTransform{ component_, transform } });
}

void MeshHandle::Cleanup()
{
	if (component_)
	{
		assert(render_data_manager);
		render_data_manager->EnqueueMsg(RDM_MSG{ RDM_MSG_RemoveComponent{ component_ } });
		component_ = nullptr;
	}
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