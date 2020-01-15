#include "stdafx.h"
#include "../renderer_interface.h"
#include "utils/base_app_helper.h"
#include "primitives/mesh_data.h"
#include "utils/graphics/command_signature.h"
#include "utils/graphics/root_signature.h"
#include "utils/graphics/pipeline_state.h"
#include <cstddef>

#include "base_renderer.h"

#ifdef NDEBUG
#include "compiled_shaders/release/draw_ps.h"
#include "compiled_shaders/release/draw_vs.h"
#include "compiled_shaders/release/generate_draw_commands_cs.h"
#else
#include "compiled_shaders/debug/draw_ps.h"
#include "compiled_shaders/debug/draw_vs.h"
#include "compiled_shaders/debug/generate_draw_commands_cs.h"
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

struct Pass
{
	ComPtr<ID3D12RootSignature> root_signature_;
	ComPtr<ID3D12PipelineState> pipeline_state_;

	void Destroy()
	{
		root_signature_.Reset();
		pipeline_state_.Reset();
	}
};

struct Descriptors
{
	constexpr static uint32_t indirect_draw_commands_uav = 0;
	constexpr static uint32_t meshes_buff_srv = 1;
	constexpr static uint32_t static_nodes_srv = 2;
	constexpr static uint32_t static_instances_srv = 3;
	constexpr static uint32_t scene_manager_params_cbv = 4;
	constexpr static uint32_t total_num = 5;
};

class Renderer : public BaseRenderer
{
	ComPtr<ID3D12GraphicsCommandList> command_list_;
	ComPtr<ID3D12CommandSignature> command_signature_;

	//ComPtr<ID3D12PipelineState> state_filter_frustum_nodes_;		// CS:						-> filtered_nodes_
	//ComPtr<ID3D12PipelineState> state_filter_depth_nodes_;		// CS: filtered_nodes_		-> temp_indexes_
	//ComPtr<ID3D12PipelineState> state_filter_frustum_instances_;	// CS: temp_indexes_		-> filtered_instances_
	//ComPtr<ID3D12PipelineState> state_filter_depth_instances_;	// CS: filtered_instances_	-> temp_indexes_
	Pass generate_draw_commands_;		// CS: temp_indexes_		-> indirect_draw_commands_
	Pass render_;

	DescriptorHeap persistent_descriptor_heap_;
	CommitedBuffer reset_counter_src_;

	struct PerFrame
	{
		UploadBuffer upload_buffer;
		DescriptorHeap buffers_heap;
		ConstantBuffer scene_manager_params;
		UavCountedBuffer indirect_draw_commands;
	};
	std::array<PerFrame, Const::kFrameCount> per_frame_;

	PerFrame& GetPerFrame() { return per_frame_[GetFrameIndex()]; }

	DescriptorHeapElementRef meshes_buff_;
	DescriptorHeapElementRef static_nodes_;
	DescriptorHeapElementRef static_instances_;

