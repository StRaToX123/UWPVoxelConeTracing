#pragma once
#include <DirectXMath.h>
#include "Scene\SceneObject.h"
#include "Graphics/DirectX/DX12DeviceResourcesSingleton.h"
#include "Graphics/DirectX/DX12Buffer.h"

class DirectionalLight
{
	public:
		DirectionalLight();
		~DirectionalLight();

		void Initialize(DX12DescriptorHeapManager* _descriptorHeapManager,
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
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCBV();

		__declspec(align(16)) struct ShaderStructureCPUDirectionalLight
		{
			DirectX::XMFLOAT4X4 view_projection;
			DirectX::XMFLOAT4 inverse_direction_world_space; // Vector pointing towards the light
			DirectX::XMFLOAT4 color;
			float intensity;
			DirectX::XMFLOAT3 padding;
		};

		bool is_static;
		UINT8 most_updated_cbv_index;
		DirectX::XMFLOAT4 position_world_space;
		DirectX::XMFLOAT4 direction_world_space;
		DirectX::XMMATRIX projection_matrix;
		ShaderStructureCPUDirectionalLight shader_structure_cpu_directional_light_data;
		static const UINT c_aligned_shader_structure_cpu_directional_light_data = (sizeof(ShaderStructureCPUDirectionalLight) + 255) & ~255;
		DX12Buffer* p_constant_buffer;
};

