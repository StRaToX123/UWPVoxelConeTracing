#include <Scene/Camera.h>


Camera::Camera() :
    is_dirty_view_matrix(true),
    is_dirty_projection_matrix(true),
    vertical_fov_degrees(45.0f),
    aspect_ratio(1.0f),
    z_near(0.1f),
    z_far(100.0f),
    view_projection_constant_mapped_buffer(nullptr),
    is_initialized(false)
{
    
}


Camera::~Camera()
{
    if (is_initialized == true)
    {
        _aligned_free(p_data);
        view_projection_constant_buffer->Unmap(0, nullptr);
        view_projection_constant_mapped_buffer = nullptr;
    }
}

void Camera::Initialize(const std::shared_ptr<DeviceResources>& deviceResources)
{
    device_resources = deviceResources;
    p_data = (AlignedData*)_aligned_malloc(sizeof(AlignedData), 16);
    p_data->translation = XMVectorZero();
    p_data->rotation_quaternion = XMQuaternionIdentity();
    std::ZeroMemory(&view_projection_constant_buffer_data, sizeof(view_projection_constant_buffer_data));

    auto d3dDevice = device_resources->GetD3DDevice();
    // Create this cameras descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = c_frame_count;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptor_heap_cbv_srv_uav)));
    NAME_D3D12_OBJECT(descriptor_heap_cbv_srv_uav);

#pragma region Camera View Projection Buffer
    // Create the camera view projection constant buffer
    CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(c_frame_count * c_aligned_view_projection_matrix_constant_buffer);
    ThrowIfFailed(d3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&view_projection_constant_buffer)));


    NAME_D3D12_OBJECT(view_projection_constant_buffer);

    // Create constant buffer views to access the upload buffer.
    D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = view_projection_constant_buffer->GetGPUVirtualAddress();
    CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHeapCpuHandle;
    descriptorHeapCpuHandle.InitOffsetted(descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart(), 0);
    for (int n = 0; n < c_frame_count; n++)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
        desc.BufferLocation = cbvGpuAddress;
        desc.SizeInBytes = c_aligned_view_projection_matrix_constant_buffer;
        d3dDevice->CreateConstantBufferView(&desc, descriptorHeapCpuHandle);

        cbvGpuAddress += desc.SizeInBytes;
        descriptorHeapCpuHandle.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
    }

    // Map the constant buffers.
    // We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.
    ThrowIfFailed(view_projection_constant_buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&view_projection_constant_mapped_buffer)));
    std::ZeroMemory(view_projection_constant_mapped_buffer, c_frame_count * c_aligned_view_projection_matrix_constant_buffer);
#pragma endregion

    is_initialized = true;
}

void Camera::CopyDescriptorsIntoDescriptorHeap(CD3DX12_CPU_DESCRIPTOR_HANDLE& destinationDescriptorHandle)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart(), device_resources->GetCurrentFrameIndex() * device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
    device_resources->GetD3DDevice()->CopyDescriptorsSimple(1, destinationDescriptorHandle, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    destinationDescriptorHandle.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
}

void Camera::UpdateGPUBuffers()
{
    // Update the mapped viewProjection constant buffer
    if (is_dirty_view_matrix)
    {
        UpdateViewMatrix();
    }

    UINT8* _destination = view_projection_constant_mapped_buffer + (device_resources->GetCurrentFrameIndex() * c_aligned_view_projection_matrix_constant_buffer);
    std::memcpy(_destination, &view_projection_constant_buffer_data, sizeof(ShaderStructureCPUViewProjectionBuffer));
}

void XM_CALLCONV Camera::SetLookAt(FXMVECTOR eye, FXMVECTOR target, FXMVECTOR up)
{
    p_data->view_matrix = XMMatrixLookAtLH(eye, target, up);
    p_data->translation = eye;
    p_data->rotation_quaternion = XMQuaternionRotationMatrix(XMMatrixTranspose(p_data->view_matrix));

    is_dirty_view_matrix = false;
}

XMMATRIX Camera::GetViewMatrix()
{
    if (is_dirty_view_matrix)
    {
        UpdateViewMatrix();
    }
    return p_data->view_matrix;
}


void Camera::SetProjection(float fovy, float aspect, float zNear, float zFar)
{
    vertical_fov_degrees = fovy;
    aspect_ratio = aspect;
    z_near = zNear;
    z_far = zFar;
    is_dirty_projection_matrix = true;
}

XMMATRIX Camera::GetProjectionMatrix()
{
    if (is_dirty_projection_matrix)
    {
        UpdateProjectionMatrix();
    }

    return p_data->projection_matrix;
}


void Camera::SetFoV(float fovyInDegrees)
{
    if (vertical_fov_degrees != fovyInDegrees)
    {
        vertical_fov_degrees = fovyInDegrees;
        is_dirty_projection_matrix = true;
    }
}

float Camera::GetFoV() const
{
    return vertical_fov_degrees;
}


void XM_CALLCONV Camera::SetTranslation(FXMVECTOR translation)
{
    p_data->translation = translation;
    is_dirty_view_matrix = true;
}

XMVECTOR Camera::GetTranslation() const
{
    return p_data->translation;
}

void Camera::SetRotation(FXMVECTOR quaternion)
{
    p_data->rotation_quaternion = quaternion;
    is_dirty_view_matrix = true;
}

XMVECTOR Camera::GetRotationQuaternion() const
{
    return p_data->rotation_quaternion;
}

void XM_CALLCONV Camera::Translate(FXMVECTOR translation, Space space)
{
    switch ( space )
    {
    case Space::Local:
        {
            p_data->translation += XMVector3Rotate( translation, p_data->rotation_quaternion );
        }
        break;
    case Space::World:
        {
            p_data->translation += translation;
        }
        break;
    }

    p_data->translation = XMVectorSetW( p_data->translation, 1.0f );
    is_dirty_view_matrix = true;
}

void Camera::Rotate(FXMVECTOR quaternion)
{
    p_data->rotation_quaternion = XMQuaternionMultiply(p_data->rotation_quaternion, quaternion);
    is_dirty_view_matrix = true;
}

void Camera::UpdateViewMatrix()
{
    XMMATRIX rotationMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(p_data->rotation_quaternion));
    XMMATRIX translationMatrix = XMMatrixTranslationFromVector(-(p_data->translation));

    p_data->view_matrix = translationMatrix * rotationMatrix;
    DirectX::XMStoreFloat4x4(&view_projection_constant_buffer_data.view, XMMatrixTranspose(p_data->view_matrix));
    is_dirty_view_matrix = false;
}

void Camera::UpdateProjectionMatrix()
{
    p_data->projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(vertical_fov_degrees), aspect_ratio, z_near, z_far);
    is_dirty_projection_matrix = false;
}
