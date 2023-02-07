#pragma once
#include <DirectXMath.h>
#include "Scene\SceneObject.h"
#include "Graphics/DirectX/DX12DeviceResources.h"
#include "Graphics/DirectX/DX12Buffer.h"

class DirectionalLight
{
	public:
		DirectionalLight();
		~DirectionalLight();

		void Initialize(DX12DeviceResources* _deviceResources,
			DX12DescriptorHeapManager* _descriptorHeapManager,
			DirectX::XMFLOAT4 directionWorldSpace = { -0.191, -1.0f, -0.574f, 1.0 },
			DirectX::XMFLOAT4 color = { 0.9, 0.9, 0.9, 1.0 },
			float intensity = 3.0f,
			float orthographicWidth = 256.0f,
			float orthographicHeight = 256.0f,
			float zNear = -256.0f,
			float zFar = 256.0f,
			bool isStatic = false);
		void SetProjectionMatrix(float orthographicWidth = 256.0f,
			float orthographicHeight = 256.0f,
			float zNear = -256.0f,
			float zFar = 256.0f);
		void UpdateBuffers();

		__declspec(align(16)) struct ShaderStructureCPUDirectionalLight
		{
			DirectX::XMFLOAT4X4 view_projection;
			DirectX::XMFLOAT4 inverse_direction_world_space; // Vector pointing towards the light
			DirectX::XMFLOAT4 color;
			float intensity;
			DirectX::XMFLOAT3 padding;
		};

		bool is_static;
		DirectX::XMFLOAT4 position_world_space;
		DirectX::XMFLOAT4 direction_world_space;
		DirectX::XMMATRIX projection_matrix;
		ShaderStructureCPUDirectionalLight data;
		DX12Buffer* p_constant_buffer;
};

