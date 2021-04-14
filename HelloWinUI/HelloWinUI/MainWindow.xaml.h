#pragma once

#pragma push_macro("GetCurrentTime")
#undef GetCurrentTime

#include "MainWindow.g.h"

#pragma pop_macro("GetCurrentTime")


namespace winrt::HelloWinUI::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
		~MainWindow();

		void OnInit();
		void OnUpdate();
		void OnRender();
		void OnDestroy();
		void StartRenderLoop();
		void StopRenderLoop();

	private:
		static const UINT FrameCount = 2;

		// Pipeline objects.
		winrt::com_ptr<IDXGISwapChain3> m_swapChain;
		winrt::com_ptr<ID3D12Device> m_device;
		winrt::com_ptr<ID3D12Resource> m_renderTargets[FrameCount];
		winrt::com_ptr<ID3D12CommandAllocator> m_commandAllocator;
		winrt::com_ptr<ID3D12CommandQueue> m_commandQueue;
		winrt::com_ptr<ID3D12DescriptorHeap> m_rtvHeap;
		winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
		winrt::com_ptr<ID3D12GraphicsCommandList> m_commandList;
		UINT m_rtvDescriptorSize;

		// Synchronization objects.
		UINT m_frameIndex;
		HANDLE m_fenceEvent;
		winrt::com_ptr<ID3D12Fence> m_fence;
		UINT m_fenceValue;

		void LoadPipeline();
		void LoadAssets();
		void PopulateCommandList();
		void WaitForPreviousFrame();

		winrt::Windows::Foundation::IAsyncAction m_renderLoopWorker;
		Concurrency::critical_section m_criticalSection;
	public:
		void swapChainPanel_SizeChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e);
	};
}

namespace winrt::HelloWinUI::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
