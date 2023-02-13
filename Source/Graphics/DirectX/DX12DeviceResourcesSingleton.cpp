//DirectX 12 wrapper with useful functions like device creation etc.

#include "Graphics\DirectX\DX12DeviceResourcesSingleton.h"


UINT DX12DeviceResourcesSingleton::mBackBufferIndex = 0;
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
    gs_dx12_device_resources.mCurrentPath = std::filesystem::current_path();
    for (UINT i = 0; i < backBufferCount; i++)
    {
        gs_dx12_device_resources.mFenceValuesGraphics[i] = 0;
    }

    gs_dx12_device_resources.mFenceValuesCompute = 0;
    gs_dx12_device_resources.mBackBufferFormat = backBufferFormat;
    gs_dx12_device_resources.mDepthBufferFormat = depthBufferFormat;
    gs_dx12_device_resources.mBackBufferCount = backBufferCount;
    gs_dx12_device_resources.mD3DMinimumFeatureLevel = minFeatureLevel;
    gs_dx12_device_resources.mD3DFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    gs_dx12_device_resources.mRTVDescriptorSize = 0;
    gs_dx12_device_resources.mDXGIFactoryFlags = 0;
    gs_dx12_device_resources.mAppWindow = nullptr;
    gs_dx12_device_resources.mScreenViewport = {};
    gs_dx12_device_resources.mScissorRect = {};
    gs_dx12_device_resources.mOutputSize = { 0, 0, 1, 1 };

    gs_dx12_device_resources.SetWindow(coreWindow);
    gs_dx12_device_resources.CreateResources();
    ThrowIfFailed(gs_dx12_device_resources.mCommandListGraphics->Close());
    ID3D12CommandList* ppCommandLists[] = { gs_dx12_device_resources.mCommandListGraphics.Get() };
    gs_dx12_device_resources.mCommandQueueGraphics->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    gs_dx12_device_resources.CreateWindowResources();
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
            mDXGIFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

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

    ThrowIfFailed(CreateDXGIFactory2(mDXGIFactoryFlags, IID_PPV_ARGS(mDXGIFactory.ReleaseAndGetAddressOf())));

    ComPtr<IDXGIAdapter1> adapter;
    GetAdapter(adapter.GetAddressOf());

    // Create the DX12 API device object.
    ThrowIfFailed(D3D12CreateDevice(
        adapter.Get(),
        mD3DMinimumFeatureLevel,
        IID_PPV_ARGS(mDevice.ReleaseAndGetAddressOf())
    ));

    mDevice->SetName(L"DirectX12Wrapper");

