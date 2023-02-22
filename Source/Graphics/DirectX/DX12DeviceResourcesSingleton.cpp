#include "Graphics\DirectX\DX12DeviceResourcesSingleton.h"


UINT DX12DeviceResourcesSingleton::back_buffer_index = 0;
static DX12DeviceResourcesSingleton gs_dx12_device_resources;


DX12DeviceResourcesSingleton::DX12DeviceResourcesSingleton()
{

}

DX12DeviceResourcesSingleton::~DX12DeviceResourcesSingleton()
{
    WaitForGPU();
}

DX12DeviceResourcesSingleton* DX12DeviceResourcesSingleton::GetDX12DeviceResources()
{
    return &gs_dx12_device_resources;
}

void DX12DeviceResourcesSingleton::Initialize(Windows::UI::Core::CoreWindow^ coreWindow,
    DXGI_FORMAT backBufferFormat, 
    DXGI_FORMAT depthBufferFormat, 
    UINT backBufferCount, 
    D3D_FEATURE_LEVEL minFeatureLevel, 
    unsigned int flags)
{
    gs_dx12_device_resources.current_path = std::filesystem::current_path();
    for (UINT i = 0; i < backBufferCount; i++)
    {
        gs_dx12_device_resources.fence_unused_values_direct_queue[i] = 0;
    }

    gs_dx12_device_resources.fence_unused_value_compute_queue = 0;
    gs_dx12_device_resources.format_back_buffer = backBufferFormat;
    gs_dx12_device_resources.format_depth_buffer = depthBufferFormat;
    gs_dx12_device_resources.back_buffer_count = backBufferCount;
    gs_dx12_device_resources.d3d_minimum_feature_level = minFeatureLevel;
    gs_dx12_device_resources.d3d_feature_level = D3D_FEATURE_LEVEL_11_0;
    gs_dx12_device_resources.descriptor_size_rtv = 0;
    gs_dx12_device_resources.dxgi_factory_flags = 0;
    gs_dx12_device_resources.core_window = nullptr;
    gs_dx12_device_resources.viewport_screen = {};
    gs_dx12_device_resources.scissor_rect_screen = {};
    gs_dx12_device_resources.core_window = coreWindow;
    gs_dx12_device_resources.output_size.left = gs_dx12_device_resources.output_size.top = gs_dx12_device_resources.output_size.right = gs_dx12_device_resources.output_size.bottom = 0;

    gs_dx12_device_resources.CreateResources();
    // This will get called on the first OnWindowSizeChanged callback
    // We don't call it here because we will use that first OnWindowSizeChanged as a WaitForGPU call
    // which is inside of the CreateWindowResources function. That way all the work submited to the gpu
    // for the main apps initialization will get waited for by the first OnWindowSizeChanged callback
    //gs_dx12_device_resources.CreateWindowResources(); 
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DX12DeviceResourcesSingleton::CreateResources()
{
#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
        {
            debugController->EnableDebugLayer();
        }
        else
        {
            OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
        }

        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
        {
            dxgi_factory_flags = DXGI_CREATE_FACTORY_DEBUG;

            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

            DXGI_INFO_QUEUE_MESSAGE_ID hide[] = { 80, };
            DXGI_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
        }
    }
#endif

    ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(dxgi_factory.ReleaseAndGetAddressOf())));

    ComPtr<IDXGIAdapter1> adapter;
    GetAdapter(adapter.GetAddressOf());

    // Create the DX12 API device object.
    ThrowIfFailed(D3D12CreateDevice(
        adapter.Get(),
        d3d_minimum_feature_level,
        IID_PPV_ARGS(d3d_device.ReleaseAndGetAddressOf())
    ));

    d3d_device->SetName(L"DirectX12Wrapper");

#ifndef NDEBUG
    // Configure debug device (if active).
    ComPtr<ID3D12InfoQueue> d3dInfoQueue;
    if (SUCCEEDED(d3d_device.As(&d3dInfoQueue)))
    {
#ifdef _DEBUG
        d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif
        D3D12_MESSAGE_ID hide[] =
        {
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE
        };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = _countof(hide);
        filter.DenyList.pIDList = hide;
        d3dInfoQueue->AddStorageFilterEntries(&filter);
    }
