#include "stdafx.h"
#include "../renderer_interface.h"
#include "utils/base_app_helper.h"

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

class Renderer;
static Renderer* renderer_inst = nullptr;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 texcoord;
};

class Renderer : public BaseSystemImpl<RT_MSG>
{
protected:
	RendererCommon common_;

	ComPtr<ID3D12RootSignature> root_signature_;
	ComPtr<ID3D12PipelineState> pipeline_state_;
	ComPtr<ID3D12GraphicsCommandList> command_list_;

	ComPtr<ID3D12Resource> vertex_buffer_;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view_;

public:
	Renderer(HWND hWnd, uint32_t width, uint32_t height)			
	{ 
		assert(!renderer_inst); 
		renderer_inst = this; 

		common_.hwnd = hWnd;
		common_.width = width;
		common_.height = height;
		common_.viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
		common_.scissor_rect = CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));
	}
	virtual ~Renderer() { assert( renderer_inst); renderer_inst = nullptr; }
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
	void operator()(RT_MSG_UpdateCamera) {}
	void operator()(RT_MSG_MeshBuffer) {}
	void operator()(RT_MSG_StaticInstances){}
	void operator()(RT_MSG_StaticNodes) {}
	void operator()(RT_MSG_ToogleFullScreen msg) 
	{
		const bool new_fullscreen = msg.forced_mode ? *msg.forced_mode : !common_.fullscreen;
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
		// Schedule a Signal command in the queue.
		ThrowIfFailed(common_.direct_command_queue->Signal(common_.fence.Get(), common_.fence_values[common_.frame_index]));

		// Wait until the fence has been processed.
		ThrowIfFailed(common_.fence->SetEventOnCompletion(common_.fence_values[common_.frame_index], common_.fence_event));
		WaitForSingleObjectEx(common_.fence_event, INFINITE, FALSE);

		// Increment the fence value for the current frame.
		common_.fence_values[common_.frame_index]++;
	}
	void MoveToNextFrame()
	{
		// Schedule a Signal command in the queue.
		const UINT64 currentFenceValue = common_.fence_values[common_.frame_index];
		ThrowIfFailed(common_.direct_command_queue->Signal(common_.fence.Get(), currentFenceValue));

		// Update the frame index.
		common_.frame_index = common_.swap_chain->GetCurrentBackBufferIndex();

		// If the next frame is not ready to be rendered yet, wait until it is ready.
		if (common_.fence->GetCompletedValue() < common_.fence_values[common_.frame_index])
		{
			ThrowIfFailed(common_.fence->SetEventOnCompletion(common_.fence_values[common_.frame_index], common_.fence_event));
			WaitForSingleObjectEx(common_.fence_event, INFINITE, FALSE);
		}

		// Set the fence value for the next frame.
		common_.fence_values[common_.frame_index] = currentFenceValue + 1;
	}
	void PopulateCommandList()
	{
		// Command list allocators can only be reset when the associated 
		// command lists have finished execution on the GPU; apps should use 
		// fences to determine GPU execution progress.
		ThrowIfFailed(common_.command_allocators[common_.frame_index]->Reset());

		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		ThrowIfFailed(command_list_->Reset(common_.command_allocators[common_.frame_index].Get(), pipeline_state_.Get()));

		// Set necessary state.
		command_list_->SetGraphicsRootSignature(root_signature_.Get());
		command_list_->RSSetViewports(1, &common_.viewport);
		command_list_->RSSetScissorRects(1, &common_.scissor_rect);

		// Indicate that the back buffer will be used as a render target.
		auto barrier_rt = CD3DX12_RESOURCE_BARRIER::Transition(common_.render_targets[common_.frame_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		command_list_->ResourceBarrier(1, &barrier_rt);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(common_.rtv_heap->GetCPUDescriptorHandleForHeapStart(), common_.frame_index, common_.rtv_descriptor_size);
		command_list_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		// Record commands.
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		command_list_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		command_list_->IASetVertexBuffers(0, 1, &vertex_buffer_view_);
		command_list_->DrawInstanced(3, 1, 0, 0);

		// Indicate that the back buffer will now be used to present.
		auto barrier_present = CD3DX12_RESOURCE_BARRIER::Transition(common_.render_targets[common_.frame_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		command_list_->ResourceBarrier(1, &barrier_present);

		ThrowIfFailed(command_list_->Close());
	}

	//Executed on own thread
	void HandleSingleMessage(RT_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(arg); }, msg); }
	void Tick() override 
	{
		// Record all the commands we need to render the scene into the command list.
		PopulateCommandList();

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { command_list_.Get() };
		common_.direct_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// Present the frame.
		ThrowIfFailed(common_.swap_chain->Present(1, 0));

		MoveToNextFrame();
	}
	void ThreadInitialize() override 
	{
		// Create an empty root signature.
		{
			CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
			rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
			ThrowIfFailed(common_.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_)));
		}

		// Create the pipeline state, which includes compiling and loading shaders.
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
		}

		// Create the command list.
		ThrowIfFailed(common_.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, common_.command_allocators[common_.frame_index].Get()
			, pipeline_state_.Get(), IID_PPV_ARGS(&command_list_)));

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		ThrowIfFailed(command_list_->Close());

		// Create the vertex buffer.
		{
			// Define the geometry for a triangle.
			Vertex triangle_vertices[] =
			{
				{ { 0.0f, 0.25f, 0.0f    }, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f} },
				{ { 0.25f, -0.25f, 0.0f  }, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f} },
				{ { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 0.0f}, { 0.0f, 0.0f} }
			};

			const UINT vertexBufferSize = sizeof(triangle_vertices);

			// Note: using upload heaps to transfer static data like vert buffers is not 
			// recommended. Every time the GPU needs it, the upload heap will be marshalled 
			// over. Please read up on Default Heap usage. An upload heap is used here for 
			// code simplicity and because there are very few verts to actually transfer.
			CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC resource_Desc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
			ThrowIfFailed(common_.device->CreateCommittedResource(
				&heap_properties,
				D3D12_HEAP_FLAG_NONE,
				&resource_Desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&vertex_buffer_)));

			// Copy the triangle data to the vertex buffer.
			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
			ThrowIfFailed(vertex_buffer_->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, triangle_vertices, sizeof(triangle_vertices));
			vertex_buffer_->Unmap(0, nullptr);

			// Initialize the vertex buffer view.
			vertex_buffer_view_.BufferLocation = vertex_buffer_->GetGPUVirtualAddress();
			vertex_buffer_view_.StrideInBytes = sizeof(Vertex);
			vertex_buffer_view_.SizeInBytes = vertexBufferSize;
		}

		// Create synchronization objects and wait until assets have been uploaded to the GPU.
		{
			ThrowIfFailed(common_.device->CreateFence(common_.fence_values[common_.frame_index], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&common_.fence)));
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
		}

		{
			D3D12_COMMAND_QUEUE_DESC direct_queue_desc = {};
			direct_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			direct_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			ThrowIfFailed(common_.device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&common_.direct_command_queue)));
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

			common_.rtv_descriptor_size = common_.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		// Create frame resources.
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(common_.rtv_heap->GetCPUDescriptorHandleForHeapStart());

			// Create a RTV and a command allocator for each frame.
			for (UINT n = 0; n < Const::kFrameCount; n++)
			{
				ThrowIfFailed(common_.swap_chain->GetBuffer(n, IID_PPV_ARGS(&common_.render_targets[n])));
				common_.device->CreateRenderTargetView(common_.render_targets[n].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, common_.rtv_descriptor_size);

				ThrowIfFailed(common_.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&common_.command_allocators[n])));
			}
		}
	}
	void CustomClose() override 
	{
		
	}
	void Destroy() override 
	{
		if (common_.swap_chain && !common_.tearing_supported)
		{
			ThrowIfFailed(common_.swap_chain->SetFullscreenState(FALSE, nullptr));
		}
	}
};

const RendererCommon* GetRendererCommon() { return renderer_inst ? &renderer_inst->GetCommon() : nullptr; }
void IRenderer::EnqueueMsg(RT_MSG&& msg) { assert(renderer_inst); renderer_inst->EnqueueMsg(std::forward<RT_MSG>(msg)); }
IBaseSystem* IRenderer::CreateSystem(HWND hWnd, uint32_t width, uint32_t height) { return new Renderer(hWnd, width, height); }