#ifndef NDEBUG
    // Configure debug device (if active).
    ComPtr<ID3D12InfoQueue> d3dInfoQueue;
    if (SUCCEEDED(mDevice.As(&d3dInfoQueue)))
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

    HRESULT hr = mDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels));
    if (SUCCEEDED(hr))
        mD3DFeatureLevel = featLevels.MaxSupportedFeatureLevel;
    else
        mD3DFeatureLevel = mD3DMinimumFeatureLevel;

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};
	HRESULT hr2 = mDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData));
    if (SUCCEEDED(hr2) && featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
		mRaytracingTierAvailable = true;
    else 
        mRaytracingTierAvailable = false;

    // Create the command queue. graphics
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        ThrowIfFailed(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(mCommandQueueGraphics.ReleaseAndGetAddressOf())));
        mCommandQueueGraphics->SetName(L"CommandQueueGraphics");

        D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
        rtvDescriptorHeapDesc.NumDescriptors = mBackBufferCount;
        rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

        ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(mRTVDescriptorHeap.ReleaseAndGetAddressOf())));
        mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        if (mDepthBufferFormat != DXGI_FORMAT_UNKNOWN)
        {
            D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
            dsvDescriptorHeapDesc.NumDescriptors = 1;
            dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

            ThrowIfFailed(mDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(mDSVDescriptorHeap.ReleaseAndGetAddressOf())));
        }

        // Create a command allocator for each back buffer that will be rendered to.
        for (UINT n = 0; n < mBackBufferCount; n++)
        {
            ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCommandAllocatorsGraphics[n].ReleaseAndGetAddressOf())));
        }

        // Create a command list for recording graphics commands.
        ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocatorsGraphics[0].Get(), nullptr, IID_PPV_ARGS(mCommandListGraphics.ReleaseAndGetAddressOf())));

        // Create a fence for tracking GPU execution progress.
        ThrowIfFailed(mDevice->CreateFence(mFenceValuesGraphics[mBackBufferIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFenceGraphics.ReleaseAndGetAddressOf())));
        mFenceValuesGraphics[mBackBufferIndex]++;
        mFenceEventGraphics.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
        if (!mFenceEventGraphics.IsValid())
        {
            throw std::exception("CreateEvent");
        }
        mFenceGraphics->SetName(L"mFenceValuesGraphics");
    }
    // Create async compute data
    {
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

		ThrowIfFailed(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(mCommandQueueCompute.ReleaseAndGetAddressOf())));
        mCommandQueueCompute->SetName(L"CommandQueueCompute");

        for (UINT n = 0; n < mBackBufferCount; n++)
        {
            ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(mCommandAllocatorsCompute[n].ReleaseAndGetAddressOf())));
        }

        ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, mCommandAllocatorsCompute[0].Get(), nullptr, IID_PPV_ARGS(mCommandListCompute.ReleaseAndGetAddressOf())));
		ThrowIfFailed(mCommandListCompute->Close());

		// Create a fence for async compute.
		ThrowIfFailed(mDevice->CreateFence(mFenceValuesCompute, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFenceCompute.ReleaseAndGetAddressOf())));
        mFenceValuesCompute++;
		mFenceEventCompute.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
		if (!mFenceEventCompute.IsValid())
		{
			throw std::exception("CreateEvent");
		}
        mFenceCompute->SetName(L"Compute fence");
    }

    // Create full screen quad
    {
        struct FullscreenVertex
        {
            XMFLOAT4 position;
            XMFLOAT2 uv;
        };

        // Define the geometry for a fullscreen triangle.
        FullscreenVertex quadVertices[] =
        {
            { { -1.0f, -1.0f, 0.0f, 1.0f },{ 0.0f, 1.0f } },       // Bottom left.
            { { -1.0f, 1.0f, 0.0f, 1.0f },{ 0.0f, 0.0f } },        // Top left.
            { { 1.0f, -1.0f, 0.0f, 1.0f },{ 1.0f, 1.0f } },        // Bottom right.
            { { 1.0f, 1.0f, 0.0f, 1.0f },{ 1.0f, 0.0f } },         // Top right.
            //{ { -1.0f, -1.0f, 0.0f, 1.0f },{ 0.0f, 0.0f } },       // Bottom left.
            //{ { -1.0f, 1.0f, 0.0f, 1.0f },{ 0.0f, 1.0f } },        // Top left.
            //{ { 1.0f, -1.0f, 0.0f, 1.0f },{ 1.0f, 0.0f } },        // Bottom right.
            //{ { 1.0f, 1.0f, 0.0f, 1.0f },{ 1.0f, 1.0f } },         // Top right.
        };

        const UINT vertexBufferSize = sizeof(quadVertices);

        ThrowIfFailed(mDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT /*D3D12_HEAP_TYPE_UPLOAD*/),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            /*D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER*/ D3D12_RESOURCE_STATE_COPY_DEST /*D3D12_RESOURCE_STATE_GENERIC_READ*/,
            nullptr,
            IID_PPV_ARGS(&mFullscreenQuadVertexBuffer)));

        ThrowIfFailed(mDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&mFullscreenQuadVertexBufferUpload)));

        // Copy data to the intermediate upload heap and then schedule a copy
        // from the upload heap to the vertex buffer.
        D3D12_SUBRESOURCE_DATA vertexData = {};
        vertexData.pData = reinterpret_cast<BYTE*>(quadVertices);
        vertexData.RowPitch = vertexBufferSize;
        vertexData.SlicePitch = vertexData.RowPitch;

        UpdateSubresources<1>(mCommandListGraphics.Get(), mFullscreenQuadVertexBuffer.Get(), mFullscreenQuadVertexBufferUpload.Get(), 0, 0, 1, &vertexData);
        mCommandListGraphics->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mFullscreenQuadVertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

        // Initialize the vertex buffer view.
        mFullscreenQuadVertexBufferView.BufferLocation = mFullscreenQuadVertexBuffer->GetGPUVirtualAddress();
        mFullscreenQuadVertexBufferView.StrideInBytes = sizeof(FullscreenVertex);
        mFullscreenQuadVertexBufferView.SizeInBytes = sizeof(quadVertices);
    }
}

