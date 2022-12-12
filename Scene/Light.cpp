#include "Scene/Light.h"

SpotLight::SpotLight() :
    is_initialized(false)
{

}

SpotLight::~SpotLight()
{
    if (is_initialized == true)
    {
        resource_constant_buffer_data->Unmap(0, nullptr);
        constant_mapped_buffer_data = nullptr;
        delete[] pName;
    }
}

void SpotLight::Initialize(const WCHAR* name, UINT64 shadowMapWidth, UINT shadowMapHeight, const std::shared_ptr<DeviceResources>& deviceResources)
{
    device_resources = deviceResources;
    pName = name;
    auto d3dDevice = device_resources->GetD3DDevice();
    // Create this light descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = (c_frame_count * 3); // each constant buffer, shadow map texture and depth buffer have a per frame descriptor
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptor_heap_cbv_srv_uav)));
    NAME_D3D12_OBJECT(descriptor_heap_cbv_srv_uav);

    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHeapCpuHandle;
    descriptorHeapCpuHandle.InitOffsetted(descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart(), 0);

    // Create descriptor heaps for render target views, this heap will hold the shadow map rtvs
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = c_frame_count;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtv_heap)));
    rtv_heap->SetName((wstring(L"SpotLight ") + wstring(name) + wstring(L"rtv_heap")).c_str());

    // Create a depth stencil heap to hold all the depth buffers
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = c_frame_count;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsv_heap)));
    dsv_heap->SetName((wstring(L"SpotLight ") + wstring(name) + wstring(L"dsv_heap")).c_str());

    // Setup the scissor rect
    scissor_rect = { 0, 0, static_cast<LONG>(shadowMapWidth), static_cast<LONG>(shadowMapHeight) };
    // Setup the viewport
    viewport.Width = shadowMapWidth;
    viewport.Height = shadowMapHeight;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

#pragma region Constant Buffer Data
    // Create the camera view projection constant buffer
    CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(c_frame_count * c_aligned_constant_buffer_data);
    ThrowIfFailed(d3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource_constant_buffer_data)));

    resource_constant_buffer_data->SetName((wstring(L"SpotLight ") + name + wstring(L" resource_constant_buffer_data")).c_str());

    // Create constant buffer views to access the upload buffer.
    D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = resource_constant_buffer_data->GetGPUVirtualAddress();
    for (int n = 0; n < c_frame_count; n++)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
        desc.BufferLocation = cbvGpuAddress;
        desc.SizeInBytes = c_aligned_constant_buffer_data;
        d3dDevice->CreateConstantBufferView(&desc, descriptorHeapCpuHandle);

        cbvGpuAddress += desc.SizeInBytes;
        descriptorHeapCpuHandle.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
    }

    // Map the constant buffers.
    // We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.
    ThrowIfFailed(resource_constant_buffer_data->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&constant_mapped_buffer_data)));
    std::ZeroMemory(constant_mapped_buffer_data, c_frame_count * c_aligned_constant_buffer_data);
    UpdateConstantBuffers();
#pragma endregion

    UpdateShadowMapBuffers(shadowMapWidth, shadowMapHeight);
    for (int i = 0; i < c_frame_count; i++)
    {
        texture_shadow_map[i]->SetName((wstring(L"SpotLight ") + wstring(name) + wstring(L"texture_shadow_map[") + to_wstring(i) + wstring(L"]")).c_str());
        texture_shadow_map[i]->SetName((wstring(L"SpotLight ") + wstring(name) + wstring(L"depth_buffer_shadow_map[") + to_wstring(i) + wstring(L"]")).c_str());
    }

    is_initialized = true;
}

void SpotLight::UpdateConstantBuffers()
{
    most_updated_constant_buffer_data_index = device_resources->GetCurrentFrameIndex();
    UINT8* _destination = constant_mapped_buffer_data + (most_updated_constant_buffer_data_index * c_aligned_constant_buffer_data);
    std::memcpy(_destination, &constant_buffer_data, sizeof(ShaderStructureCPUSpotLight));
}

void SpotLight::UpdateShadowMapBuffers(UINT64 shadowMapWidth, UINT shadowMapHeight)
{
    auto d3dDevice = device_resources->GetD3DDevice();
    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvUavCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart(), c_frame_count * device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());

