#include "stdafx.h"
#include "../renderer_interface.h"
#include "utils/base_app_helper.h"
#include "primitives/mesh_data.h"
#include "utils/graphics/command_signature.h"
#include "utils/graphics/root_signature.h"
#include "utils/graphics/pipeline_state.h"
#include <cstddef>
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

class BaseRenderer;
static BaseRenderer* renderer_inst = nullptr;

class BaseRenderer : public BaseSystemImpl<RT_MSG>
{
protected:
	RendererCommon common_;

public:
	BaseRenderer(HWND hWnd, uint32_t width, uint32_t height)
	{ 
		assert(!renderer_inst); 
		renderer_inst = this; 

		common_.hwnd = hWnd;
		common_.width = width;
		common_.height = height;
		common_.viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
		common_.scissor_rect = CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));
	}
	virtual ~BaseRenderer() { assert( renderer_inst); renderer_inst = nullptr; }
	const RendererCommon& GetCommon() const { return common_; }

	//HELPERS:
	static void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
	{
		ComPtr<IDXGIAdapter1> adapter;
		*ppAdapter = nullptr;

		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				// If you want a software adapter, pass in "/warp" on the command line.
				continue;
			}

			// Check to see if the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}

		*ppAdapter = adapter.Detach();
	}

protected:
	void SetupFullscreen(const bool new_fullscreen)
	{
		const UINT window_style = WS_OVERLAPPEDWINDOW;
		if (!new_fullscreen)
		{
			// Restore the window's attributes and size.
			SetWindowLong(common_.hwnd, GWL_STYLE, window_style);

			SetWindowPos(
				common_.hwnd,
				HWND_NOTOPMOST,
				common_.window_rect.left,
				common_.window_rect.top,
				common_.window_rect.right - common_.window_rect.left,
				common_.window_rect.bottom - common_.window_rect.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);

			ShowWindow(common_.hwnd, SW_NORMAL);
		}
		else
		{
			// Save the old window rect so we can restore it when exiting fullscreen mode.
			GetWindowRect(common_.hwnd, &common_.window_rect);

			// Make the window borderless so that the client area can fill the screen.
			SetWindowLong(common_.hwnd, GWL_STYLE, window_style & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

			RECT fullscreen_rect;
			try
			{
				if (common_.swap_chain)
				{
					// Get the settings of the display on which the app's window is currently displayed
					ComPtr<IDXGIOutput> output;
					ThrowIfFailed(common_.swap_chain->GetContainingOutput(&output));
					DXGI_OUTPUT_DESC desc;
					ThrowIfFailed(output->GetDesc(&desc));
					fullscreen_rect = desc.DesktopCoordinates;
				}
				else
				{
					// Fallback to EnumDisplaySettings implementation
					throw HrException(S_FALSE);
				}
			}
			catch (HrException & e)
			{
				UNREFERENCED_PARAMETER(e);

				// Get the settings of the primary display
				DEVMODE devMode = {};
				devMode.dmSize = sizeof(DEVMODE);
				EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);

				fullscreen_rect = {
					devMode.dmPosition.x,
					devMode.dmPosition.y,
					devMode.dmPosition.x + static_cast<LONG>(devMode.dmPelsWidth),
					devMode.dmPosition.y + static_cast<LONG>(devMode.dmPelsHeight)
				};
			}

			SetWindowPos(
				common_.hwnd,
				HWND_TOPMOST,
				fullscreen_rect.left,
				fullscreen_rect.top,
				fullscreen_rect.right,
				fullscreen_rect.bottom,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);
			ShowWindow(common_.hwnd, SW_MAXIMIZE);
		}
		common_.fullscreen = new_fullscreen;
	}

	void WaitForGpu()
	{
		uint64_t& fence_value = common_.fence_values[common_.frame_index];
		ThrowIfFailed(common_.direct_command_queue->Signal(common_.fence.Get(), fence_value));

		// Wait until the fence has been processed.
		ThrowIfFailed(common_.fence->SetEventOnCompletion(fence_value, common_.fence_event));
		WaitForSingleObjectEx(common_.fence_event, INFINITE, FALSE);

		// Increment the fence value for the current frame.
		fence_value++;
	}

	void MoveToNextFrame()
	{
		const uint64_t current_fence_value = common_.fence_values[common_.frame_index];
		ThrowIfFailed(common_.direct_command_queue->Signal(common_.fence.Get(), current_fence_value));

		common_.frame_index = common_.swap_chain->GetCurrentBackBufferIndex();
		uint64_t& next_fence_value = common_.fence_values[common_.frame_index];
		if (common_.fence->GetCompletedValue() < next_fence_value)
		{
			ThrowIfFailed(common_.fence->SetEventOnCompletion(next_fence_value, common_.fence_event));
			WaitForSingleObjectEx(common_.fence_event, INFINITE, FALSE);
		}

		next_fence_value = current_fence_value + 1;
	}

	void ThreadInitialize() override 
	{
		ThrowIfFailed(common_.device->CreateFence(common_.fence_values[common_.frame_index], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&common_.fence)));
		NAME_D3D12_OBJECT(common_.fence);
		common_.fence_values[common_.frame_index]++;

		// Create an event handle to use for frame synchronization.
		common_.fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (common_.fence_event == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForGpu();
	}

	void ThreadCleanUp() override 
	{
		WaitForGpu();
		CloseHandle(common_.fence_event);
	}

	//Executed on main thread
	void CustomOpen() override 
	{
		UINT dxgi_flags = 0;
#if defined(_DEBUG)
		{
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				dxgi_flags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}
#endif
		ComPtr<IDXGIFactory4> factory;
		ThrowIfFailed(CreateDXGIFactory2(dxgi_flags, IID_PPV_ARGS(&factory)));

		{
			ComPtr<IDXGIFactory6> factory6;
			if (SUCCEEDED(factory.As(&factory6)))
			{
				BOOL allow_tearing = FALSE;
				HRESULT hr = factory6->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
				common_.tearing_supported = SUCCEEDED(hr) && allow_tearing;
			}
		}

		{
			ComPtr<IDXGIAdapter1> adapter;
			GetHardwareAdapter(factory.Get(), &adapter);
			ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&common_.device)));
			NAME_D3D12_OBJECT(common_.device);
		}

		{
			D3D12_COMMAND_QUEUE_DESC direct_queue_desc = {};
			direct_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			direct_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			ThrowIfFailed(common_.device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&common_.direct_command_queue)));
			NAME_D3D12_OBJECT(common_.direct_command_queue);
		}

		{
			DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
			swap_chain_desc.BufferCount = Const::kFrameCount;
			swap_chain_desc.Width = common_.width;
			swap_chain_desc.Height = common_.height;
			swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swap_chain_desc.SampleDesc.Count = 1;

			ComPtr<IDXGISwapChain1> swap_chain;
			ThrowIfFailed(factory->CreateSwapChainForHwnd(common_.direct_command_queue.Get(), common_.hwnd, &swap_chain_desc, nullptr, nullptr, &swap_chain));
			ThrowIfFailed(factory->MakeWindowAssociation(common_.hwnd, DXGI_MWA_NO_ALT_ENTER));
			ThrowIfFailed(swap_chain.As(&common_.swap_chain));
		}

		common_.feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(common_.device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &common_.feature_data, sizeof(common_.feature_data))))
		{
			common_.feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		common_.frame_index = common_.swap_chain->GetCurrentBackBufferIndex();
		{
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = Const::kFrameCount;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(common_.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&common_.rtv_heap)));
			NAME_D3D12_OBJECT(common_.rtv_heap);
			common_.rtv_descriptor_size = common_.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		// Create frame resources.
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(common_.rtv_heap->GetCPUDescriptorHandleForHeapStart());
			for (UINT n = 0; n < Const::kFrameCount; n++)
			{
				ThrowIfFailed(common_.swap_chain->GetBuffer(n, IID_PPV_ARGS(&common_.render_targets[n])));
				common_.device->CreateRenderTargetView(common_.render_targets[n].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, common_.rtv_descriptor_size);

				ThrowIfFailed(common_.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&common_.command_allocators[n])));
				NAME_D3D12_OBJECT_INDEXED(common_.command_allocators, n);
				NAME_D3D12_OBJECT_INDEXED(common_.render_targets, n);
			}
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.NumDescriptors = 1;
			dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(common_.device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&common_.dsv_heap)));
			NAME_D3D12_OBJECT(common_.dsv_heap);
			common_.dsv_descriptor_size = common_.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		}

		{
			D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
			depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
			depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

			D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
			depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
			depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			depthOptimizedClearValue.DepthStencil.Stencil = 0;

			const CD3DX12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE_DEFAULT);
			const CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT
				, common_.width, common_.height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
			ThrowIfFailed(common_.device->CreateCommittedResource(&heap_props
				, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue
				, IID_PPV_ARGS(&common_.depth_stencil)));

			NAME_D3D12_OBJECT(common_.depth_stencil);

			common_.device->CreateDepthStencilView(common_.depth_stencil.Get(), &depthStencilDesc, common_.dsv_heap->GetCPUDescriptorHandleForHeapStart());
		}
	}

	void CustomClose() override 
	{
		common_.fence.Reset();
		common_.depth_stencil.Reset();
		common_.dsv_heap.Reset();
		for (UINT n = 0; n < Const::kFrameCount; n++) 
		{ 
			common_.render_targets[n].Reset(); 
			common_.command_allocators[n].Reset();
		}
		common_.rtv_heap.Reset();
		common_.swap_chain.Reset();
		common_.direct_command_queue.Reset();
	}

	void Destroy() override 
	{
		if (common_.swap_chain && !common_.tearing_supported)
		{
			ThrowIfFailed(common_.swap_chain->SetFullscreenState(FALSE, nullptr));
		}
	}
};

