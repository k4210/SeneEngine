#include "stdafx.h"
#include "../renderer_interface.h"
#include "utils/base_app_helper.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;
using namespace IRenderer;

class Renderer;
static Renderer* renderer_inst = nullptr;

struct RendererCommon
{
	ComPtr<IDXGISwapChain3> swap_chain;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> direct_command_queue;

	BOOL tearing_supported = false;
	uint32_t width = 1280;
	uint32_t height = 720;
	HWND hwnd = nullptr;
	
	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = { D3D_ROOT_SIGNATURE_VERSION_1 };

	RECT window_rect = { 0, 0, 1280, 720 };
	bool fullscreen = false;
/*
	ComPtr<ID3D12Fence> direct_queue_fence_;
	ComPtr<ID3D12Fence> update_queue_fence_;
	ComPtr<ID3D12Fence> loading_queue_fence_;

	ComPtr<ID3D12Resource> counter_reset_resource_;

	DescriptorHeap buff_heap_;

	const class RenderThread* render_thread_ = nullptr;
	const class SceneThread* scene_thread_ = nullptr;
	const class MoveableThread* moveable_thread_ = nullptr;

	

	std::wstring base_shader_path;

	FenceValue GetLastUnfinishedFrameRT() const;
	FenceValue GetLastUnfinishedFrameMT() const;
	FenceValue GetLastUnfinishedBatch() const;
	FenceValue GetLastLoadedBatch() const;
*/
};

class Renderer : public BaseSystemImpl<RT_MSG>
{
protected:
	RendererCommon common_;

public:
	Renderer(HWND hWnd, uint32_t width, uint32_t height)			
	{ 
		assert(!renderer_inst); 
		renderer_inst = this; 

		common_.hwnd = hWnd;
		common_.width = width;
		common_.height = height;
	}
	virtual ~Renderer() { assert( renderer_inst); renderer_inst = nullptr; }

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

	//Executed on own thread
	void HandleSingleMessage(RT_MSG& msg) override { std::visit([&](auto&& arg) { (*this)(arg); }, msg); }
	void Tick() override {}
	void ThreadInitialize() override {}
	void ThreadCleanUp() override {}

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
	}
	void CustomClose() override {}
	void Destroy() override 
	{
		if (common_.swap_chain && !common_.tearing_supported)
		{
			ThrowIfFailed(common_.swap_chain->SetFullscreenState(FALSE, nullptr));
		}
	}
};

void IRenderer::EnqueueMsg(RT_MSG&& msg) { assert(renderer_inst); renderer_inst->EnqueueMsg(std::forward<RT_MSG>(msg)); }
IBaseSystem* IRenderer::CreateSystem(HWND hWnd, uint32_t width, uint32_t height) { return new Renderer(hWnd, width, height); }