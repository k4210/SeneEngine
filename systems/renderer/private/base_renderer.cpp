#include "stdafx.h"
#include "base_renderer.h"
#include "stat/stat.h"

void SyncPerFrame::Initialize(const ComPtr<ID3D12Device>& device, uint32 index)
{
	assert(!need_to_wait);
	ThrowIfFailed(device->CreateFence(fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	SetNameIndexed(fence.Get(), L"fence", index);
	fence_value++;

	// Create an event handle to use for frame synchronization.
	fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void SyncPerFrame::StartSync(const ComPtr<ID3D12CommandQueue>& direct_command_queue)
{
	STAT_TIME_SCOPE(renderer, sync_start);
	assert(!need_to_wait);
	ThrowIfFailed(direct_command_queue->Signal(fence.Get(), fence_value));

	need_to_wait = fence->GetCompletedValue() < fence_value;
	if (need_to_wait)
	{
		ThrowIfFailed(fence->SetEventOnCompletion(fence_value, fence_event));
	}
	else
	{
		fence_value++;
	}
}

void SyncPerFrame::Wait()
{
	if (need_to_wait)
	{
		STAT_TIME_SCOPE(renderer, sync_wait_gpu);
		WaitForSingleObjectEx(fence_event, INFINITE, FALSE);
		fence_value++;
		need_to_wait = false;
	}
}

void SyncPerFrame::Close()
{
	assert(!need_to_wait);
	CloseHandle(fence_event);
	fence.Reset();
}

static BaseRenderer* renderer_inst = nullptr;

const RendererCommon& BaseRenderer::GetCommon()
{
	assert(renderer_inst);
	return renderer_inst->common_;
}

void BaseRenderer::StaticEnqueueMsg(RT_MSG&& msg)
{
	assert(renderer_inst);
	renderer_inst->EnqueueMsg(std::forward<RT_MSG>(msg));
}

BaseRenderer::BaseRenderer(HWND hWnd, uint32_t width, uint32_t height, BaseApp& app)
	: hwnd_(hWnd), width_(width), height_(height), app_(app)
{
	assert(!renderer_inst);
	renderer_inst = this;
	viewport_ = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	scissor_rect_ = CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));
}

BaseRenderer::~BaseRenderer()
{
	assert(renderer_inst);
	renderer_inst = nullptr;
}

void BaseRenderer::GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
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

void BaseRenderer::SetupFullscreen(const std::optional<bool> forced_mode)
{
	const bool new_fullscreen = forced_mode ? *forced_mode : !fullscreen_;
	const UINT window_style = WS_OVERLAPPEDWINDOW;
	if (!new_fullscreen)
	{
		// Restore the window's attributes and size.
		SetWindowLong(hwnd_, GWL_STYLE, window_style);

		SetWindowPos(
			hwnd_,
			HWND_NOTOPMOST,
			window_rect_.left,
			window_rect_.top,
			window_rect_.right - window_rect_.left,
			window_rect_.bottom - window_rect_.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);

		ShowWindow(hwnd_, SW_NORMAL);
	}
	else
	{
		// Save the old window rect so we can restore it when exiting fullscreen mode.
		GetWindowRect(hwnd_, &window_rect_);

		// Make the window borderless so that the client area can fill the screen.
		SetWindowLong(hwnd_, GWL_STYLE, window_style & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

		RECT fullscreen_rect;
		try
		{
			if (swap_chain_)
			{
				// Get the settings of the display on which the app's window is currently displayed
				ComPtr<IDXGIOutput> output;
				ThrowIfFailed(swap_chain_->GetContainingOutput(&output));
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
		catch (HrException& e)
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
			hwnd_,
			HWND_TOPMOST,
			fullscreen_rect.left,
			fullscreen_rect.top,
			fullscreen_rect.right,
			fullscreen_rect.bottom,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);
		ShowWindow(hwnd_, SW_MAXIMIZE);
	}
	fullscreen_ = new_fullscreen;
}

void BaseRenderer::WaitForGpu()
{
	for (SyncPerFrame& sync : sync_)
	{
		sync.Wait();
	}
	SyncPerFrame& sync = GetSync();
	sync.StartSync(direct_command_queue_);
	sync.Wait();
}

void BaseRenderer::Tick()
{
	STAT_TIME_SCOPE(renderer, tick);

	auto& sync = GetSync();
	sync.Wait();

	Draw();
	Present();

	sync.StartSync(direct_command_queue_);

	{
		STAT_TIME_SCOPE(renderer, tick_broadcast);
		const auto new_time = Utils::GetTime();
		const Utils::TimeSpan delta = new_time - time_;
		app_.ReceiveMsgToBroadcast(CommonMsg::Frame{ frame_counter, delta });
		time_ = new_time;
		frame_counter++;
	}
}

void BaseRenderer::PrepareCommonDraw(ID3D12GraphicsCommandList* command_list)
{
	assert(command_list);
	command_list->RSSetViewports(1, &viewport_);
	command_list->RSSetScissorRects(1, &scissor_rect_);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtv_heap_->GetCPUDescriptorHandleForHeapStart(), frame_index_, rtv_descriptor_size_);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsv_heap_->GetCPUDescriptorHandleForHeapStart());
	command_list->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	command_list->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	command_list->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

ID3D12CommandAllocator* BaseRenderer::GetActiveAllocator()
{
	return command_allocators[frame_index_].Get();
}

D3D_ROOT_SIGNATURE_VERSION BaseRenderer::GetRootSignatureVersion() const
{
	return feature_data_.HighestVersion;
}

void BaseRenderer::Execute(ID3D12CommandList* comand_list)
{
	ID3D12CommandList* ppCommandLists[] = { comand_list };
	Execute(_countof(ppCommandLists), ppCommandLists);
}

void BaseRenderer::Execute(uint32_t num, ID3D12CommandList* const* comand_lists)
{
	direct_command_queue_->ExecuteCommandLists(num, comand_lists);
}

void BaseRenderer::Present()
{
	STAT_TIME_SCOPE(renderer, present);
	ThrowIfFailed(swap_chain_->Present(1, 0));
	frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
}

void BaseRenderer::ThreadInitialize()
{
	for (int32 idx = 0; idx < sync_.size(); idx++)
	{
		sync_[idx].Initialize(common_.device, idx);
	}

	// Wait for the command list to execute; we are reusing the same command 
	// list in our main loop but for now, we just want to wait for setup to 
	// complete before continuing.
	WaitForGpu();

	time_ = Utils::GetTime();
}

void BaseRenderer::ThreadCleanUp()
{
	WaitForGpu();
	for (SyncPerFrame& sync : sync_)
	{
		sync.Close();
	}
}

void BaseRenderer::CustomOpen()
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
			tearing_supported_ = SUCCEEDED(hr) && allow_tearing;
		}
	}

	{
		ComPtr<IDXGIAdapter1> adapter;
		GetHardwareAdapter(factory.Get(), &adapter);
		ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&common_.device)));
		NAME_D3D12_OBJECT(common_.device);
	}

	{
		feature_data_.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(common_.device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data_, sizeof(feature_data_))))
		{
			feature_data_.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}
	}

	{
		D3D12_COMMAND_QUEUE_DESC direct_queue_desc = {};
		direct_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		direct_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ThrowIfFailed(common_.device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&direct_command_queue_)));
		NAME_D3D12_OBJECT(direct_command_queue_);
	}

	{
		DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
		swap_chain_desc.BufferCount = Const::kFrameCount;
		swap_chain_desc.Width = width_;
		swap_chain_desc.Height = height_;
		swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_desc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swap_chain;
		ThrowIfFailed(factory->CreateSwapChainForHwnd(direct_command_queue_.Get(), hwnd_, &swap_chain_desc, nullptr, nullptr, &swap_chain));
		ThrowIfFailed(factory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER));
		ThrowIfFailed(swap_chain.As(&swap_chain_));
	}

	frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = Const::kFrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(common_.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtv_heap_)));
		NAME_D3D12_OBJECT(rtv_heap_);
		rtv_descriptor_size_ = common_.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtv_heap_->GetCPUDescriptorHandleForHeapStart());
		for (UINT n = 0; n < Const::kFrameCount; n++)
		{
			ThrowIfFailed(swap_chain_->GetBuffer(n, IID_PPV_ARGS(&render_targets_[n])));
			common_.device->CreateRenderTargetView(render_targets_[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, rtv_descriptor_size_);

			ThrowIfFailed(common_.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[n])));
			NAME_D3D12_OBJECT_INDEXED(command_allocators, n);
			NAME_D3D12_OBJECT_INDEXED(render_targets_, n);
		}
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(common_.device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsv_heap_)));
		NAME_D3D12_OBJECT(dsv_heap_);
		dsv_descriptor_size_ = common_.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
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
			, width_, height_, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		ThrowIfFailed(common_.device->CreateCommittedResource(&heap_props
			, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue
			, IID_PPV_ARGS(&depth_stencil_)));

		NAME_D3D12_OBJECT(depth_stencil_);

		common_.device->CreateDepthStencilView(depth_stencil_.Get(), &depthStencilDesc, dsv_heap_->GetCPUDescriptorHandleForHeapStart());
	}
}

void BaseRenderer::CustomClose()
{
	depth_stencil_.Reset();
	dsv_heap_.Reset();
	for (UINT n = 0; n < Const::kFrameCount; n++)
	{
		render_targets_[n].Reset();
		command_allocators[n].Reset();
	}
	rtv_heap_.Reset();
	swap_chain_.Reset();
	direct_command_queue_.Reset();
}

void BaseRenderer::Destroy()
{
	if (swap_chain_ && !tearing_supported_)
	{
		ThrowIfFailed(swap_chain_->SetFullscreenState(FALSE, nullptr));
	}
}