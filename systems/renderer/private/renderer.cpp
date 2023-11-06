#include "stdafx.h"
#include "../renderer_interface.h"
#include "base_app_helper.h"
#include "base_app.h"
#include "primitives/mesh_data.h"
#include "graphics/command_signature.h"
#include "graphics/root_signature.h"
#include "graphics/pipeline_state.h"
#include "stat/stat.h"
#include <cstddef>

#include "base_renderer.h"

#ifdef NDEBUG
#include "compiled_shaders/release/filter_nodes_frustum_cs.h"
#include "compiled_shaders/release/filter_nodes_depth_cs.h"
#include "compiled_shaders/release/extract_instances_cs.h"
#include "compiled_shaders/release/filter_inst_frustum_cs.h"
#include "compiled_shaders/release/filter_inst_depth_cs.h"
#include "compiled_shaders/release/draw_ps.h"
#include "compiled_shaders/release/draw_vs.h"
#include "compiled_shaders/release/generate_draw_commands_cs.h"
#include "compiled_shaders/release/compact_depth_cs.h"
#else
#include "compiled_shaders/debug/filter_nodes_frustum_cs.h"
#include "compiled_shaders/debug/filter_nodes_depth_cs.h"
#include "compiled_shaders/debug/extract_instances_cs.h"
#include "compiled_shaders/debug/filter_inst_frustum_cs.h"
#include "compiled_shaders/debug/filter_inst_depth_cs.h"
#include "compiled_shaders/debug/draw_ps.h"
#include "compiled_shaders/debug/draw_vs.h"
#include "compiled_shaders/debug/generate_draw_commands_cs.h"
#include "compiled_shaders/debug/compact_depth_cs.h"
#endif

using namespace DirectX;
using Microsoft::WRL::ComPtr;
using namespace IRenderer;

struct IndirectCommandGPU
{
	XMFLOAT4X4 wvp_mtx;
	D3D12_INDEX_BUFFER_VIEW index_buffer;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer;
	XMUINT2 texture_index;
	float mat_val;
	D3D12_DRAW_INDEXED_ARGUMENTS draw_arg;
};

struct SceneManagerParamsGPU
{
	uint32_t num_instances_ = 1;
};

class Renderer : public BaseRenderer
{
	enum class E_SRV
	{
		Invalid,
		Temp1,
		Temp2,
	};

	enum class E_UAV
	{
		Temp1,
		Temp2,
		Commands,
	};

	struct PerFrame
	{
		UploadBuffer upload_buffer;
		DescriptorHeap buffers_heap;
		ConstantBuffer scene_manager_params;

		UavCountedBuffer indirect_draw_commands;
		UavCountedBuffer temp_buffer_1;
		UavCountedBuffer temp_buffer_2;

		TransitionManager tm;

		UavCountedBuffer& GetUav(E_UAV uav)
		{
			if (uav == E_UAV::Temp1) return temp_buffer_1;
			if (uav == E_UAV::Temp2) return temp_buffer_2;
			return indirect_draw_commands;
		}

		UavCountedBuffer& GetSrv(E_SRV srv)
		{
			if (srv == E_SRV::Temp1) return temp_buffer_1;
			if (srv == E_SRV::Temp2) return temp_buffer_2;
			assert(false);
			return indirect_draw_commands;
		}
	};

	struct SceneManagerPass
	{
		static constexpr uint32_t k_threads_x = 64;

		ComPtr<ID3D12PipelineState> pipeline_state_;
		ComPtr<ID3D12RootSignature> root_signature_;

		const uint32_t max_dispatch;
		const E_SRV srv;
		const E_UAV uav;

		void destroy()
		{
			pipeline_state_.Reset();
			root_signature_.Reset();
		}

		SceneManagerPass(uint32_t in_max_dispatch, E_UAV in_uav, E_SRV in_srv)
			: max_dispatch(in_max_dispatch), srv(in_srv), uav(in_uav) {}
	};

protected:
	ComPtr<ID3D12GraphicsCommandList> command_list_;
	ComPtr<ID3D12CommandSignature> command_signature_;
	ComPtr<ID3D12PipelineState> render_state_;
	ComPtr<ID3D12RootSignature> draw_root_signature_;

	SceneManagerPass filter_nodes_frustum_;			//			-> temp_1
	SceneManagerPass filter_nodes_depth_;			// temp_1	-> temp_2
	SceneManagerPass extract_instances_;			// temp_2	-> temp_1
	SceneManagerPass filter_inst_frustum_;			// temp_1	-> temp_2
	SceneManagerPass filter_inst_depth_;			// temp_2	-> temp_1
	SceneManagerPass generate_draw_commands_;		// temp_1	-> indirect_draw_commands