struct IndirectCommandGPU
{
	XMFLOAT4X4 wvp_mtx;
	D3D12_INDEX_BUFFER_VIEW index_buffer;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer;
	XMUINT2 texture_index;
	float mat_val;
	D3D12_DRAW_INDEXED_ARGUMENTS draw_arg;
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
	constexpr static uint32_t static_instances_srv = 2;
	constexpr static uint32_t static_nodes_srv = 3;
	constexpr static uint32_t total_num = 4;
};

class Renderer : public BaseRenderer
{
protected:

	ComPtr<ID3D12GraphicsCommandList> command_list_;
	ComPtr<ID3D12CommandSignature> command_signature_;

	//ComPtr<ID3D12PipelineState> state_filter_frustum_nodes_;		// CS:						-> filtered_nodes_
	//ComPtr<ID3D12PipelineState> state_filter_depth_nodes_;		// CS: filtered_nodes_		-> temp_indexes_
	//ComPtr<ID3D12PipelineState> state_filter_frustum_instances_;	// CS: temp_indexes_		-> filtered_instances_
	//ComPtr<ID3D12PipelineState> state_filter_depth_instances_;	// CS: filtered_instances_	-> temp_indexes_
	Pass generate_draw_commands_;		// CS: temp_indexes_		-> indirect_draw_commands_
	Pass render_;