void DX12DeviceResourcesSingleton::CreateWindowResources()
{
    if (mAppWindow == NULL)
    {
        throw std::exception("Call SetWindow with a valid Win32 window handle");
    }

    WaitForGPU();
    // Release resources that are tied to the swap chain and update fence values.
    for (UINT n = 0; n < mBackBufferCount; n++)
    {
        mRenderTargets[n].Reset();
    }

    // Determine the render target size in pixels.
    UINT backBufferWidth = std::max<UINT>(static_cast<UINT>(mOutputSize.right - mOutputSize.left), 1u);
    UINT backBufferHeight = std::max<UINT>(static_cast<UINT>(mOutputSize.bottom - mOutputSize.top), 1u);
    
    // If the swap chain already exists, resize it, otherwise create one.
    if (mSwapChain)
    {
        // If the swap chain already exists, resize it.
        HRESULT hr = mSwapChain->ResizeBuffers(
            mBackBufferCount,
            backBufferWidth,
            backBufferHeight,
            mBackBufferFormat,
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
        swapChainDesc.Format = mBackBufferFormat;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = mBackBufferCount;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        //DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
        //fsSwapChainDesc.Windowed = TRUE;

        // Create a swap chain for the window.
        ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailed(mDXGIFactory->CreateSwapChainForCoreWindow(
            mCommandQueueGraphics.Get(),
            reinterpret_cast<IUnknown*>(mAppWindow.Get()),
            &swapChainDesc,
            nullptr/*&fsSwapChainDesc*/,
            &swapChain));

        ThrowIfFailed(swapChain.As(&mSwapChain));
        //ThrowIfFailed(mDXGIFactory->MakeWindowAssociation(mAppWindow, DXGI_MWA_NO_ALT_ENTER));
    }

    // Obtain the back buffers for this window which will be the final render targets
    // and create render target views for each of them.
    for (UINT n = 0; n < mBackBufferCount; n++)
    {
        ThrowIfFailed(mSwapChain->GetBuffer(n, IID_PPV_ARGS(mRenderTargets[n].GetAddressOf())));

        wchar_t name[25] = {};
        swprintf_s(name, L"Main Render target %u", n);
        mRenderTargets[n]->SetName(name);

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = mBackBufferFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(mRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            static_cast<INT>(n), 
            mRTVDescriptorSize);
        mDevice->CreateRenderTargetView(mRenderTargets[n].Get(), &rtvDesc, rtvDescriptor);
    }

    // Reset the index to the current back buffer.
    mBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

    if (mDepthBufferFormat != DXGI_FORMAT_UNKNOWN)
    {
        // Allocate a 2-D surface as the depth/stencil buffer and create a depth/stencil view
        // on this surface.
        CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

        D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            mDepthBufferFormat,
            MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT,
            1, // This depth stencil view has only one texture.
            1  // Use a single mipmap level.
        );
        depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = mDepthBufferFormat;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        ThrowIfFailed(mDevice->CreateCommittedResource(
            &depthHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(mDepthStencilTarget.ReleaseAndGetAddressOf())
        ));

        mDepthStencilTarget->SetName(L"Depth-stencil target");

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = mDepthBufferFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

        mDevice->CreateDepthStencilView(mDepthStencilTarget.Get(), &dsvDesc, mDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // Set the 3D rendering viewport and scissor rectangle to target the entire window.
    mScreenViewport.TopLeftX = mScreenViewport.TopLeftY = 0.f;
    mScreenViewport.Width = static_cast<float>(backBufferWidth);
    mScreenViewport.Height = static_cast<float>(backBufferHeight);
    mScreenViewport.MinDepth = D3D12_MIN_DEPTH;
    mScreenViewport.MaxDepth = D3D12_MAX_DEPTH;

    mScissorRect.left = mScissorRect.top = 0;
    mScissorRect.right = static_cast<LONG>(backBufferWidth);
    mScissorRect.bottom = static_cast<LONG>(backBufferHeight);
}

void DX12DeviceResourcesSingleton::SetWindow(Windows::UI::Core::CoreWindow^ coreWindow)
{
    mAppWindow = coreWindow;

    mOutputSize.left = mOutputSize.top = 0;
    mOutputSize.right = coreWindow->Bounds.Width;
    mOutputSize.bottom = coreWindow->Bounds.Height;
}

bool DX12DeviceResourcesSingleton::OnWindowSizeChanged()
{
    RECT newRc;
    newRc.left = newRc.top = 0;
    newRc.right = mAppWindow->Bounds.Width;
    newRc.bottom = mAppWindow->Bounds.Height;
    if (newRc.left == mOutputSize.left
        && newRc.top == mOutputSize.top
        && newRc.right == mOutputSize.right
        && newRc.bottom == mOutputSize.bottom)
    {
        return false;
    }

    mOutputSize = newRc;
    CreateWindowResources();
    return true;
}

void DX12DeviceResourcesSingleton::TransitionMainRT(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES beforeState)
{
	if (beforeState != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		// Transition the render target into the correct state to allow for drawing into it.
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mBackBufferIndex].Get(), beforeState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList->ResourceBarrier(1, &barrier);
	}
}
void DX12DeviceResourcesSingleton::Present(D3D12_RESOURCE_STATES beforeState, bool needExecuteCmdList)
{
    if (needExecuteCmdList) 
    {

        if (beforeState != D3D12_RESOURCE_STATE_PRESENT)
        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mBackBufferIndex].Get(), beforeState, D3D12_RESOURCE_STATE_PRESENT);
            mCommandListGraphics->ResourceBarrier(1, &barrier);
        }

        ThrowIfFailed(mCommandListGraphics->Close());
        mCommandQueueGraphics->ExecuteCommandLists(1, CommandListCast(mCommandListGraphics.GetAddressOf()));
    }

    HRESULT hr;
    hr = mSwapChain->Present(1, 0);
    // If the device was reset we must completely reinitialize the renderer.
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
#ifdef _DEBUG
        char buff[64] = {};
        sprintf_s(buff, "Device Lost on Present: Reason code 0x%08X\n", (hr == DXGI_ERROR_DEVICE_REMOVED) ? mDevice->GetDeviceRemovedReason() : hr);
        OutputDebugStringA(buff);
