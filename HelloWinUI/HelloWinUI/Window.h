#pragma once

class Window
{
public:
	Window(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& swapChainPanel);
	~Window();

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

	winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel m_swapChainPanel;

	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	void WaitForPreviousFrame();

	winrt::Windows::Foundation::IAsyncAction m_renderLoopWorker;
	Concurrency::critical_section m_criticalSection;
};

