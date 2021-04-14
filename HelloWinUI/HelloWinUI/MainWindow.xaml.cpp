#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <microsoft.ui.xaml.media.dxinterop.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;
using namespace Windows::UI::Core;
using namespace Microsoft::System;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::HelloWinUI::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

		OnInit();
		StartRenderLoop();
    }

	MainWindow::~MainWindow()
	{
		StopRenderLoop();
		OnDestroy();
	}

	void MainWindow::OnInit()
	{
		LoadPipeline();
		LoadAssets();
	}

	void MainWindow::OnUpdate()
	{
		com_ptr<ISwapChainPanelNative> panelNative;
		panelNative = swapChainPanel().as<ISwapChainPanelNative>();
		check_hresult(panelNative->SetSwapChain(m_swapChain.get()));

		textBlock().Text(to_hstring(swapChainPanel().ActualWidth()) + L"x" + to_hstring(swapChainPanel().ActualHeight()));
	}

	void MainWindow::OnRender()
	{
		// Record all the commands we need to render the scene into the command list.
		PopulateCommandList();

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { m_commandList.get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// Present the frame.
		check_hresult(m_swapChain->Present(1, 0));

		WaitForPreviousFrame();
	}

	void MainWindow::OnDestroy()
	{
		// Ensure that the GPU is no longer referencing resources that are about to be
		// cleaned up by the destructor.
		WaitForPreviousFrame();

		CloseHandle(m_fenceEvent);
	}


	void MainWindow::StartRenderLoop()
	{
		// If the animation render loop is already running then do not start another thread.
		if (m_renderLoopWorker != nullptr && m_renderLoopWorker.Status() == AsyncStatus::Started)
		{
			return;
		}

		// Create a task that will be run on a background thread.
		auto workItemHandler = WorkItemHandler([=](IAsyncAction action)
			{
				// Calculate the updated frame and render once per vertical blanking interval.
				while (action.Status() == AsyncStatus::Started)
				{
					critical_section::scoped_lock lock(m_criticalSection);
					OnUpdate();
					OnRender();
				}
			});

		// Run task on a dedicated high priority background thread.
		m_renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
	}

	void MainWindow::StopRenderLoop()
	{
		m_renderLoopWorker.Cancel();
	}

	void MainWindow::LoadPipeline()
	{
		UINT dxgiFactoryFlags = 0;

		/******************* Enable the debug layer. ********************/
#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
		{
			com_ptr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();

				// Enable additional debug layers.
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}
#endif

		/******************* Create the device. *************************/
		com_ptr<IDXGIFactory4> factory;
		check_hresult(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

		com_ptr<IDXGIAdapter1> hardwareAdapter;

		//hardwareAdapter->Release();

		com_ptr<IDXGIAdapter1> adapter;

		com_ptr<IDXGIFactory6> factory6;
		if (SUCCEEDED(factory.get()->QueryInterface(IID_PPV_ARGS(&factory6))))
		{
			for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)); ++adapterIndex)
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
			for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, adapter.put()); ++adapterIndex)
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

		/******************* Create command queue. *************************/
		// Describe and create the command queue.
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		check_hresult(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

		/******************* Create swap chain. *************************/
		// Describe and create the swap chain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = FrameCount;
		swapChainDesc.Width = max(static_cast<UINT>(swapChainPanel().ActualWidth()), 1);
		swapChainDesc.Height = max(static_cast<UINT>(swapChainPanel().ActualHeight()), 1);
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		swapChainDesc.SampleDesc.Count = 1;

		// When using XAML interop, the swap chain must be created for composition.
		com_ptr<IDXGISwapChain1> swapChain;
		check_hresult(factory->CreateSwapChainForComposition(
			m_commandQueue.get(),        // Swap chain needs the queue so that it can force a flush on it.
			&swapChainDesc,
			nullptr,
			swapChain.put()
		));

		check_hresult(swapChain.as(IID_PPV_ARGS(&m_swapChain)));
		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

		// Associate swap chain with SwapChainPanel
		// UI changes will need to be dispatched back to the UI thread
		DispatcherQueue().TryEnqueue(DispatcherQueuePriority::High, [=]()
			{
				// Get backing native interface for SwapChainPanel
				com_ptr<ISwapChainPanelNative> panelNative;
				panelNative = swapChainPanel().as<ISwapChainPanelNative>();
				//check_hresult(get_unknown(m_swapChainPanel)->QueryInterface(IID_PPV_ARGS(&panelNative)));

				check_hresult(panelNative->SetSwapChain(m_swapChain.get()));
			});

		//com_ptr<IDXGIDevice3> dxgiDevice;
		//check_hresult(m_device.as(IID_PPV_ARGS(&dxgiDevice)));
		//// Ensure that DXGI does not queue more than one frame at a time. This both reduces latency and
		//// ensures that the application will only render after each VSync, minimizing power consumption.
		//check_hresult(dxgiDevice->SetMaximumFrameLatency(1));

		/******************* Create descriptor heap(RTV). ***********************/
		// Create descriptor heaps.
		{
			// Describe and create a render target view (RTV) descriptor heap.
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = FrameCount;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			check_hresult(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

			m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		/******************* Create frame resources. ****************************/
		// Create frame resources.
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

			// Create a RTV for each frame.
			for (UINT n = 0; n < FrameCount; n++)
			{
				check_hresult(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
				m_device->CreateRenderTargetView(m_renderTargets[n].get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, m_rtvDescriptorSize);
			}
		}

		/******************* Create command allocator. **************************/
		check_hresult(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
	}

	void MainWindow::LoadAssets()
	{
		// Create the command list.
		check_hresult(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.get(), nullptr, IID_PPV_ARGS(&m_commandList)));

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		check_hresult(m_commandList->Close());

		// Create synchronization objects.
		{
			check_hresult(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
			m_fenceValue = 1;

			// Create an event handle to use for frame synchronization.
			m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (m_fenceEvent == nullptr)
			{
				check_hresult(HRESULT_FROM_WIN32(GetLastError()));
			}
		}
	}

	void MainWindow::PopulateCommandList()
	{
		// Command list allocators can only be reset when the associated 
		// command lists have finished execution on the GPU; apps should use 
		// fences to determine GPU execution progress.
		check_hresult(m_commandAllocator->Reset());

		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		check_hresult(m_commandList->Reset(m_commandAllocator.get(), m_pipelineState.get()));

		// Indicate that the back buffer will be used as a render target.
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ResourceBarrier(1, &barrier);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);

		// Record commands.
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		// Indicate that the back buffer will now be used to present.
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		m_commandList->ResourceBarrier(1, &barrier);

		check_hresult(m_commandList->Close());

	}

	void MainWindow::WaitForPreviousFrame()
	{
		// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
		// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
		// sample illustrates how to use fences for efficient resource usage and to
		// maximize GPU utilization.

		// Signal and increment the fence value.
		const UINT64 fence = m_fenceValue;
		check_hresult(m_commandQueue->Signal(m_fence.get(), fence));
		m_fenceValue++;

		// Wait until the previous frame is finished.
		if (m_fence->GetCompletedValue() < fence)
		{
			check_hresult(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
			WaitForSingleObject(m_fenceEvent, INFINITE);
		}

		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	}
}


void winrt::HelloWinUI::implementation::MainWindow::swapChainPanel_SizeChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& /*e*/)
{
	com_ptr<ISwapChainPanelNative> panelNative;
	panelNative = swapChainPanel().as<ISwapChainPanelNative>();
	check_hresult(panelNative->SetSwapChain(m_swapChain.get()));
}