#endif

    // Determine maximum supported feature level for this device
    static const D3D_FEATURE_LEVEL s_featureLevels[] =
    {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels =
    {
        _countof(s_featureLevels), s_featureLevels, D3D_FEATURE_LEVEL_11_0
    };

    HRESULT hr = d3d_device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels));
    if (SUCCEEDED(hr))
        d3d_feature_level = featLevels.MaxSupportedFeatureLevel;
    else
        d3d_feature_level = d3d_minimum_feature_level;

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};
	HRESULT hr2 = d3d_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData));
    if (SUCCEEDED(hr2) && featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
        is_ray_tracing_available = true;
    }
    else
    {
        is_ray_tracing_available = false;
    }
        
    // Create the command queue. graphics
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        ThrowIfFailed(d3d_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(command_queue_direct.ReleaseAndGetAddressOf())));
        command_queue_direct->SetName(L"CommandQueueGraphics");

        D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
        rtvDescriptorHeapDesc.NumDescriptors = back_buffer_count;
        rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

        ThrowIfFailed(d3d_device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(descriptor_heap_rtv.ReleaseAndGetAddressOf())));
        descriptor_size_rtv = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        if (format_depth_buffer != DXGI_FORMAT_UNKNOWN)
        {
            D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
            dsvDescriptorHeapDesc.NumDescriptors = 1;
            dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

            ThrowIfFailed(d3d_device->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(descriptor_heap_dsv.ReleaseAndGetAddressOf())));
        }

        // Create a fence for tracking GPU execution progress.
        ThrowIfFailed(d3d_device->CreateFence(fence_unused_values_direct_queue[back_buffer_index], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_direct.ReleaseAndGetAddressOf())));
        fence_unused_values_direct_queue[back_buffer_index]++;
        fence_event.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
        if (!fence_event.IsValid())
        {
            throw std::exception("CreateEvent");
        }

        fence_direct->SetName(L"mFenceValuesGraphics");
    }

    // Create async compute data
    {
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

		ThrowIfFailed(d3d_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(command_queue_compute.ReleaseAndGetAddressOf())));
        command_queue_compute->SetName(L"CommandQueueCompute");

		// Create a fence for async compute.
		ThrowIfFailed(d3d_device->CreateFence(fence_unused_value_compute_queue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_compute.ReleaseAndGetAddressOf())));
        fence_unused_value_compute_queue++;
        fence_compute->SetName(L"Compute fence");
    }
}