	DescriptorHeap persistent_descriptor_heap_;
	CommitedBuffer reset_counter_src_;

	std::array<PerFrame, Const::kFrameCount> per_frame_;

	DescriptorHeapElementRef meshes_buff_;
	DescriptorHeapElementRef static_nodes_;
	DescriptorHeapElementRef static_instances_;
	DescriptorHeapElementRef static_instances_in_node;
	uint32_t num_nodes_ = 0;

	std::vector<std::shared_ptr<Mesh>> to_register_;

	uint64 frame_counter = 0;
	Utils::TimeType time;
	BaseApp& app;

	uint32 static_buffers_frame_idx = 0;
	uint64 static_buffers_fence_value = 0;
public:
	Renderer(HWND hWnd, uint32_t width, uint32_t height, BaseApp& in_app)
		: BaseRenderer(hWnd, width, height) 
		, filter_nodes_frustum_(	Const::kStaticNodesCapacity			, E_UAV::Temp1		, E_SRV::Invalid)
		, filter_nodes_depth_(		Const::kStaticNodesCapacity			, E_UAV::Temp2		, E_SRV::Temp1)
		, extract_instances_(		Const::kStaticNodesCapacity			, E_UAV::Temp1		, E_SRV::Temp2)
		, filter_inst_frustum_(		Const::kStaticInstancesCapacity		, E_UAV::Temp2		, E_SRV::Temp1)
		, filter_inst_depth_(		Const::kStaticInstancesCapacity		, E_UAV::Temp1		, E_SRV::Temp2)
		, generate_draw_commands_(	Const::kStaticInstancesCapacity		, E_UAV::Commands	, E_SRV::Temp1)
		, app(in_app)
	{}

protected:
	PerFrame& GetPerFrame() { return per_frame_[GetFrameIndex()]; }

	void operator()(RT_MSG_UpdateCamera) {}

	void operator()(RT_MSG_MeshBuffer msg)
	{
		meshes_buff_ = msg.meshes_buff;
	}

	void operator()(RT_MSG_StaticBuffers msg)
	{
		static_nodes_ = msg.nodes;
		static_instances_ = msg.instances;
		static_instances_in_node = msg.instances_in_node;
		num_nodes_ = msg.num_nodes;

		const SyncPerFrame& sync = sync_[static_buffers_frame_idx];
		msg.promise_previous_nodes_not_used.set_value(SyncGPU{ sync.fence, static_buffers_fence_value });

		static_buffers_frame_idx = frame_index_;
		static_buffers_fence_value = GetSync().fence_value;
	}

	void operator()(RT_MSG_ToogleFullScreen msg)
	{
		SetupFullscreen(msg.forced_mode);
	}

	void operator()(RT_MSG_RegisterMeshes msg)
	{
		if (!to_register_.size())
		{
			to_register_ = std::move(msg.to_register);
		}
		else
		{
			to_register_.insert(to_register_.end(), msg.to_register.begin(), msg.to_register.end());
		}
	}

