#include "stdafx.h"
#include "rdm_base.h"
#include "mesh_manager.h"
#include "scene_manager.h"

struct GPUCommands
{
private:
	ComPtr<ID3D12GraphicsCommandList> copy_command_list_;
	ComPtr<ID3D12CommandQueue> copy_queue_;
	ComPtr<ID3D12CommandAllocator> copy_command_allocator_;
	bool command_list_open_ = false;

	friend struct SyncFence;

public:
	void Reopen()
	{
		if (command_list_open_)
			return;
		ThrowIfFailed(copy_command_allocator_->Reset());
		ThrowIfFailed(copy_command_list_->Reset(copy_command_allocator_.Get(), nullptr));
		command_list_open_ = true;
	}

	void Execute()
	{
		ThrowIfFailed(copy_command_list_->Close());
		ID3D12CommandList* ppCommandLists[] = { copy_command_list_.Get() };
		copy_queue_->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		command_list_open_ = false;
	}

	void Create(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type)
	{
		D3D12_COMMAND_QUEUE_DESC queue_desc = {};
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.Type = type;
		ThrowIfFailed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&copy_queue_)));
		NAME_D3D12_OBJECT(copy_queue_);

		ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&copy_command_allocator_)));
		NAME_D3D12_OBJECT(copy_command_allocator_);

		ThrowIfFailed(device->CreateCommandList(0, type, copy_command_allocator_.Get(), nullptr, IID_PPV_ARGS(&copy_command_list_)));
		NAME_D3D12_OBJECT(copy_command_list_);
		command_list_open_ = true;
	}

	void Destroy()
	{
		copy_command_list_.Reset();
		copy_queue_.Reset();
		copy_command_allocator_.Reset();
	}

	ID3D12GraphicsCommandList* GetCommandList() { assert(copy_command_list_); return copy_command_list_.Get(); }
};

struct SyncFence
{
private:
	ComPtr<ID3D12Fence> fence_;
	HANDLE fence_event_ = 0;
	uint64_t fence_value_ = 0;
	std::future<IRenderer::SyncGPU> rt_future_;

public:
	void Create(ID3D12Device* device)
	{
		fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fence_event_ == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		ThrowIfFailed(device->CreateFence(fence_value_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
		NAME_D3D12_OBJECT(fence_);
		fence_value_++;
	}

	void WaitForGPU(GPUCommands& commands)
	{
		ThrowIfFailed(commands.copy_queue_->Signal(fence_.Get(), fence_value_));
		ThrowIfFailed(fence_->SetEventOnCompletion(fence_value_, fence_event_));
		WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);
		fence_value_++;
	}

	std::promise<IRenderer::SyncGPU> MakePromiseRT()
	{
		assert(!rt_future_.valid());
		std::promise<IRenderer::SyncGPU> promise;
		rt_future_ = promise.get_future();
		assert(rt_future_.valid());
		return promise;
	}

	void WaitForRT(volatile bool& open)
	{
		assert(rt_future_.valid());
		for (std::future_status status = std::future_status::deferred;
			(status != std::future_status::ready) && open;
			status = rt_future_.wait_for(Microsecond{ 8000 }));
		if (!open)
			return;
		assert(rt_future_.valid());
		const auto [rt_fence, rt_fence_value] = rt_future_.get();
		assert(rt_fence);
		if (rt_fence->GetCompletedValue() < rt_fence_value)
		{
			ThrowIfFailed(rt_fence->SetEventOnCompletion(rt_fence_value, fence_event_));
			for (DWORD  status = WAIT_TIMEOUT;
				(status == WAIT_TIMEOUT) && open;
				status = WaitForSingleObjectEx(fence_event_, 8, FALSE));
		}
	}

	void Destroy()
	{
		if (fence_event_)
		{
			CloseHandle(fence_event_);
			fence_event_ = nullptr;
		}
	}

	~SyncFence()
	{
		Destroy();
	}
};

class RenderDataManager* render_data_manager = nullptr;
class RenderDataManager : public BaseSystemImpl<RDM_MSG>
{
protected:
	SceneManager scene_;
	MeshManager meshes_;
	Twins<std::vector<std::shared_ptr<Mesh>>> waiting_meshes_;
	SyncFence fence_;

	UploadBuffer upload_buffer_;
	DescriptorHeap buffers_heap_;

	GPUCommands commands_;
	
	StructBuffer mesh_buffer_;
	StructBuffer instances_buffer_;
	Twins<StructBuffer> nodes_;

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
		assert(msg.component);
		MeshComponent& instance = *msg.component;
		assert(instance.mesh);
		if (instance.mesh->index == Const::kInvalid)
		{
			meshes_.Add(instance.mesh);
			assert(instance.mesh->added_in_batch == Const::kInvalid);
			instance.mesh->added_in_batch.store(actual_batch_ + 1);
			waiting_meshes_.GetActive().push_back(instance.mesh);
		}
		scene_.Add(instance);
	}