	std::vector<std::shared_ptr<Mesh>> to_register_;

public:
	Renderer(HWND hWnd, uint32_t width, uint32_t height) : BaseRenderer(hWnd, width, height) {}

private:
	void RegisterMeshes()
	{
		if (!to_register_.size())
			return;
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		barriers.reserve(2 * to_register_.size());
		for (auto& mesh : to_register_)
		{
			assert(mesh);
			barriers.push_back(mesh->index_buffer.transition_barrier(D3D12_RESOURCE_STATE_INDEX_BUFFER));
			barriers.push_back(mesh->vertex_buffer.transition_barrier(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
		}
		to_register_.clear();

		assert(command_list_);
		command_list_->ResourceBarrier(static_cast<uint32_t>(barriers.size()), &barriers[0]);
	}

	void GenerateCommands()
	{
		auto& pf = GetPerFrame();

		SceneManagerParamsGPU params;
		const bool params_ok = FillGpuContainer<SceneManagerParamsGPU, ConstantBuffer>(
			pf.scene_manager_params, &params, 1, command_list_.Get(), pf.upload_buffer);
		assert(params_ok);

		pf.indirect_draw_commands.reset_counter(command_list_.Get(), reset_counter_src_.get_resource(), 0);

		D3D12_RESOURCE_BARRIER barriers[] = { pf.indirect_draw_commands.transition_barrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS) };
		command_list_->ResourceBarrier(_countof(barriers), barriers);

		ID3D12DescriptorHeap* heap_ptr = pf.buffers_heap.get_heap();
		command_list_->SetDescriptorHeaps(1, &heap_ptr);
		command_list_->SetComputeRootSignature(generate_draw_commands_.root_signature_.Get());
		command_list_->SetComputeRootDescriptorTable(0, pf.buffers_heap.get_gpu_handle(0));




		command_list_->SetPipelineState(generate_draw_commands_.pipeline_state_.Get());
		command_list_->Dispatch(1, 1, 1);
	}

	void DrawPass()
	{
		auto& pf = GetPerFrame();

		command_list_->SetPipelineState(render_.pipeline_state_.Get());
		command_list_->SetGraphicsRootSignature(render_.root_signature_.Get());

		D3D12_RESOURCE_BARRIER barriers[] = { pf.indirect_draw_commands.transition_barrier(D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT) };
		command_list_->ResourceBarrier(_countof(barriers), barriers);

		BeforeCommonDraw(command_list_.Get());

		command_list_->ExecuteIndirect(command_signature_.Get(), pf.indirect_draw_commands.elements_num()
			, pf.indirect_draw_commands.get_resource(), 0
			, pf.indirect_draw_commands.get_resource(), pf.indirect_draw_commands.get_counter_offset());

		AfterCommonDraw(command_list_.Get());

		barriers[0] = pf.indirect_draw_commands.transition_barrier(D3D12_RESOURCE_STATE_COPY_DEST);
		command_list_->ResourceBarrier(_countof(barriers), barriers);
	}

protected:
	void operator()(RT_MSG_UpdateCamera) {}

	void operator()(RT_MSG_MeshBuffer msg)
	{
		meshes_buff_ = msg.meshes_buff;
	}

	void operator()(RT_MSG_StaticBuffers msg)
	{
		static_nodes_ = msg.static_nodes;
		static_instances_ = msg.static_instances;
		msg.promise_previous_nodes_not_used.set_value(PrevFrameSync());
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
		if (!meshes_buff_ || !static_nodes_ || !static_instances_)
			return;

		auto update_descriptors_heap = [&]()
		{
			auto& pf = GetPerFrame();
			pf.buffers_heap.clear(Descriptors::total_num);
			auto device = common_.device.Get();
			CopyDescriptor(device, pf.buffers_heap, pf.indirect_draw_commands.get_uav_handle(), Descriptors::indirect_draw_commands_uav);
			CopyDescriptor(device, pf.buffers_heap, meshes_buff_, Descriptors::meshes_buff_srv);
			CopyDescriptor(device, pf.buffers_heap, static_nodes_, Descriptors::static_nodes_srv);
			CopyDescriptor(device, pf.buffers_heap, static_instances_, Descriptors::static_instances_srv);
			CopyDescriptor(device, pf.buffers_heap, pf.scene_manager_params.get_cbv_handle(), Descriptors::scene_manager_params_cbv);
		};
		update_descriptors_heap();

		ID3D12CommandAllocator* const allocator = GetActiveAllocator();
		ThrowIfFailed(allocator->Reset());
		ThrowIfFailed(command_list_->Reset(allocator, nullptr));

		RegisterMeshes();
		GenerateCommands();
		DrawPass();

		ThrowIfFailed(command_list_->Close());

		Execute(command_list_.Get());
		Present();
		MoveToNextFrame();
	}

	void ThreadInitialize() override
	{
		{
			RootSignature builder(2, 0);
			builder[0].InitAsConstants(0, 16, D3D12_SHADER_VISIBILITY_VERTEX);
			builder[1].InitAsConstants(0, 3, D3D12_SHADER_VISIBILITY_PIXEL);
			render_.root_signature_ = builder.Finalize(common_.device.Get(), GetRootSignatureVersion(),
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
			NAME_D3D12_OBJECT(render_.root_signature_);
		}

		{
			CommandSignature builder(5);
			builder[0].Constant(0, 0, 16);//World Matrix
			builder[1].IndexBufferView();
			builder[2].VertexBufferView(0);
			builder[3].Constant(1, 0, 3);// Material Constants
			builder[4].DrawIndexed();
			command_signature_ = builder.Finalize(common_.device.Get(), render_.root_signature_.Get());
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
			builder.SetRootSignature(render_.root_signature_.Get());
			builder.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
			builder.SetVertexShader(g_draw_vs, sizeof(g_draw_vs));
			builder.SetPixelShader(g_draw_ps, sizeof(g_draw_ps));
			builder.SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN);
			builder.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			builder.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
			builder.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
			render_.pipeline_state_ = builder.Finalize(common_.device.Get());
			NAME_D3D12_OBJECT(render_.pipeline_state_);
		}

		{
			RootSignature builder(1, 0);
			builder[0].InitAsDescriptorTable(3);
			builder[0].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
			builder[0].SetTableRange(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3);
			builder[0].SetTableRange(2, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1);
			generate_draw_commands_.root_signature_ = builder.Finalize(common_.device.Get(), GetRootSignatureVersion());
			NAME_D3D12_OBJECT(generate_draw_commands_.root_signature_);
		}

		{
			ComputePSO builder;
			builder.SetComputeShader(g_generate_draw_commands_cs, sizeof(g_generate_draw_commands_cs));
			builder.SetRootSignature(generate_draw_commands_.root_signature_.Get());
			generate_draw_commands_.pipeline_state_ = builder.Finalize(common_.device.Get());
			NAME_D3D12_OBJECT(generate_draw_commands_.pipeline_state_);
		}

		persistent_descriptor_heap_.create(common_.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, Descriptors::total_num * Const::kFrameCount);

		ThrowIfFailed(common_.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, GetActiveAllocator(), nullptr, IID_PPV_ARGS(&command_list_)));
		NAME_D3D12_OBJECT(command_list_);

		for (auto& pf : per_frame_)
		{
			pf.upload_buffer.initialize(common_.device.Get(), 1024);
			pf.buffers_heap.create(common_.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, Descriptors::total_num);
			Construct<IndirectCommandGPU, UavCountedBuffer>(pf.indirect_draw_commands, nullptr, Const::kStaticInstancesCapacity
				, common_.device.Get(), command_list_.Get(), pf.upload_buffer, &persistent_descriptor_heap_, { D3D12_RESOURCE_STATE_COPY_DEST });
			Construct<SceneManagerParamsGPU, ConstantBuffer>(pf.scene_manager_params, nullptr, 1, common_.device.Get(), command_list_.Get()
				, pf.upload_buffer, &persistent_descriptor_heap_);
		}

		const uint32_t zero_val = 0;
		Construct<uint32_t>(reset_counter_src_, &zero_val, 1, common_.device.Get(), command_list_.Get()
			, per_frame_[0].upload_buffer, nullptr, D3D12_RESOURCE_STATE_COPY_SOURCE);

		ThrowIfFailed(command_list_->Close());
		Execute(command_list_.Get());
		
		BaseRenderer::ThreadInitialize();

		for (auto& pf : per_frame_)
		{
			pf.upload_buffer.reset();
		}
	}

	void ThreadCleanUp() override
	{
		BaseRenderer::ThreadCleanUp();

		command_list_.Reset();
		command_signature_.Reset();

		render_.Destroy();
		generate_draw_commands_.Destroy();

		for (auto& pf : per_frame_)
		{
			pf.buffers_heap.destroy();
			pf.indirect_draw_commands.destroy();
			pf.scene_manager_params.destroy();
			pf.upload_buffer.destroy();
		}
		persistent_descriptor_heap_.destroy();
	}
};

const RendererCommon& IRenderer::GetRendererCommon() { return BaseRenderer::GetCommon(); }
void IRenderer::EnqueueMsg(RT_MSG&& msg) { BaseRenderer::StaticEnqueueMsg(std::forward<RT_MSG>(msg)); }
IBaseSystem* IRenderer::CreateSystem(HWND hWnd, uint32_t width, uint32_t height) { return new Renderer(hWnd, width, height); }
