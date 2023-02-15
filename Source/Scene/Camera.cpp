#include "Scene\Camera.h"


Camera::Camera() :
    is_dirty_view_matrix(true),
    is_dirty_projection_matrix(true),
    vertical_fov_degrees(45.0f),
    aspect_ratio(1.0f),
    z_near(0.1f),
    z_far(100.0f),
    p_constant_buffer(nullptr),
    is_initialized(false),
    rotation_local_space_quaternion(DirectX::XMQuaternionIdentity())
{
    position_world_space = DirectX::XMVectorSet(0.0f, 0.0f, 0.0, 1.0f);
}


Camera::~Camera()
{
    if (is_initialized == true)
    {
        delete p_constant_buffer;
    }
}

void Camera::Initialize(DX12DescriptorHeapManager* _descriptorHeapManager)
{
    rotation_local_space_quaternion = DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), XM_PI);
    DX12DeviceResourcesSingleton* _dx12DeviceResourcesSingleton = DX12DeviceResourcesSingleton::GetDX12DeviceResources();
    most_updated_cbv_index = _dx12DeviceResourcesSingleton->GetBackBufferCount() - 1;
    DX12Buffer::Description constantBufferDescriptor;
    constantBufferDescriptor.mElementSize = c_aligned_shader_structure_cpu_camera;
    constantBufferDescriptor.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
    constantBufferDescriptor.mDescriptorType = DX12Buffer::DescriptorType::CBV;
    p_constant_buffer = new DX12Buffer(_descriptorHeapManager,
        constantBufferDescriptor, 
        _dx12DeviceResourcesSingleton->GetBackBufferCount(), // <- we need a copy of the constant buffer per frame
        L"Camera Constant Buffer");

    UpdateBuffers();
    is_initialized = true;
}


void Camera::UpdateBuffers()
{
    if (is_dirty_projection_matrix == true)
    {
        UpdateProjectionMatrix();
    }


    // Update the mapped viewProjection constant buffer
    if (is_dirty_view_matrix == true)
    {
        UpdateViewMatrix();
    }

    most_updated_cbv_index++;
    if (most_updated_cbv_index == DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetBackBufferCount())
    {
        most_updated_cbv_index = 0;
    }

    DirectX::XMStoreFloat3(&shader_structure_cpu_camera_data.position_world_space, position_world_space);
    memcpy(p_constant_buffer->GetMappedData(most_updated_cbv_index), &shader_structure_cpu_camera_data, sizeof(ShaderStructureCPUCamera));
}

//void XM_CALLCONV Camera::SetLookAt(FXMVECTOR eye, FXMVECTOR target, FXMVECTOR up)
//{
//    p_data->view_matrix = DirectX::XMMatrixLookAtLH(eye, target, up);
//    p_data->translation = eye;
//    p_data->rotation_quaternion = DirectX::XMQuaternionRotationMatrix(DirectX::XMMatrixTranspose(p_data->view_matrix));
//    is_dirty_view_matrix = false;
//}

XMMATRIX Camera::GetViewMatrix()
{
    if (is_dirty_view_matrix)
    {
        UpdateViewMatrix();
    }
    return view_matrix;
}


void Camera::SetProjection(float fovy, float aspect, float zNear, float zFar)
{
    vertical_fov_degrees = fovy;
    aspect_ratio = aspect;
    z_near = zNear;
    z_far = zFar;
    is_dirty_projection_matrix = true;
}

DirectX::XMMATRIX Camera::GetProjectionMatrix()
{
    if (is_dirty_projection_matrix)
    {
        UpdateProjectionMatrix();
    }

    return projection_matrix;
}


void Camera::SetFoVYDeggrees(float fovyInDegrees)
{
    if (vertical_fov_degrees != fovyInDegrees)
    {
        vertical_fov_degrees = fovyInDegrees;
        is_dirty_projection_matrix = true;
    }
}

float Camera::GetFoVYDegrees() const
{
    return vertical_fov_degrees;
}


void XM_CALLCONV Camera::SetPositionWorldSpace(DirectX::XMVECTOR positionWorldSpace)
{
    position_world_space = positionWorldSpace;
    is_dirty_view_matrix = true;
}

DirectX::XMVECTOR Camera::GetPositionWorldSpace() const
{
    return position_world_space;
}

void Camera::SetRotationLocalSpaceQuaternion(FXMVECTOR rotationLocalSpaceQuaternion)
{
    rotation_local_space_quaternion = rotationLocalSpaceQuaternion;
    is_dirty_view_matrix = true;
}

XMVECTOR Camera::GetRotationLocalSpaceQuaternion() const
{
    return rotation_local_space_quaternion;
}

void XM_CALLCONV Camera::Translate(FXMVECTOR translation, Space space)
{
    switch ( space )
    {
        case Space::Local:
        {
            position_world_space += DirectX::XMVector3Rotate(translation, rotation_local_space_quaternion);
            break;
        }  
        case Space::World:
        {
            position_world_space += translation;
            break;
        }
    }

    is_dirty_view_matrix = true;
}

//void Camera::Rotate(FXMVECTOR quaternion)
//{
//    p_data->rotation_quaternion = DirectX::XMQuaternionMultiply(p_data->rotation_quaternion, quaternion);
//    is_dirty_view_matrix = true;
//}

void Camera::UpdateViewMatrix()
{
    XMMATRIX rotationMatrix = DirectX::XMMatrixRotationQuaternion(rotation_local_space_quaternion);
    //XMMATRIX inverseRotationMatrix = rotationMatrix; // We dont use the camera->worldSpace matrix
    rotationMatrix = DirectX::XMMatrixTranspose(rotationMatrix);
    position_world_space = DirectX::XMVectorSetW(position_world_space, 1.0f);
    XMMATRIX translationMatrix = DirectX::XMMatrixTranslationFromVector(-position_world_space);
    //XMMATRIX inverseTranslationMatrix = DirectX::XMMatrixTranslationFromVector(position_world_space); // We dont use the camera->worldSpace matrix
    view_matrix = DirectX::XMMatrixMultiply(translationMatrix, rotationMatrix);

    DirectX::XMStoreFloat4x4(&shader_structure_cpu_camera_data.view_projection, DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(view_matrix, projection_matrix)));
    is_dirty_view_matrix = false;
}

void Camera::UpdateProjectionMatrix()
{
    projection_matrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(vertical_fov_degrees), aspect_ratio, z_near, z_far);
    is_dirty_projection_matrix = false;
}


CD3DX12_CPU_DESCRIPTOR_HANDLE Camera::GetCBV()
{
    return p_constant_buffer->GetCBV(most_updated_cbv_index);
}