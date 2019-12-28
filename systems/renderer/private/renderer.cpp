#include "stdafx.h"
#include "../renderer_interface.h"
#include "utils/base_app_helper.h"
#include "primitives/mesh_data.h"

#ifdef NDEBUG
#include "compiled_shaders/release/draw_ps.h"
#include "compiled_shaders/release/draw_vs.h"
#else
#include "compiled_shaders/debug/draw_ps.h"
#include "compiled_shaders/debug/draw_vs.h"
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

struct IndirectCommand
{
	XMFLOAT4X4 world;
	D3D12_INDEX_BUFFER_VIEW index_buffer;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer;
	XMUINT2 texture_index;
	D3D12_DRAW_INDEXED_ARGUMENTS draw_arg;
};

class Renderer : public BaseRenderer
{
protected:
	ComPtr<ID3D12RootSignature> root_signature_;
	ComPtr<ID3D12PipelineState> pipeline_state_;
	ComPtr<ID3D12GraphicsCommandList> command_list_;
	ComPtr<ID3D12CommandSignature> command_signature_;

	VertexBuffer vertex_buffer_;
	IndexBuffer32 index_buffer_;
	UavCountedBuffer indirect_draw_commands_;
	DescriptorHeap buffers_heap_;
	UploadBuffer upload_buffer_;

public:
	Renderer(HWND hWnd, uint32_t width, uint32_t height) : BaseRenderer(hWnd, width, height) {}

protected:
	void operator()(RT_MSG_UpdateCamera) {}
	void operator()(RT_MSG_MeshBuffer) 
	{
	
	}
	void operator()(RT_MSG_StaticInstances) 
	{
	
	}
	void operator()(RT_MSG_StaticNodes) {}
	void operator()(RT_MSG_ToogleFullScreen msg)
	{
		const bool new_fullscreen = msg.forced_mode ? *msg.forced_mode : !common_.fullscreen;
		SetupFullscreen(new_fullscreen);
	}