#pragma region Texture Shadow Map
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapCpuHandle(rtv_heap->GetCPUDescriptorHandleForHeapStart());
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    for (int i = 0; i < c_frame_count; i++)
    {
        D3D12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT,
            shadowMapWidth,
            shadowMapHeight,
            1);
        textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            nullptr,
            IID_PPV_ARGS(&texture_shadow_map[i])));
        
        // RTV
        d3dDevice->CreateRenderTargetView(texture_shadow_map[i].Get(), &rtvDesc, rtvHeapCpuHandle);
        rtvHeapCpuHandle.Offset(d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

        // SRV
        device_resources->GetD3DDevice()->CreateShaderResourceView(texture_shadow_map[i].Get(), &srvDesc, cbvSrvUavCpuHandle);
        cbvSrvUavCpuHandle.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
    }
#pragma endregion

#pragma region Depth Buffer Shadow Map
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHeapCpuHandle(dsv_heap->GetCPUDescriptorHandleForHeapStart());
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = device_resources->GetDepthBufferFormat();
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDescDepthBuffer = {};
    srvDescDepthBuffer.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDescDepthBuffer.Format = DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
    srvDescDepthBuffer.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDescDepthBuffer.Texture2D.MipLevels = 1;
    srvDescDepthBuffer.Texture2D.MostDetailedMip = 0;
    Windows::Foundation::Size backBufferDimensions = device_resources->GetOutputSize();
    for (int i = 0; i < c_frame_count; i++)
    {
        D3D12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, 
            backBufferDimensions.Width, 
            backBufferDimensions.Height, 
            1, 
            1);
        depthResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        CD3DX12_CLEAR_VALUE depthOptimizedClearValue(device_resources->GetDepthBufferFormat(), 1.0f, 0);

        ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &depthResourceDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(&depth_buffer_shadow_map[i])
        ));

        // Create a dsv for the shadow pass depth buffer
        d3dDevice->CreateDepthStencilView(depth_buffer_shadow_map[i].Get(), &dsvDesc, dsvHeapCpuHandle);
        dsvHeapCpuHandle.Offset(d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

        // Create a srv for the depth buffer, so we may read it in the shadow map shader
        device_resources->GetD3DDevice()->CreateShaderResourceView(depth_buffer_shadow_map[i].Get(), &srvDescDepthBuffer, cbvSrvUavCpuHandle);
        cbvSrvUavCpuHandle.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
    }
#pragma endregion

}

void SpotLight::CopyDescriptorsIntoDescriptorHeap(CD3DX12_CPU_DESCRIPTOR_HANDLE& destinationDescriptorHandle, bool copyShadowMapSRV, bool copyDepthBufferSRV)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart(), most_updated_constant_buffer_data_index * device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
    device_resources->GetD3DDevice()->CopyDescriptorsSimple(1, destinationDescriptorHandle, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    destinationDescriptorHandle.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
    if (copyShadowMapSRV == true)
    {
        cpuHandle.Offset(((c_frame_count - most_updated_constant_buffer_data_index) * device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav()) + (device_resources->GetCurrentFrameIndex() * device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav()));
        device_resources->GetD3DDevice()->CopyDescriptorsSimple(1, destinationDescriptorHandle, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        destinationDescriptorHandle.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
    }
    
    if (copyDepthBufferSRV == true)
    {
        cpuHandle.Offset(((c_frame_count - device_resources->GetCurrentFrameIndex()) * device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav()) + (device_resources->GetCurrentFrameIndex() * device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav()));
        device_resources->GetD3DDevice()->CopyDescriptorsSimple(1, destinationDescriptorHandle, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        destinationDescriptorHandle.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE SpotLight::GetShadowMapRenderTargetView()
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(rtv_heap->GetCPUDescriptorHandleForHeapStart(), device_resources->GetCurrentFrameIndex() * device_resources->GetDescriptorSizeDescriptorHeapRtv());
}

D3D12_CPU_DESCRIPTOR_HANDLE SpotLight::GetShadowMapDepthStencilView()
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(dsv_heap->GetCPUDescriptorHandleForHeapStart(), device_resources->GetCurrentFrameIndex() * device_resources->GetDescriptorSizeDescriptorHeapDsv());
}