	void HandleSingleMessage(RT_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(std::move(arg)); }, msg); }

	void Tick() override
	{
		STAT_TIME_SCOPE(renderer, tick);

		if (!meshes_buff_ || !static_nodes_ || !static_instances_ || !static_instances_in_node)
			return;

		auto& sync = GetSync();
		{
			STAT_TIME_SCOPE(renderer, wait_gpu);
			sync.Wait();
		}
		auto& pf = GetPerFrame();
		pf.upload_buffer.reset();

		auto Draw = [&]()
		{
			STAT_TIME_SCOPE(renderer, draw);
			auto device = common_.device.Get();

			//INITIAL COMMAND LIST
			{
				ID3D12CommandAllocator* const allocator = GetActiveAllocator();
				ThrowIfFailed(allocator->Reset());
				ThrowIfFailed(command_list_->Reset(allocator, nullptr));
				pf.tm.set_command_list(command_list_);
			}

			//REGISTER MESHES
			{
				for (auto& mesh : to_register_)
				{
					assert(mesh);
					for (uint32_t idx = 0; idx < mesh->get_lod_num(); idx++)
					{
						pf.tm.insert(mesh->lod[idx].index_buffer.transition_barrier(D3D12_RESOURCE_STATE_INDEX_BUFFER));
						pf.tm.insert(mesh->lod[idx].vertex_buffer.transition_barrier(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
					}
				}
				to_register_.clear();
			}

			//PREPARE BUFFERS AND RT
			{
				pf.tm.insert(CD3DX12_RESOURCE_BARRIER::Transition(GetRTResource(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
				pf.tm.wait_for(pf.scene_manager_params.get_resource());
				SceneManagerParamsGPU params;
				const bool params_ok = FillGpuContainer<SceneManagerParamsGPU, ConstantBuffer>(
					pf.scene_manager_params, &params, 1, command_list_.Get(), pf.upload_buffer);
				assert(params_ok);
				pf.tm.wait_for(pf.scene_manager_params.transition_barrier(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
			}

			//HEAP
			{
				pf.buffers_heap.clear(64);
				CopyDescriptor(device, pf.buffers_heap, static_nodes_, 0);
				CopyDescriptor(device, pf.buffers_heap, static_instances_, 1);
				CopyDescriptor(device, pf.buffers_heap, static_instances_in_node, 2);
				CopyDescriptor(device, pf.buffers_heap, meshes_buff_, 3);

				ID3D12DescriptorHeap* heap_ptr = pf.buffers_heap.get_heap();
				command_list_->SetDescriptorHeaps(1, &heap_ptr);
			}
			uint32_t buffer_heap_offset = 4;

			auto execute = [&](SceneManagerPass& pass)
			{
				UavCountedBuffer& uav = pf.GetUav(pass.uav);
				UavCountedBuffer* srv = (pass.srv != E_SRV::Invalid) ? &pf.GetSrv(pass.srv) : nullptr;
				if (uav.get_state() != D3D12_RESOURCE_STATE_COPY_DEST)	pf.tm.insert(uav.transition_barrier(D3D12_RESOURCE_STATE_COPY_DEST));
				if (srv)												pf.tm.insert(srv->transition_barrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
				pf.tm.start();
				pf.tm.wait_for(uav.get_resource());
				uav.reset_counter(command_list_.Get(), reset_counter_src_.get_resource(), 0);
				pf.tm.wait_for(uav.transition_barrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS), srv ? srv->get_resource() : nullptr);

				uint32_t local_heap_offset = buffer_heap_offset;
				CopyDescriptor(device, pf.buffers_heap, uav.get_uav_handle(), local_heap_offset++);
				if (srv)
				{
					CopyDescriptor(device, pf.buffers_heap, srv->get_counter_srv_handle(), local_heap_offset++);
					CopyDescriptor(device, pf.buffers_heap, srv->get_srv_handle(), local_heap_offset++);
				}

				command_list_->SetComputeRootSignature(pass.root_signature_.Get());
				command_list_->SetComputeRootConstantBufferView(0, pf.scene_manager_params.get_resource()->GetGPUVirtualAddress());
				const uint32_t static_buffers_offset = 0;
				command_list_->SetComputeRootDescriptorTable(1, pf.buffers_heap.get_gpu_handle(static_buffers_offset));
				command_list_->SetComputeRootDescriptorTable(2, pf.buffers_heap.get_gpu_handle(buffer_heap_offset));
				buffer_heap_offset = local_heap_offset;

				command_list_->SetPipelineState(pass.pipeline_state_.Get());
				command_list_->Dispatch(pass.max_dispatch / pass.k_threads_x, 1, 1);
			};
			execute(filter_nodes_frustum_);
			execute(filter_nodes_depth_);
			execute(extract_instances_);
			execute(filter_inst_frustum_);
			execute(filter_inst_depth_);
			execute(generate_draw_commands_);

			//DRAW
			{
				pf.tm.insert(pf.indirect_draw_commands.transition_barrier(D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));
				pf.tm.start();
				command_list_->SetPipelineState(render_state_.Get());
				command_list_->SetGraphicsRootSignature(draw_root_signature_.Get());
				pf.tm.wait_for_all_started();
				PrepareCommonDraw(command_list_.Get());
				command_list_->ExecuteIndirect(command_signature_.Get(), pf.indirect_draw_commands.elements_num()
					, pf.indirect_draw_commands.get_resource(), 0
					, pf.indirect_draw_commands.get_resource(), pf.indirect_draw_commands.get_counter_offset());
			}

			//EXECUTE
			{
				pf.tm.insert(pf.scene_manager_params.transition_barrier(D3D12_RESOURCE_STATE_COPY_DEST));
				pf.tm.insert(pf.indirect_draw_commands.transition_barrier(D3D12_RESOURCE_STATE_COPY_DEST));
				pf.tm.insert(pf.temp_buffer_1.transition_barrier(D3D12_RESOURCE_STATE_COPY_DEST));
				pf.tm.insert(pf.temp_buffer_2.transition_barrier(D3D12_RESOURCE_STATE_COPY_DEST));
				pf.tm.wait_for(CD3DX12_RESOURCE_BARRIER::Transition(GetRTResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
				ThrowIfFailed(command_list_->Close());
				Execute(command_list_.Get());
			}
		};
		
		Draw();
		Present();

		sync.StartSync(direct_command_queue_);
		{
			const auto new_time = Utils::GetTime();
			const Utils::TimeSpan delta = new_time - time;
			app.ReceiveMsgToBroadcast(CommonMsg::Frame{ frame_counter, delta });
			time = new_time;
			frame_counter++;
		}
	}

	void InitializeDrawPipeline()
	{
		{
			RootSignature builder(2, 0);
			builder[0].InitAsConstants(0, 16, D3D12_SHADER_VISIBILITY_VERTEX);
			builder[1].InitAsConstants(0, 3, D3D12_SHADER_VISIBILITY_PIXEL);
			draw_root_signature_ = builder.Finalize(common_.device.Get(), GetRootSignatureVersion(),
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
			NAME_D3D12_OBJECT(draw_root_signature_);
		}

		{
			CommandSignature builder(5);
			builder[0].Constant(0, 0, 16);//World Matrix
			builder[1].IndexBufferView();
			builder[2].VertexBufferView(0);
			builder[3].Constant(1, 0, 3);// Material Constants
			builder[4].DrawIndexed();
			command_signature_ = builder.Finalize(common_.device.Get(), draw_root_signature_.Get());
			assert(sizeof(IndirectCommandGPU) == builder.GetByteStride());
			NAME_D3D12_OBJECT(command_signature_);
		}

		{
			D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			GraphicsPSO builder;
			builder.SetRootSignature(draw_root_signature_.Get());
			builder.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
			builder.SetVertexShader(g_draw_vs, sizeof(g_draw_vs));
			builder.SetPixelShader(g_draw_ps, sizeof(g_draw_ps));
			builder.SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN);
			builder.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			builder.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
			builder.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
			render_state_ = builder.Finalize(common_.device.Get());
			NAME_D3D12_OBJECT(render_state_);
		}
	}

	void InitializeScenePasses()
	{
		ID3D12Device* const device = common_.device.Get();
		const D3D_ROOT_SIGNATURE_VERSION rs_version = GetRootSignatureVersion();
		auto create_sm_pass = [device, rs_version](SceneManagerPass& pass, const uint8_t* shader_bytes, std::size_t size)
		{
			RootSignature rs_builder(3, 0);
			rs_builder[0].InitAsConstantBuffer(0);
			rs_builder[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
			if (pass.srv != E_SRV::Invalid)
			{
				rs_builder[2].InitAsDescriptorTable(2);
				rs_builder[2].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
				rs_builder[2].SetTableRange(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 2);
			}
			else
			{
				rs_builder[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
			}
			pass.root_signature_ = rs_builder.Finalize(device, rs_version);

			ComputePSO builder;
			builder.SetComputeShader(shader_bytes, size);
			builder.SetRootSignature(pass.root_signature_.Get());
			pass.pipeline_state_ = builder.Finalize(device);
		};

		create_sm_pass(filter_nodes_frustum_, g_filter_nodes_frustum_cs, sizeof(g_filter_nodes_frustum_cs));
		NAME_D3D12_OBJECT(filter_nodes_frustum_.pipeline_state_);
		NAME_D3D12_OBJECT(filter_nodes_frustum_.root_signature_);
		create_sm_pass(filter_nodes_depth_, g_filter_nodes_depth_cs, sizeof(g_filter_nodes_depth_cs));
		NAME_D3D12_OBJECT(filter_nodes_depth_.pipeline_state_);
		NAME_D3D12_OBJECT(filter_nodes_depth_.root_signature_);
		create_sm_pass(extract_instances_, g_extract_instances_cs, sizeof(g_extract_instances_cs));
		NAME_D3D12_OBJECT(extract_instances_.pipeline_state_);
		NAME_D3D12_OBJECT(extract_instances_.root_signature_);
		create_sm_pass(filter_inst_frustum_, g_filter_inst_frustum_cs, sizeof(g_filter_inst_frustum_cs));
		NAME_D3D12_OBJECT(filter_inst_frustum_.pipeline_state_);
		NAME_D3D12_OBJECT(filter_inst_frustum_.root_signature_);
		create_sm_pass(filter_inst_depth_, g_filter_inst_depth_cs, sizeof(g_filter_inst_depth_cs));
		NAME_D3D12_OBJECT(filter_inst_depth_.pipeline_state_);
		NAME_D3D12_OBJECT(filter_inst_depth_.root_signature_);
		create_sm_pass(generate_draw_commands_, g_generate_draw_commands_cs, sizeof(g_generate_draw_commands_cs));
		NAME_D3D12_OBJECT(generate_draw_commands_.pipeline_state_);
		NAME_D3D12_OBJECT(generate_draw_commands_.root_signature_);
	}

	void ThreadInitialize() override
	{
		InitializeDrawPipeline();
		InitializeScenePasses();

		const uint32_t views_per_frame = 16;
		persistent_descriptor_heap_.create(common_.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, views_per_frame * Const::kFrameCount);

		ThrowIfFailed(common_.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, GetActiveAllocator(), nullptr, IID_PPV_ARGS(&command_list_)));
		NAME_D3D12_OBJECT(command_list_);

		for (auto& pf : per_frame_)
		{
			pf.upload_buffer.initialize(common_.device.Get(), 1024);
			pf.buffers_heap.create(common_.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 64);
			Construct<IndirectCommandGPU, UavCountedBuffer>(pf.indirect_draw_commands, nullptr, Const::kStaticInstancesCapacity, common_.device.Get(), command_list_.Get()
				, pf.upload_buffer, &persistent_descriptor_heap_, { D3D12_RESOURCE_STATE_COPY_DEST });
			NAME_D3D12_BUFFER(pf.indirect_draw_commands);
			Construct<SceneManagerParamsGPU, ConstantBuffer>(pf.scene_manager_params, nullptr, 1, common_.device.Get(), command_list_.Get()
				, pf.upload_buffer, &persistent_descriptor_heap_, { D3D12_RESOURCE_STATE_COPY_DEST });
			NAME_D3D12_BUFFER(pf.scene_manager_params);
			Construct<InstancesInNodeGPU::TIndex, UavCountedBuffer>(pf.temp_buffer_1, nullptr, Const::kStaticInstancesCapacity, common_.device.Get(), command_list_.Get()
				, pf.upload_buffer, &persistent_descriptor_heap_, { D3D12_RESOURCE_STATE_COPY_DEST });
			NAME_D3D12_BUFFER(pf.temp_buffer_1);
			Construct<InstancesInNodeGPU::TIndex, UavCountedBuffer>(pf.temp_buffer_2, nullptr, Const::kStaticInstancesCapacity, common_.device.Get(), command_list_.Get()
				, pf.upload_buffer, &persistent_descriptor_heap_, { D3D12_RESOURCE_STATE_COPY_DEST });
			NAME_D3D12_BUFFER(pf.temp_buffer_2);
		}

		const uint32_t zero_val = 0;
		Construct<uint32_t>(reset_counter_src_, &zero_val, 1, common_.device.Get(), command_list_.Get()
			, per_frame_[0].upload_buffer, nullptr, D3D12_RESOURCE_STATE_COPY_SOURCE);
		NAME_D3D12_BUFFER(reset_counter_src_);

		ThrowIfFailed(command_list_->Close());
		Execute(command_list_.Get());
		
		BaseRenderer::ThreadInitialize();

		for (auto& pf : per_frame_)
		{
			pf.upload_buffer.reset();
		}

		time = Utils::GetTime();
	}

	void ThreadCleanUp() override
	{
		BaseRenderer::ThreadCleanUp();

		command_list_.Reset();
		command_signature_.Reset();
		render_state_.Reset();

		filter_nodes_frustum_.destroy();
		filter_nodes_depth_.destroy();
		extract_instances_.destroy();
		filter_inst_frustum_.destroy();
		filter_inst_depth_.destroy();
		generate_draw_commands_.destroy();

		for (auto& pf : per_frame_)
		{
			pf.buffers_heap.destroy();
			pf.indirect_draw_commands.destroy();
			pf.scene_manager_params.destroy();
			pf.upload_buffer.destroy();
			pf.temp_buffer_1.destroy();
			pf.temp_buffer_2.destroy();
			pf.tm.clear();
		}
		persistent_descriptor_heap_.destroy();
		reset_counter_src_.destroy();
	}

	std::string_view GetName() const override { return "Renderer"; }
};

const RendererCommon& IRenderer::GetRendererCommon() { return BaseRenderer::GetCommon(); }

void IRenderer::EnqueueMsg(RT_MSG&& msg) { BaseRenderer::StaticEnqueueMsg(std::forward<RT_MSG>(msg)); }

IBaseSystem* IRenderer::CreateSystem(HWND hWnd, uint32_t width, uint32_t height, BaseApp& app) { return new Renderer(hWnd, width, height, app); }
