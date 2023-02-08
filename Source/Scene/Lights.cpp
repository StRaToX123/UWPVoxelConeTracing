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

void DirectionalLight::Initialize(DX12DeviceResourcesSingleton* _deviceResources, 
	DX12DescriptorHeapManager* _descriptorHeapManager,
	DirectX::XMFLOAT4 directionWorldSpace,
	DirectX::XMFLOAT4 color,
	float intensity,
	float orthographicWidth,
	float orthographicHeight,
	float zNear,
	float zFar,
	bool isStatic)
{
	data.color = color;
	data.intensity = intensity;
	direction_world_space = directionWorldSpace;
	this->is_static = isStatic;
	projection_matrix = XMMatrixOrthographicLH(orthographicWidth, orthographicHeight, zNear, zFar);

	DX12Buffer::Description constantBufferDescriptor;
	constantBufferDescriptor.mElementSize = sizeof(ShaderStructureCPUDirectionalLight);
	constantBufferDescriptor.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
	constantBufferDescriptor.mDescriptorType = DX12Buffer::DescriptorType::CBV;

	p_constant_buffer = new DX12Buffer(_deviceResources->GetD3DDevice(), _descriptorHeapManager, _deviceResources->GetCommandListGraphics(), constantBufferDescriptor, L"Lighting Pass CB");
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
	DirectX::XMStoreFloat4x4(&data.view_projection, DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(viewMatrix, projection_matrix)));
	
	data.inverse_direction_world_space.x = -direction_world_space.x;
	data.inverse_direction_world_space.y = -direction_world_space.y;
	data.inverse_direction_world_space.z = -direction_world_space.z;
	data.inverse_direction_world_space.w = direction_world_space.w;
	memcpy(p_constant_buffer->Map(), &data, sizeof(ShaderStructureCPUDirectionalLight));
}