	DescriptorHeap persistent_descriptor_heap_;
	Twins<DescriptorHeap> buffers_heaps_;
	UploadBuffer upload_buffer_;
	//UavCountedBuffer filtered_nodes_; //full nodes
	//UavCountedBuffer filtered_instances_; // instance_idx
	//UavCountedBuffer temp_indexes_; //filtered_node' and instance_idx'
	UavCountedBuffer indirect_draw_commands_;
	CommitedBuffer reset_counter_src_;

	std::vector<std::shared_ptr<Mesh>> to_register_;

	DescriptorHeapElementRef meshes_buff_;
	DescriptorHeapElementRef static_nodes_;
	DescriptorHeapElementRef static_instances_;

	bool need_update_descriptors_ = false;
public:
	Renderer(HWND hWnd, uint32_t width, uint32_t height) : BaseRenderer(hWnd, width, height) {}

protected:
	void operator()(RT_MSG_UpdateCamera) {}

	void operator()(RT_MSG_MeshBuffer msg)
	{
		meshes_buff_ = msg.meshes_buff;
		need_update_descriptors_ = true;
	}

	void operator()(RT_MSG_StaticBuffers msg)
	{
		static_nodes_ = msg.static_nodes;
		static_instances_ = msg.static_instances;
		need_update_descriptors_ = true;

		const uint64_t current_frame_num = common_.fence_values[common_.frame_index];
		assert(current_frame_num);
		const uint64_t necessary_fence_value = current_frame_num - 1;
		msg.promise_previous_nodes_not_used.set_value({ common_.fence, necessary_fence_value });
	}

