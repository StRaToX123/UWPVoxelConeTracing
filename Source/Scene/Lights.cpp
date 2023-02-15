#include "Scene/Lights.h"



DirectionalLight::DirectionalLight()
{
	p_constant_buffer = nullptr;
}

DirectionalLight::~DirectionalLight()
{
	if (p_constant_buffer != nullptr)
	{
		delete p_constant_buffer;
	}
}

void DirectionalLight::Initialize(DX12DescriptorHeapManager* _descriptorHeapManager,
	DirectX::XMFLOAT4 directionWorldSpace,
	DirectX::XMFLOAT4 color,
	float intensity,
	float orthographicWidth,
	float orthographicHeight,
	float zNear,
	float zFar,
	bool isStatic)
{
	most_updated_cbv_index = DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetBackBufferCount() - 1;
	shader_structure_cpu_directional_light_data.color = color;
	shader_structure_cpu_directional_light_data.intensity = intensity;
	direction_world_space = directionWorldSpace;
	this->is_static = isStatic;
	projection_matrix = XMMatrixOrthographicLH(orthographicWidth, orthographicHeight, zNear, zFar);

	DX12Buffer::Description constantBufferDescriptor;
	constantBufferDescriptor.mElementSize = c_aligned_shader_structure_cpu_directional_light_data;
	constantBufferDescriptor.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
	constantBufferDescriptor.mDescriptorType = DX12Buffer::DescriptorType::CBV;

	p_constant_buffer = new DX12Buffer(_descriptorHeapManager, 
		constantBufferDescriptor, 
		DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetBackBufferCount(), // <- We need per frame data for the light
		L"Lighting Pass CB");
	UpdateBuffers();
}

void DirectionalLight::SetProjectionMatrix(float orthographicWidth, float orthographicHeight, float zNear, float zFar)
{
	projection_matrix = XMMatrixOrthographicLH(orthographicWidth, orthographicHeight, zNear, zFar);
}

void DirectionalLight::UpdateBuffers()
{
	XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(-position_world_space.x, -position_world_space.y, -position_world_space.z);
	XMVECTOR rotationQuaternion = XMQuaternionRotationMatrix(DirectX::XMMatrixTranspose(DirectX::XMMatrixLookToLH(DirectX::XMVectorSet(position_world_space.x, position_world_space.y, position_world_space.z, 1.0f), DirectX::XMVectorSet(direction_world_space.x, direction_world_space.y, direction_world_space.z, 1.0f), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f))));
	XMMATRIX rotationMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixRotationQuaternion(rotationQuaternion));
	XMMATRIX viewMatrix = DirectX::XMMatrixMultiply(translationMatrix, rotationMatrix);
	DirectX::XMStoreFloat4x4(&shader_structure_cpu_directional_light_data.view_projection, DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(viewMatrix, projection_matrix)));
	
	shader_structure_cpu_directional_light_data.inverse_direction_world_space.x = -direction_world_space.x;
	shader_structure_cpu_directional_light_data.inverse_direction_world_space.y = -direction_world_space.y;
	shader_structure_cpu_directional_light_data.inverse_direction_world_space.z = -direction_world_space.z;
	shader_structure_cpu_directional_light_data.inverse_direction_world_space.w = direction_world_space.w;

	most_updated_cbv_index++;
	if (most_updated_cbv_index == DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetBackBufferCount())
	{
		most_updated_cbv_index = 0;
	}

	memcpy(p_constant_buffer->GetMappedData(most_updated_cbv_index), &shader_structure_cpu_directional_light_data, sizeof(ShaderStructureCPUDirectionalLight));
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DirectionalLight::GetCBV()
{
	return p_constant_buffer->GetCBV(most_updated_cbv_index);
}