#endif
        ThrowIfFailed(hr);
    }
    else
    {
        ThrowIfFailed(hr);
        MoveToNextFrame();

        if (!mDXGIFactory->IsCurrent())
            ThrowIfFailed(CreateDXGIFactory2(mDXGIFactoryFlags, IID_PPV_ARGS(mDXGIFactory.ReleaseAndGetAddressOf())));
    }
}

void DX12DeviceResourcesSingleton::WaitForComputeToFinish()
{
    assert(mCommandQueueGraphics && mFenceCompute/* && mFenceEventCompute.IsValid()*/);

	UINT64 fenceValue = mFenceValuesCompute - 1;
    mCommandQueueGraphics->Wait(mFenceCompute.Get(), fenceValue);

    UINT64 completedValue = mFenceCompute->GetCompletedValue();

	// wait until it is ready.
	if (completedValue < fenceValue)
	{
		ThrowIfFailed(mFenceCompute->SetEventOnCompletion(fenceValue, mFenceEventCompute.Get()));
		WaitForSingleObjectEx(mFenceEventCompute.Get(), INFINITE, FALSE);
	}

}

void DX12DeviceResourcesSingleton::SignalGraphicsFence()
{
	UINT64 fenceValue = mFenceValuesGraphics[mBackBufferIndex];
	mCommandQueueGraphics->Signal(mFenceGraphics.Get(), fenceValue);
    mFenceValuesGraphics[mBackBufferIndex]++;
}