	void operator()(RT_MSG_ToogleFullScreen msg)
	{
		const bool new_fullscreen = msg.forced_mode ? *msg.forced_mode : !common_.fullscreen;
		SetupFullscreen(new_fullscreen);
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
		indirect_draw_commands_.reset_counter(command_list_.Get(), reset_counter_src_.get_resource(), 0);

		D3D12_RESOURCE_BARRIER barriers[] = { indirect_draw_commands_.transition_barrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS) };
		command_list_->ResourceBarrier(_countof(barriers), barriers);

		auto& desc_heap = buffers_heaps_.GetActive();
		ID3D12DescriptorHeap* heap_ptr = desc_heap.get_heap();
		command_list_->SetDescriptorHeaps(1, &heap_ptr);
		command_list_->SetPipelineState(generate_draw_commands_.pipeline_state_.Get());
		command_list_->SetComputeRootSignature(generate_draw_commands_.root_signature_.Get());
		command_list_->SetComputeRoot32BitConstant(0, 1, 0);
		command_list_->SetComputeRootDescriptorTable(1, desc_heap.get_gpu_handle(0));
		
		//command_list_->SetComputeRootShaderResourceView(1, static_instances->GetGPUVirtualAddress());
		//command_list_->SetComputeRootShaderResourceView(2, meshes_buff->GetGPUVirtualAddress());
		//command_list_->SetComputeRootUnorderedAccessView(3, indirect_draw_commands_.get_resource()->GetGPUVirtualAddress());
		command_list_->Dispatch(1, 1, 1);
	}

	void DrawPass()
	{
		command_list_->SetPipelineState(render_.pipeline_state_.Get());
		command_list_->SetGraphicsRootSignature(render_.root_signature_.Get());
		command_list_->RSSetViewports(1, &common_.viewport);
		command_list_->RSSetScissorRects(1, &common_.scissor_rect);

		D3D12_RESOURCE_BARRIER barriers[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition(common_.render_targets[common_.frame_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
			indirect_draw_commands_.transition_barrier(D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT) };
		command_list_->ResourceBarrier(_countof(barriers), barriers);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(common_.rtv_heap->GetCPUDescriptorHandleForHeapStart(), common_.frame_index, common_.rtv_descriptor_size);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(common_.dsv_heap->GetCPUDescriptorHandleForHeapStart());
		command_list_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		command_list_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		command_list_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		command_list_->ExecuteIndirect(command_signature_.Get(), indirect_draw_commands_.elements_num()
			, indirect_draw_commands_.get_resource(), 0
			, indirect_draw_commands_.get_resource(), indirect_draw_commands_.get_counter_offset());

		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		barriers[1] = indirect_draw_commands_.transition_barrier(D3D12_RESOURCE_STATE_COPY_DEST);
		command_list_->ResourceBarrier(_countof(barriers), barriers);
	}

	void PopulateCommandList()
	{
		ID3D12CommandAllocator* const allocator = common_.command_allocators[common_.frame_index].Get();
		ThrowIfFailed(allocator->Reset());
		ThrowIfFailed(command_list_->Reset(allocator, nullptr));
		RegisterMeshes();
		GenerateCommands();
		DrawPass();
		ThrowIfFailed(command_list_->Close());
	}

	void HandleSingleMessage(RT_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(std::move(arg)); }, msg); }

	void UpdateDescriptorsHeap()
	{
		if (!need_update_descriptors_)
			return;
		buffers_heaps_.FlipActive();
		DescriptorHeap& heap = buffers_heaps_.GetActive();
		heap.clear(Descriptors::total_num);
		auto device = common_.device.Get();
		CopyDescriptor(device, heap, indirect_draw_commands_.get_uav_handle(), Descriptors::indirect_draw_commands_uav);
		CopyDescriptor(device, heap, meshes_buff_, Descriptors::meshes_buff_srv);
		CopyDescriptor(device, heap, static_nodes_, Descriptors::static_nodes_srv);
		CopyDescriptor(device, heap, static_instances_, Descriptors::static_instances_srv);

		need_update_descriptors_ = false;
	}

	void Tick() override
	{
		if (!meshes_buff_ || !static_nodes_ || !static_instances_)
			return;

		UpdateDescriptorsHeap();

		PopulateCommandList();
		ID3D12CommandList* ppCommandLists[] = { command_list_.Get() };
		common_.direct_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		ThrowIfFailed(common_.swap_chain->Present(1, 0));
		MoveToNextFrame();
	}

	void ThreadInitialize() override
	{
		upload_buffer_.initialize(common_.device.Get(), 1024 * 1024);

		{
			RootSignature builder(2, 0);
			builder[0].InitAsConstants(0, 16, D3D12_SHADER_VISIBILITY_VERTEX);
			builder[1].InitAsConstants(0, 3, D3D12_SHADER_VISIBILITY_PIXEL);
			render_.root_signature_ = builder.Finalize(common_.device.Get(), common_.feature_data.HighestVersion,
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
			RootSignature builder(2, 0);
			builder[0].InitAsConstants(0, 1);
			builder[1].InitAsDescriptorTable(2);
			builder[1].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
			builder[1].SetTableRange(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3);
			generate_draw_commands_.root_signature_ = builder.Finalize(common_.device.Get(), common_.feature_data.HighestVersion);
			NAME_D3D12_OBJECT(generate_draw_commands_.root_signature_);
		}

		{
			ComputePSO builder;
			builder.SetComputeShader(g_generate_draw_commands_cs, sizeof(g_generate_draw_commands_cs));
			builder.SetRootSignature(generate_draw_commands_.root_signature_.Get());
			generate_draw_commands_.pipeline_state_ = builder.Finalize(common_.device.Get());
			NAME_D3D12_OBJECT(generate_draw_commands_.pipeline_state_);
		}

		persistent_descriptor_heap_.create(common_.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE,			Descriptors::total_num);
		buffers_heaps_.GetFirst()  .create(common_.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, Descriptors::total_num);
		buffers_heaps_.GetSecond() .create(common_.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, Descriptors::total_num);
		ThrowIfFailed(common_.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, common_.command_allocators[common_.frame_index].Get(), nullptr, IID_PPV_ARGS(&command_list_)));
		NAME_D3D12_OBJECT(command_list_);

		const uint32_t zero_val = 0;
		Construct<uint32_t>(reset_counter_src_, &zero_val, 1, common_.device.Get(), command_list_.Get()
			, upload_buffer_, nullptr, D3D12_RESOURCE_STATE_COPY_SOURCE);

		Construct<IndirectCommandGPU, UavCountedBuffer>(indirect_draw_commands_, nullptr, Const::kStaticInstancesCapacity, common_.device.Get(),
			command_list_.Get(), upload_buffer_, &persistent_descriptor_heap_, { D3D12_RESOURCE_STATE_COPY_DEST });

		ThrowIfFailed(command_list_->Close());

		ID3D12CommandList* ppCommandLists[] = { command_list_.Get() };
		common_.direct_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		
		BaseRenderer::ThreadInitialize();

		upload_buffer_.reset();
	}

	void ThreadCleanUp() override
	{
		BaseRenderer::ThreadCleanUp();

		command_list_.Reset();
		command_signature_.Reset();

		render_.Destroy();
		generate_draw_commands_.Destroy();

		indirect_draw_commands_.destroy();
		buffers_heaps_.GetFirst().destroy();
		buffers_heaps_.GetSecond().destroy();
		persistent_descriptor_heap_.destroy();
		upload_buffer_.destroy();
	}
};

const RendererCommon& IRenderer::GetRendererCommon() { assert(renderer_inst); return renderer_inst->GetCommon(); }
void IRenderer::EnqueueMsg(RT_MSG&& msg) { assert(renderer_inst); renderer_inst->EnqueueMsg(std::forward<RT_MSG>(msg)); }
IBaseSystem* IRenderer::CreateSystem(HWND hWnd, uint32_t width, uint32_t height) { return new Renderer(hWnd, width, height); }
