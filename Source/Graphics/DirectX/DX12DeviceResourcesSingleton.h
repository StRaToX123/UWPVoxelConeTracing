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
        ID3D12Device*               GetD3DDevice() const { return d3d_device.Get(); }
        ID3D12Device5*              GetDXRDevice() const { return (ID3D12Device5*)(d3d_device.Get());}
        IDXGISwapChain3*            GetSwapChain() const { return swap_chain.Get(); }
        IDXGIFactory4*              GetDXGIFactory() const { return dxgi_factory.Get(); }
        D3D_FEATURE_LEVEL           GetDeviceFeatureLevel() const { return d3d_feature_level; }
        ID3D12CommandQueue*         GetCommandQueueDirect() const { return command_queue_direct.Get(); }
        ID3D12CommandQueue*         GetCommandQueueCompute() const { return command_queue_compute.Get(); }
        DXGI_FORMAT                 GetBackBufferFormat() const { return format_back_buffer; }
        DXGI_FORMAT                 GetDepthBufferFormat() const { return format_depth_buffer; }
        D3D12_VIEWPORT              GetScreenViewport() const { return viewport_screen; }
        D3D12_RECT                  GetScissorRect() const { return scissor_rect_screen; }
        UINT                        GetCurrentFrameIndex() const { return back_buffer_index; }
        UINT                        GetBackBufferCount() const { return back_buffer_count; }
        RECT                        GetOutputSize() const { return output_size; }

        ID3D12Resource*             GetRenderTarget() const { return render_targets_back_buffer[back_buffer_index].Get(); }
        ID3D12Resource*             GetDepthStencil() const { return depth_stencil.Get(); }
        inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const { return CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(back_buffer_index), descriptor_size_rtv); }
        inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const { return CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptor_heap_dsv->GetCPUDescriptorHandleForHeapStart()); }
        bool OnWindowSizeChanged();
        void ResourceBarriersBegin(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers) { barriers.clear(); }
        void ResourceBarriersEnd(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, ID3D12GraphicsCommandList* commandList) {
            size_t num = barriers.size();
            if (num > 0)
                commandList->ResourceBarrier(num, barriers.data());
        }

        static const size_t                 MAX_BACK_BUFFER_COUNT = 3;
        static UINT                         back_buffer_index;

        bool IsRaytracingSupported() { return is_ray_tracing_available; }
        Platform::Agile<Windows::UI::Core::CoreWindow> core_window;
    private:
        void CreateResources();
        void CreateWindowResources();
        DX12DeviceResourcesSingleton(const DX12DeviceResourcesSingleton& rhs);
   
        DX12DeviceResourcesSingleton& operator=(const DX12DeviceResourcesSingleton& rhs);
        void MoveToNextFrame();
        void GetAdapter(IDXGIAdapter1** ppAdapter);
    
        ComPtr<IDXGIFactory4>               dxgi_factory;
        ComPtr<IDXGISwapChain3>             swap_chain;
        ComPtr<ID3D12Device>                d3d_device;

        ComPtr<ID3D12CommandQueue>          command_queue_direct;
	    ComPtr<ID3D12CommandQueue>          command_queue_compute;
	   
        Wrappers::Event                     fence_event;
        ComPtr<ID3D12Fence>                 fence_direct;
        ComPtr<ID3D12Fence>                 fence_compute;
        UINT64                              fence_unused_values_direct_queue[MAX_BACK_BUFFER_COUNT];
	    UINT64                              fence_unused_value_compute_queue;

        ComPtr<ID3D12Resource>              render_targets_back_buffer[MAX_BACK_BUFFER_COUNT];
        D3D12_RESOURCE_STATES               back_buffer_render_targets_before_states[MAX_BACK_BUFFER_COUNT];
        ComPtr<ID3D12Resource>              depth_stencil;
        ComPtr<ID3D12DescriptorHeap>        descriptor_heap_rtv;
        ComPtr<ID3D12DescriptorHeap>        descriptor_heap_dsv;
        UINT                                descriptor_size_rtv;
        D3D12_VIEWPORT                      viewport_screen;
        D3D12_RECT                          scissor_rect_screen;

        DXGI_FORMAT                         format_back_buffer;
        DXGI_FORMAT                         format_depth_buffer;
        UINT                                back_buffer_count;
        D3D_FEATURE_LEVEL                   d3d_minimum_feature_level;

    
        RECT                                output_size;
        D3D_FEATURE_LEVEL                   d3d_feature_level;
        DWORD                               dxgi_factory_flags;

        std::filesystem::path               current_path;
        bool is_ray_tracing_available = false;
};