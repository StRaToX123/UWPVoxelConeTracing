#pragma once


#include <wrl.h>
#include <d3d12.h>
#include "Graphics\DirectX\d3dx12.h"
#include "Graphics\DirectX\DirectXHelper.h"
#include "Graphics\DeviceResources\DeviceResourcesCommon.h"
#include "Utility/Time/HighResolutionClock.h"
#include "Utility/Debugging/DebugMessage.h"
#include "Scene/Mesh.h"
#include <DirectXMath.h>
#include <dxgi1_4.h>


using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;


// Controls all the DirectX device resources.
class DeviceResources
{
	public:
		DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT);
		~DeviceResources();
		void SetWindow(Windows::UI::Core::CoreWindow^ window);
		void SetLogicalSize(Windows::Foundation::Size logicalSize);
		void SetCurrentOrientation(Windows::Graphics::Display::DisplayOrientations currentOrientation);
		void SetDpi(float dpi);
		void ValidateDevice();
		void Present();
		void WaitForGpu();

		// The size of the render target, in pixels.
		Windows::Foundation::Size	GetOutputSize() const				{ return m_outputSize; }

		// The size of the render target, in dips.
		Windows::Foundation::Size	GetLogicalSize() const				{ return m_logicalSize; }

		float						GetDpi() const						{ return m_effectiveDpi; }
		bool						IsDeviceRemoved() const				{ return m_deviceRemoved; }

		// D3D Accessors.
		ID3D12Device*				GetD3DDevice() const				{ return m_d3dDevice.Get(); }
		IDXGISwapChain3*			GetSwapChain() const				{ return m_swapChain.Get(); }
		ID3D12Resource*				GetRenderTarget() const				{ return m_renderTargets[current_back_buffer_index].Get(); }
		ID3D12Resource*				GetDepthStencil() const				{ return m_depthStencil.Get(); }
		ID3D12CommandQueue*			GetCommandQueueDirect() const	    { return command_queue_direct.Get(); }
		ID3D12CommandQueue*         GetCommandQueueCopyNormalPriority() const { return command_queue_copy_normal_priority.Get(); }
		ID3D12CommandQueue*         GetCommandQueueCopyHighPriority() const { return command_queue_copy_high_priority.Get(); }
		ID3D12CommandQueue*         GetCommandQueueCompute() const      { return command_queue_compute.Get(); }
		ID3D12CommandAllocator*		GetCommandAllocatorDirect() const	{ return command_allocators_direct[current_back_buffer_index].Get(); }
		ID3D12CommandAllocator*     GetCommandAllocatorCopyNormalPriority() const     { return command_allocator_copy_normal_priority.Get(); }
		ID3D12CommandAllocator*     GetCommandAllocatorCopyHighPriority() const { return command_allocator_copy_high_priority.Get(); }
		ID3D12CommandAllocator*     GetCommandAllocatorCompute() const  { return command_allocator_compute[current_back_buffer_index].Get(); }
		//ID3D12PipelineState*        GetDefaultPipelineState() const     { return pipeline_state_default.Get(); }

		DXGI_FORMAT					GetBackBufferFormat() const			{ return m_backBufferFormat; }
		DXGI_FORMAT					GetDepthBufferFormat() const		{ return m_depthBufferFormat; }
		D3D12_VIEWPORT				GetScreenViewport() const			{ return m_screenViewport; }
		DirectX::XMFLOAT4X4			GetOrientationTransform3D() const	{ return m_orientationTransform3D; }
		UINT						GetCurrentFrameIndex() const		{ return current_back_buffer_index; }


		CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), current_back_buffer_index, m_rtvDescriptorSize);
		}
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		}

		
	private:
		void CreateDeviceIndependentResources();
		void CreateDeviceResources();
		void CreateWindowSizeDependentResources();
		void UpdateRenderTargetSize();
		//void MoveToNextFrame();
		DXGI_MODE_ROTATION ComputeDisplayRotation();
		void GetHardwareAdapter(IDXGIAdapter1** ppAdapter);

		
		HighResolutionClock high_resolution_clock;
		bool first_tick;
		UINT64 frame_count;
		// Direct3D objects.
		Microsoft::WRL::ComPtr<ID3D12Device>			m_d3dDevice;
		Microsoft::WRL::ComPtr<IDXGIFactory4>			m_dxgiFactory;
		Microsoft::WRL::ComPtr<IDXGISwapChain3>			m_swapChain;
		Microsoft::WRL::ComPtr<ID3D12Resource>			m_renderTargets[c_frame_count];
		int                                             current_back_buffer_index;
		Microsoft::WRL::ComPtr<ID3D12Resource>			m_depthStencil;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	m_rtvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	m_dsvHeap;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>		command_queue_direct;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>		command_queue_copy_normal_priority;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>		command_queue_copy_high_priority;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>      command_queue_compute;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>	command_allocators_direct[c_frame_count];
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>	command_allocator_copy_normal_priority;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>	command_allocator_copy_high_priority;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>	command_allocator_compute[c_frame_count];
		//Microsoft::WRL::ComPtr<ID3D12PipelineState>     pipeline_state_default;
		DXGI_FORMAT										m_backBufferFormat;
		DXGI_FORMAT										m_depthBufferFormat;
		D3D12_VIEWPORT									m_screenViewport;
		UINT											m_rtvDescriptorSize;
		bool											m_deviceRemoved;



		// This event and fence are independant from the ones used by the PresentThread
		// These variables are used for custom on the side wait for gpu workload tasks
		Microsoft::WRL::ComPtr<ID3D12Fence>		        fence;
		HANDLE                                          event_wait_for_gpu;
		UINT64                                          fence_values[c_frame_count];
		UINT64                                          fence_latest_unused_value;

		// Cached reference to the Window.
		Platform::Agile<Windows::UI::Core::CoreWindow>	m_window;

		// Cached device properties.
		Windows::Foundation::Size						m_d3dRenderTargetSize;
		Windows::Foundation::Size						m_outputSize;
		Windows::Foundation::Size						m_logicalSize;
		Windows::Graphics::Display::DisplayOrientations	m_nativeOrientation;
		Windows::Graphics::Display::DisplayOrientations	m_currentOrientation;
		float											m_dpi;

		// This is the DPI that will be reported back to the app. It takes into account whether the app supports high resolution screens or not.
		float											m_effectiveDpi;

		// Transforms used for display orientation.
		DirectX::XMFLOAT4X4								m_orientationTransform3D;
};