void DX12DeviceResourcesSingleton::WaitForGraphicsToFinish()
{
	assert(mCommandQueueCompute && mFenceGraphics/* && mFenceEventCompute.IsValid()*/);
    mCommandQueueCompute->Wait(mFenceGraphics.Get(), mFenceValuesGraphics[mBackBufferIndex] - 1);
}

void DX12DeviceResourcesSingleton::PresentCompute()
{
    mCommandListCompute->Close();

	ID3D12CommandList* ppCommandLists[] = { mCommandListCompute.Get() };
	mCommandQueueCompute->ExecuteCommandLists(1, ppCommandLists);

	UINT64 fenceValue = mFenceValuesCompute;
    mCommandQueueCompute->Signal(mFenceCompute.Get(), fenceValue);
    mFenceValuesCompute++;
}

void DX12DeviceResourcesSingleton::WaitForGPU() 
{
    // Wait for graphics queue
    if (mCommandQueueGraphics && mFenceGraphics && mFenceEventGraphics.IsValid())
    {
        // Schedule a Signal command in the GPU queue.
        UINT64 fenceValue = mFenceValuesGraphics[mBackBufferIndex];
        if (SUCCEEDED(mCommandQueueGraphics->Signal(mFenceGraphics.Get(), fenceValue)))
        {
            // Wait until the Signal has been processed.
            if (SUCCEEDED(mFenceGraphics->SetEventOnCompletion(fenceValue, mFenceEventGraphics.Get())))
            {
                DisplayDebugMessage("@@@@@@@@@@@@ Starting Wait\n");
                WaitForSingleObjectEx(mFenceEventGraphics.Get(), INFINITE, FALSE);
                DisplayDebugMessage("@@@@@@@@@@@@ Finished Wait\n");

                // Increment the fence value for the current frame.
                mFenceValuesGraphics[mBackBufferIndex]++;
            }
        }
    }

    // Wait for compute queue
    if (mCommandQueueCompute && mFenceCompute && mFenceEventCompute.IsValid())
    {
        // Schedule a Signal command in the GPU queue.
        UINT64 fenceValue = mFenceValuesCompute;
        if (SUCCEEDED(mCommandQueueCompute->Signal(mFenceCompute.Get(), fenceValue)))
        {
            // Wait until the Signal has been processed.
            if (SUCCEEDED(mFenceCompute->SetEventOnCompletion(fenceValue, mFenceEventCompute.Get())))
            {
                DisplayDebugMessage("@@@@@@@@@@@@@\n Thread waiting is %d\n", GetCurrentThreadId());
                WaitForSingleObjectEx(mFenceEventCompute.Get(), INFINITE, FALSE);

                // Increment the fence value for the current frame.
                mFenceValuesCompute++;
            }
        }
    }
}

void DX12DeviceResourcesSingleton::MoveToNextFrame()
{
    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = mFenceValuesGraphics[mBackBufferIndex];
    ThrowIfFailed(mCommandQueueGraphics->Signal(mFenceGraphics.Get(), currentFenceValue));

    // Update the back buffer index.
    mBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (mFenceGraphics->GetCompletedValue() < mFenceValuesGraphics[mBackBufferIndex])
    {
        ThrowIfFailed(mFenceGraphics->SetEventOnCompletion(mFenceValuesGraphics[mBackBufferIndex], mFenceEventGraphics.Get()));
        WaitForSingleObjectEx(mFenceEventGraphics.Get(), INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    mFenceValuesGraphics[mBackBufferIndex] = currentFenceValue + 1;
}

void DX12DeviceResourcesSingleton::GetAdapter(IDXGIAdapter1** ppAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

#if defined(__dxgi1_6_h__) && defined(NTDDI_WIN10_RS4)
    ComPtr<IDXGIFactory6> factory6;
    HRESULT hr = mDXGIFactory.As(&factory6);
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
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), mD3DMinimumFeatureLevel, _uuidof(ID3D12Device), nullptr)))
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
            SUCCEEDED(mDXGIFactory->EnumAdapters1(
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
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), mD3DMinimumFeatureLevel, _uuidof(ID3D12Device), nullptr)))
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
        if (FAILED(mDXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))))
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