void DX12DeviceResourcesSingleton::CreateWindowResources()
{
    if (core_window == NULL)
    {
        throw std::exception("Call SetWindow with a valid Win32 window handle");
    }

    WaitForGPU();
    // Release resources that are tied to the swap chain and update fence values.
    for (UINT i = 0; i < back_buffer_count; i++)
    {
        render_targets_back_buffer[i].Reset();
        // We have to reset all the back buffer fence values to the newest most unused value.
        // That way no matter the value of the mBackBufferIndex at the moment of this function's execution
        // once the mBackBufferIndex resets to 0 latter on in this function due to us reinitializing the swapchain,
        // all the fence values will be ready to use the latest unused value.
        fence_unused_values_direct_queue[i] = fence_unused_values_direct_queue[back_buffer_index];
    }

    // Determine the render target size in pixels.
    UINT backBufferWidth = std::max<UINT>(static_cast<UINT>(output_size.right - output_size.left), 1u);
    UINT backBufferHeight = std::max<UINT>(static_cast<UINT>(output_size.bottom - output_size.top), 1u);
    
    // If the swap chain already exists, resize it, otherwise create one.
    if (swap_chain)
    {
        // If the swap chain already exists, resize it.
        HRESULT hr = swap_chain->ResizeBuffers(
            back_buffer_count,
            backBufferWidth,
            backBufferHeight,
            format_back_buffer,
            0u
        );
        ThrowIfFailed(hr);
    }
    else
    {
        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = format_back_buffer;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = back_buffer_count;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        //DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
        //fsSwapChainDesc.Windowed = TRUE;

        // Create a swap chain for the window.
        ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailed(dxgi_factory->CreateSwapChainForCoreWindow(
            command_queue_direct.Get(),
            reinterpret_cast<IUnknown*>(core_window.Get()),
            &swapChainDesc,
            nullptr/*&fsSwapChainDesc*/,
            &swapChain));

        ThrowIfFailed(swapChain.As(&swap_chain));
        //ThrowIfFailed(dxgi_factory->MakeWindowAssociation(mAppWindow, DXGI_MWA_NO_ALT_ENTER));
    }

    // Obtain the back buffers for this window which will be the final render targets
    // and create render target views for each of them.
    for (UINT n = 0; n < back_buffer_count; n++)
    {
        ThrowIfFailed(swap_chain->GetBuffer(n, IID_PPV_ARGS(render_targets_back_buffer[n].GetAddressOf())));

        wchar_t name[25] = {};
        swprintf_s(name, L"Main Render target %u", n);
        render_targets_back_buffer[n]->SetName(name);

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = format_back_buffer;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart(),
            static_cast<INT>(n), 
            descriptor_size_rtv);
        d3d_device->CreateRenderTargetView(render_targets_back_buffer[n].Get(), &rtvDesc, rtvDescriptor);
    }

    // Reset the index to the current back buffer.
    back_buffer_index = swap_chain->GetCurrentBackBufferIndex();

    if (format_depth_buffer != DXGI_FORMAT_UNKNOWN)
    {
        // Allocate a 2-D surface as the depth/stencil buffer and create a depth/stencil view
        // on this surface.
        CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

        D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            format_depth_buffer,
            output_size.right, output_size.bottom,
            1, // This depth stencil view has only one texture.
            1  // Use a single mipmap level.
        );
        depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = format_depth_buffer;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        ThrowIfFailed(d3d_device->CreateCommittedResource(
            &depthHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(depth_stencil.ReleaseAndGetAddressOf())
        ));

        depth_stencil->SetName(L"Depth-stencil target");

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = format_depth_buffer;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

        d3d_device->CreateDepthStencilView(depth_stencil.Get(), &dsvDesc, descriptor_heap_dsv->GetCPUDescriptorHandleForHeapStart());
    }

    // Set the 3D rendering viewport and scissor rectangle to target the entire window.
    viewport_screen.TopLeftX = viewport_screen.TopLeftY = 0.f;
    viewport_screen.Width = static_cast<float>(backBufferWidth);
    viewport_screen.Height = static_cast<float>(backBufferHeight);
    viewport_screen.MinDepth = D3D12_MIN_DEPTH;
    viewport_screen.MaxDepth = D3D12_MAX_DEPTH;

    scissor_rect_screen.left = scissor_rect_screen.top = 0;
    scissor_rect_screen.right = static_cast<LONG>(backBufferWidth);
    scissor_rect_screen.bottom = static_cast<LONG>(backBufferHeight);
}

bool DX12DeviceResourcesSingleton::OnWindowSizeChanged()
{
    RECT newRc;
    newRc.left = newRc.top = 0;
    newRc.right = core_window->Bounds.Width;
    newRc.bottom = core_window->Bounds.Height;
    if (newRc.left == output_size.left
        && newRc.top == output_size.top
        && newRc.right == output_size.right
        && newRc.bottom == output_size.bottom)
    {
        return false;
    }

    output_size = newRc;
    CreateWindowResources();
    return true;
}

void DX12DeviceResourcesSingleton::Present()
{
    HRESULT hr;
    hr = swap_chain->Present(use_vsync, 0);
    // If the device was reset we must completely reinitialize the renderer.
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
#ifdef _DEBUG
        char buff[64] = {};
        sprintf_s(buff, "Device Lost on Present: Reason code 0x%08X\n", (hr == DXGI_ERROR_DEVICE_REMOVED) ? d3d_device->GetDeviceRemovedReason() : hr);
        OutputDebugStringA(buff);
#endif
        ThrowIfFailed(hr);
    }
    else
    {
        ThrowIfFailed(hr);
        MoveToNextFrame();

        if (!dxgi_factory->IsCurrent())
            ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(dxgi_factory.ReleaseAndGetAddressOf())));
    }
}