	void DrawSceneColor()   
	{
		command_list_->SetPipelineState(pipeline_state_.Get());
		command_list_->SetGraphicsRootSignature(root_signature_.Get());
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

		const D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = vertex_buffer_.get_vertex_view();
		command_list_->IASetVertexBuffers(0, 1, &vertex_buffer_view);
		const D3D12_INDEX_BUFFER_VIEW index_buffer_view = index_buffer_.get_index_view();
		command_list_->IASetIndexBuffer(&index_buffer_view);

		//command_list_->DrawIndexedInstanced(3, 1, 0, 0, 0);
		command_list_->ExecuteIndirect(command_signature_.Get(), indirect_draw_commands_.elements_num()
			, indirect_draw_commands_.get_resource(), 0
			, indirect_draw_commands_.get_resource(), indirect_draw_commands_.get_counter_offset());
		
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		barriers[1] = indirect_draw_commands_.transition_barrier(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		command_list_->ResourceBarrier(_countof(barriers), barriers);
	}

	void PopulateCommandList()
	{
		ID3D12CommandAllocator* const allocator = common_.command_allocators[common_.frame_index].Get();
		ThrowIfFailed(allocator->Reset());
		ThrowIfFailed(command_list_->Reset(allocator, nullptr));
		DrawSceneColor();
		ThrowIfFailed(command_list_->Close());
	}

	void HandleSingleMessage(RT_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(arg); }, msg); }

	void Tick() override
	{
		PopulateCommandList();
		ID3D12CommandList* ppCommandLists[] = { command_list_.Get() };
		common_.direct_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		ThrowIfFailed(common_.swap_chain->Present(1, 0));
		MoveToNextFrame();
	}

	void InitializeAssets()
	{
		{	//vertex buffer.
			const Vertex triangle_vertices[] =
			{
				{ { 0.0f, 0.25f, 0.0f    }, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f} },
				{ { 0.25f, -0.25f, 0.0f  }, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f} },
				{ { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f} }
			};

			Construct(vertex_buffer_, triangle_vertices, _countof(triangle_vertices), common_.device.Get(), 
				command_list_.Get(), upload_buffer_, buffers_heap_);
		}

		{	//Index Buffer
			const uint32_t indices[] = { 0, 1, 2 };

			Construct(index_buffer_, indices, _countof(indices), common_.device.Get(),
				command_list_.Get(), upload_buffer_, buffers_heap_);
		}

		{
			const D3D12_DRAW_INDEXED_ARGUMENTS args{ index_buffer_.elements_num() , 1, 0, 0, 0};
			const IndirectCommand indirect_commands[] = {
				{ XMFLOAT4X4(), index_buffer_.get_index_view(), vertex_buffer_.get_vertex_view(), XMUINT2(), args}
			};
			Construct(indirect_draw_commands_, indirect_commands, _countof(indirect_commands), common_.device.Get(),
				command_list_.Get(), upload_buffer_, buffers_heap_);
		}
	}

	void ThreadInitialize() override
	{
		upload_buffer_.initialize(common_.device.Get(), 1024 * 1024);

		{
			CD3DX12_ROOT_PARAMETER1 root_params[2];
			root_params[0].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
			root_params[1].InitAsConstants(2, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
			root_signature_desc.Init_1_1(_countof(root_params), root_params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&root_signature_desc, common_.feature_data.HighestVersion, &signature, &error));
			ThrowIfFailed(common_.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_)));
			NAME_D3D12_OBJECT(root_signature_);
		}

		{
			D3D12_INDIRECT_ARGUMENT_DESC arguments[5] = {};
			arguments[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
			arguments[0].Constant.RootParameterIndex = 0;
			arguments[0].Constant.Num32BitValuesToSet = 16;
			arguments[0].Constant.DestOffsetIn32BitValues = 0;
			arguments[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
			arguments[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
			arguments[2].VertexBuffer.Slot = 0;
			arguments[3].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
			arguments[3].Constant.RootParameterIndex = 1;
			arguments[3].Constant.Num32BitValuesToSet = 2;
			arguments[3].Constant.DestOffsetIn32BitValues = 0;
			arguments[4].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

			const uint32_t stride = sizeof(IndirectCommand);
			const D3D12_COMMAND_SIGNATURE_DESC desc{ stride,_countof(arguments), arguments, 0 };
			ThrowIfFailed(common_.device->CreateCommandSignature(&desc, root_signature_.Get(), IID_PPV_ARGS(&command_signature_)));
			NAME_D3D12_OBJECT(command_signature_);
		}

		{
			// Define the vertex input layout.
			D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			// Describe and create the graphics pipeline state object (PSO).
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
			psoDesc.pRootSignature = root_signature_.Get();
			psoDesc.VS = CD3DX12_SHADER_BYTECODE(g_draw_vs, sizeof(g_draw_vs));
			psoDesc.PS = CD3DX12_SHADER_BYTECODE(g_draw_ps, sizeof(g_draw_ps));
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = FALSE;
			psoDesc.DepthStencilState.StencilEnable = FALSE;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.SampleDesc.Count = 1;
			ThrowIfFailed(common_.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline_state_)));
			NAME_D3D12_OBJECT(pipeline_state_);
		}

		buffers_heap_.create(common_.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 16);
		ThrowIfFailed(common_.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, common_.command_allocators[common_.frame_index].Get(), nullptr, IID_PPV_ARGS(&command_list_)));
		NAME_D3D12_OBJECT(command_list_);

		InitializeAssets();

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
		root_signature_.Reset();
		command_signature_.Reset();
		pipeline_state_.Reset();

		vertex_buffer_.destroy();
		index_buffer_.destroy();
		indirect_draw_commands_.destroy();
		buffers_heap_.destroy();
		upload_buffer_.destroy();
	}
};

const RendererCommon* GetRendererCommon() { return renderer_inst ? &renderer_inst->GetCommon() : nullptr; }
void IRenderer::EnqueueMsg(RT_MSG&& msg) { assert(renderer_inst); renderer_inst->EnqueueMsg(std::forward<RT_MSG>(msg)); }
IBaseSystem* IRenderer::CreateSystem(HWND hWnd, uint32_t width, uint32_t height) { return new Renderer(hWnd, width, height); }