	void operator()(RDM_MSG_RemoveComponent msg) 
	{
		assert(msg.component);
		std::shared_ptr<Mesh> mesh_to_remove = scene_.RemoveAndFree(*msg.component);
		if (mesh_to_remove)
		{
			meshes_.Remove(mesh_to_remove);
		}
	}

protected:
	void ThreadInitialize() override
	{
		const IRenderer::RendererCommon& common = IRenderer::GetRendererCommon();
		upload_buffer_.initialize(common.device.Get(), 2 * 1024 * 1024);
		buffers_heap_.create(common.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 4);

		commands_.Create(common.device.Get(), D3D12_COMMAND_LIST_TYPE_COPY);

		fence_.Create(common.device.Get());
		
		Construct<MeshDataGPU,		StructBuffer>(mesh_buffer_,			nullptr, Const::kMeshCapacity,				common.device.Get(), commands_.GetCommandList(), upload_buffer_, &buffers_heap_, { D3D12_RESOURCE_STATE_COMMON });
		Construct<MeshInstanceGPU,	StructBuffer>(instances_buffer_,	nullptr, Const::kStaticInstancesCapacity,	common.device.Get(), commands_.GetCommandList(), upload_buffer_, &buffers_heap_, { D3D12_RESOURCE_STATE_COMMON });
		for (auto& n : nodes_)
		{
			Construct<SceneNodeGPU, StructBuffer>(n, nullptr, Const::kStaticNodesCapacity, common.device.Get(), commands_.GetCommandList(), upload_buffer_, &buffers_heap_, { D3D12_RESOURCE_STATE_COMMON });
		}
		
		commands_.Execute();
		fence_.WaitForGPU(commands_);
		upload_buffer_.reset();

		IRenderer::EnqueueMsg({ IRenderer::RT_MSG_MeshBuffer { mesh_buffer_.get_srv_handle()} });
	}

	void ThreadCleanUp() override
	{
		fence_.WaitForGPU(commands_);
		fence_.Destroy();
		auto remove_mesh = [&](std::shared_ptr<Mesh> mesh) {meshes_.Remove(mesh);};
		scene_.Clear(remove_mesh);
		meshes_.Clear();
		commands_.Destroy();
	}

	void HandleSingleMessage(RDM_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(std::move(arg)); }, msg); }

	void Tick() override 
	{
		// Assumptions: no sync mesh/instances (common state).
		// Assumptions: Upload buffer is not filled during msg handling.
		const Microsecond start_time = GetTime();

		// 1. Wait for last update (5). Reopen CL, reset upload.
		fence_.WaitForGPU(commands_);
		waiting_meshes_.FlipActive();
		auto send_loaded_meshes = [](std::vector<std::shared_ptr<Mesh>>& ready_to_register)
		{
			if (ready_to_register.size())
			{
				IRenderer::EnqueueMsg({ IRenderer::RT_MSG_RegisterMeshes { std::move(ready_to_register) } });
				ready_to_register.clear();
			}
		};
		send_loaded_meshes(waiting_meshes_.GetActive());
		commands_.Reopen();
		upload_buffer_.reset();
		actual_batch_++;

		// 2. Update Nodes
		scene_.CompactNodes();
		const uint32_t nodes_num = scene_.NumNodes();
		assert(nodes_num < Const::kStaticNodesCapacity);
		const bool nodes_update_requiried = scene_.NeedsNodesUpdate();
		if(nodes_update_requiried)
		{
			nodes_.FlipActive();
			scene_.UpdateNodes(nodes_.GetActive(), commands_.GetCommandList(), upload_buffer_);
		}

		//3. Update instances
		bool must_sync_rt = false;
		while(true)
		{
			if (!IsOpen())
				return;
			const EUpdateResult local_status = scene_.UpdateInstancesBuffer(instances_buffer_, commands_.GetCommandList(), upload_buffer_);
			if (local_status == EUpdateResult::NoUpdateRequired && !nodes_update_requiried)
				break;
			commands_.Execute();
			fence_.WaitForGPU(commands_);
			commands_.Reopen();
			upload_buffer_.reset();
			must_sync_rt = true;
			if(local_status != EUpdateResult::UpdateStillNeeded)
				break;
		}

		// 4. Send new node buff to Render Thread. -> Get fence ?
		if(must_sync_rt)
		{
			std::promise<IRenderer::SyncGPU> rt_promise = fence_.MakePromiseRT();
			IRenderer::EnqueueMsg({ IRenderer::RT_MSG_StaticBuffers {
					nodes_.GetActive().get_srv_handle(),
					instances_buffer_.get_srv_handle(),
					nodes_num, std::move(rt_promise)} });
		}

		// 5. Update meshes/IB/VB -> CL
		while (true)
		{
			const EUpdateResult all_meshes_updated = meshes_.Update(mesh_buffer_, commands_.GetCommandList(), upload_buffer_);
			if (all_meshes_updated == EUpdateResult::NoUpdateRequired)
				break;
			commands_.Execute(); // we'll wait for it in 1st step
			if (all_meshes_updated == EUpdateResult::Updated)
				break;
			fence_.WaitForGPU(commands_); 
			commands_.Reopen();
			upload_buffer_.reset();
		}

		// 6. Add new nodes (locally on CPU)
		scene_.AddPendingInstances();

		// 7. Wait until [n-1] node buff is no longer used by RT. (fence from 4th step)
		if (must_sync_rt)
		{
			fence_.WaitForRT(open_);
		}

		// 8. Remove pending IB/VB and meshes.
		meshes_.FlushPendingRemove();

		const Microsecond duration = GetTime() - start_time;
		const Microsecond budget{ 7000 };
		if (IsOpen() && duration < budget)
		{
			std::this_thread::sleep_for(budget - duration);
		}
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