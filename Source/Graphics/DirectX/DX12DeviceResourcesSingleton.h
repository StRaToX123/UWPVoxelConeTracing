#pragma once

#define NOMINMAX
#include "Common.h"
#include "Utility\Debug\DebugMessage.h"

#include <Windows.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <Graphics\DirectX\d3dx12.h>

#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <deque>

#define MAX_SCREEN_WIDTH 1920
#define MAX_SCREEN_HEIGHT 1080
#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)

using namespace Microsoft::WRL;


class DX12DeviceResourcesSingleton
{
    public:
        DX12DeviceResourcesSingleton();
        ~DX12DeviceResourcesSingleton();

        static void Initialize(Windows::UI::Core::CoreWindow^ coreWindow,
            DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM, 
            DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT, 
            UINT backBufferCount = 2, 
            D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_11_0, 
            unsigned int flags = 0);
        static DX12DeviceResourcesSingleton* GetDX12DeviceResources();
        void                        Present();
        void                        WaitForGPU();
        ID3D12Device*               GetD3DDevice() const { return mDevice.Get(); }
        ID3D12Device5*              GetDXRDevice() const { return (ID3D12Device5*)(mDevice.Get());}
        IDXGISwapChain3*            GetSwapChain() const { return mSwapChain.Get(); }
        IDXGIFactory4*              GetDXGIFactory() const { return mDXGIFactory.Get(); }
        D3D_FEATURE_LEVEL           GetDeviceFeatureLevel() const { return mD3DFeatureLevel; }
        ID3D12CommandQueue*         GetCommandQueueDirect() const { return command_queue_direct.Get(); }
        ID3D12CommandQueue*         GetCommandQueueCompute() const { return command_queue_compute.Get(); }
        DXGI_FORMAT                 GetBackBufferFormat() const { return mBackBufferFormat; }
        DXGI_FORMAT                 GetDepthBufferFormat() const { return mDepthBufferFormat; }
        D3D12_VIEWPORT              GetScreenViewport() const { return mScreenViewport; }
        D3D12_RECT                  GetScissorRect() const { return mScissorRect; }
        UINT                        GetCurrentFrameIndex() const { return back_buffer_index; }
        UINT                        GetBackBufferCount() const { return mBackBufferCount; }
        RECT                        GetOutputSize() const { return mOutputSize; }

        ID3D12Resource*             GetRenderTarget() const { return mRenderTargets[back_buffer_index].Get(); }
        ID3D12Resource*             GetDepthStencil() const { return mDepthStencilTarget.Get(); }
        inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const { return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(back_buffer_index), mRTVDescriptorSize); }
        inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const { return CD3DX12_CPU_DESCRIPTOR_HANDLE(mDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart()); }
        bool OnWindowSizeChanged();
        void ResourceBarriersBegin(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers) { barriers.clear(); }
        void ResourceBarriersEnd(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, ID3D12GraphicsCommandList* commandList) {
            size_t num = barriers.size();
            if (num > 0)
                commandList->ResourceBarrier(num, barriers.data());
        }

        static const size_t                 MAX_BACK_BUFFER_COUNT = 3;
        static UINT                         back_buffer_index;

        bool IsRaytracingSupported() { return mRaytracingTierAvailable; }
        Platform::Agile<Windows::UI::Core::CoreWindow> mAppWindow;
    private:
        void CreateResources();
        void CreateWindowResources();
        void SetWindow(Windows::UI::Core::CoreWindow^ coreWindow);
        DX12DeviceResourcesSingleton(const DX12DeviceResourcesSingleton& rhs);
   
        DX12DeviceResourcesSingleton& operator=(const DX12DeviceResourcesSingleton& rhs);
        void MoveToNextFrame();
        void GetAdapter(IDXGIAdapter1** ppAdapter);
    
        ComPtr<IDXGIFactory4>               mDXGIFactory;
        ComPtr<IDXGISwapChain3>             mSwapChain;
        ComPtr<ID3D12Device>                mDevice;

        ComPtr<ID3D12CommandQueue>          command_queue_direct;
	    ComPtr<ID3D12CommandQueue>          command_queue_compute;
	   
        Wrappers::Event                     fence_event;
        ComPtr<ID3D12Fence>                 fence_direct;
        ComPtr<ID3D12Fence>                 fence_compute;
        UINT64                              fence_unused_values_direct_queue[MAX_BACK_BUFFER_COUNT];
	    UINT64                              fence_unused_value_compute_queue;

        ComPtr<ID3D12Resource>              mRenderTargets[MAX_BACK_BUFFER_COUNT];
        D3D12_RESOURCE_STATES               back_buffer_render_targets_before_states[MAX_BACK_BUFFER_COUNT];
        ComPtr<ID3D12Resource>              mDepthStencilTarget;
        ComPtr<ID3D12DescriptorHeap>        mRTVDescriptorHeap;
        ComPtr<ID3D12DescriptorHeap>        mDSVDescriptorHeap;
        UINT                                mRTVDescriptorSize;
        D3D12_VIEWPORT                      mScreenViewport;
        D3D12_RECT                          mScissorRect;

        DXGI_FORMAT                         mBackBufferFormat;
        DXGI_FORMAT                         mDepthBufferFormat;
        UINT                                mBackBufferCount;
        D3D_FEATURE_LEVEL                   mD3DMinimumFeatureLevel;

    
        RECT                                mOutputSize;
        D3D_FEATURE_LEVEL                   mD3DFeatureLevel;
        DWORD                               mDXGIFactoryFlags;

        std::filesystem::path               mCurrentPath;
        std::wstring ExecutableDirectory();
        bool mRaytracingTierAvailable = false;
};