#include "pch.h"
#include "Window.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml::Controls;

Window::Window(SwapChainPanel& swapChainPanel) : 
	m_swapChainPanel(swapChainPanel)
{
}

Window::~Window()
{
	OnDestroy();
}

void Window::OnInit()
{
	LoadPipeline();
	LoadAssets();
}

void Window::LoadPipeline()
{
	uint32_t dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		com_ptr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.put()))))
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	com_ptr<IDXGIFactory4> factory;
	check_hresult(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(factory.put())));

	com_ptr<IDXGIAdapter1> hardwareAdapter;

	hardwareAdapter.get()->Release();

	com_ptr<IDXGIAdapter1> adapter;

	com_ptr<IDXGIFactory6> factory6;
	if (SUCCEEDED(factory.get()->QueryInterface(IID_PPV_ARGS(factory6.put()))))
	{
		for (uint32_t adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapter.put())); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				// If you want a software adapter, pass in "/warp" on the command line.
				continue;
			}

			// Check to see whether the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}
	else
	{
		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory.get()->EnumAdapters1(adapterIndex, adapter.put()); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				// If you want a software adapter, pass in "/warp" on the command line.
				continue;
			}

			// Check to see whether the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}

	*hardwareAdapter.put() = adapter.detach();

	check_hresult(D3D12CreateDevice(
		hardwareAdapter.get(),
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&m_device)
	));
}