void DX12DeviceResourcesSingleton::WaitForGPU() 
{
    // Wait for graphics queue
    // Schedule a Signal command in the GPU queue.
    UINT64 fenceValue = fence_unused_values_direct_queue[back_buffer_index];
    if (SUCCEEDED(command_queue_direct->Signal(fence_direct.Get(), fenceValue)))
    {
        // Wait until the Signal has been processed.
        if (SUCCEEDED(fence_direct->SetEventOnCompletion(fenceValue, fence_event.Get())))
        {
            WaitForSingleObjectEx(fence_event.Get(), INFINITE, FALSE);
            // Increment the fence value for the current frame.
            fence_unused_values_direct_queue[back_buffer_index]++;
        }
    }
    

    // Wait for compute queue
    // Schedule a Signal command in the GPU queue.
    fenceValue = fence_unused_value_compute_queue;
    if (SUCCEEDED(command_queue_compute->Signal(fence_compute.Get(), fenceValue)))
    {
        // Wait until the Signal has been processed.
        if (SUCCEEDED(fence_compute->SetEventOnCompletion(fenceValue, fence_event.Get())))
        {
            WaitForSingleObjectEx(fence_event.Get(), INFINITE, FALSE);

            // Increment the fence value for the current frame.
            fence_unused_value_compute_queue++;
        }
    }
}

void DX12DeviceResourcesSingleton::MoveToNextFrame()
{
    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = fence_unused_values_direct_queue[back_buffer_index];
    ThrowIfFailed(command_queue_direct->Signal(fence_direct.Get(), currentFenceValue));

    // Update the back buffer index.
    back_buffer_index = swap_chain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (fence_direct->GetCompletedValue() < fence_unused_values_direct_queue[back_buffer_index])
    {
        ThrowIfFailed(fence_direct->SetEventOnCompletion(fence_unused_values_direct_queue[back_buffer_index], fence_event.Get()));
        WaitForSingleObjectEx(fence_event.Get(), INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    fence_unused_values_direct_queue[back_buffer_index] = currentFenceValue + 1;
}

void DX12DeviceResourcesSingleton::GetAdapter(IDXGIAdapter1** ppAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

#if defined(__dxgi1_6_h__) && defined(NTDDI_WIN10_RS4)
    ComPtr<IDXGIFactory6> factory6;
    HRESULT hr = dxgi_factory.As(&factory6);
    if (SUCCEEDED(hr))
    {
        for (UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())));
            adapterIndex++)
        {
            DXGI_ADAPTER_DESC1 desc;
            ThrowIfFailed(adapter->GetDesc1(&desc));

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                continue;
            }

            // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), d3d_minimum_feature_level, _uuidof(ID3D12Device), nullptr)))
            {
#ifdef _DEBUG
                wchar_t buff[256] = {};
                swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
                OutputDebugStringW(buff);
#endif
                break;
            }
        }
    }
#endif
    if (!adapter)
    {
        for (UINT adapterIndex = 0;
            SUCCEEDED(dxgi_factory->EnumAdapters1(
                adapterIndex,
                adapter.ReleaseAndGetAddressOf()));
            ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            ThrowIfFailed(adapter->GetDesc1(&desc));

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                continue;
            }

            // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), d3d_minimum_feature_level, _uuidof(ID3D12Device), nullptr)))
            {
#ifdef _DEBUG
                wchar_t buff[256] = {};
                swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
                OutputDebugStringW(buff);
#endif
                break;
            }
        }
    }

#if !defined(NDEBUG)
    if (!adapter)
    {
        // Try WARP12 instead
        if (FAILED(dxgi_factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))))
        {
            throw std::exception("WARP12 not available");
        }

        OutputDebugStringA("Direct3D Adapter - WARP12\n");
    }
#endif

    if (!adapter)
    {
        throw std::exception("No Direct3D 12 device found");
    }

    *ppAdapter = adapter.Detach();
}