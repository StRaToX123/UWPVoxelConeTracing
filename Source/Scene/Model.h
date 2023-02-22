#pragma once

#include "Common.h"
#include "Graphics\DirectX\DX12DeviceResourcesSingleton.h"
#include "Graphics\DirectX\DX12Buffer.h"
#include "Scene\Mesh.h"
#include "fbxsdk.h"
#include "fbxsdk/fileio/fbxiosettings.h"
#include "fbxsdk/scene/fbxaxissystem.h"
#include "fbxsdk/utils/fbxgeometryconverter.h"
#include <map>
#include <wrl.h>

using namespace Microsoft::WRL;
using namespace DirectX;


class Model
{
	__declspec(align(16)) struct ShaderStructureCPUModelData
	{
		DirectX::XMFLOAT4X4	model;
		DirectX::XMFLOAT4	diffuse_color;
	};

	public:
		Model(DX12DescriptorHeapManager* _descriptorHeapManager,
			ID3D12GraphicsCommandList* _commandList,
			DirectX::XMVECTOR positionWorldSpace,
			DirectX::XMVECTOR rotationLocalSpaceQuaternion,
			XMFLOAT4 color = XMFLOAT4(1, 0, 1, 1),
			bool isDynamic = false,
			float speed = 0.0f,
			float amplitude = 1.0f,
			bool initializeAsACube = true);
		Model(DX12DescriptorHeapManager* _descriptorHeapManager, 
			ID3D12GraphicsCommandList* _commandList,
			std::string& filename, 
			DirectX::XMVECTOR positionWorldSpace,
			DirectX::XMVECTOR rotationLocalSpaceQuaternion,
			XMFLOAT4 color = XMFLOAT4(1, 0, 1, 1),
			bool isDynamic = false, 
			float speed = 0.0f, 
			float amplitude = 1.0f);
		~Model();

		void UpdateBuffers();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCBV();
		bool HasMeshes() const;
		bool HasMaterials() const;

		void Render(ID3D12GraphicsCommandList* commandList);

		const std::vector<Mesh*>& Meshes() const;
		const std::string GetFileName() { return file_name; }
		const char* GetFileNameChar() { return file_name.c_str(); }
		std::vector<XMFLOAT3> GenerateAABB();

		XMFLOAT4 GetDiffuseColor() { return shader_structure_cpu_model_data.diffuse_color; }
		bool GetIsDynamic() { return is_dynamic; }
		float GetSpeed() { return speed; }
		float GetAmplitude() { return amplitude; }

		Model& operator=(const Model& rhs);

		DirectX::XMVECTOR position_world_space;
		DirectX::XMVECTOR rotation_local_space_quaternion;
	private:
		Model(const Model& rhs);
	
		ShaderStructureCPUModelData shader_structure_cpu_model_data;
		static const UINT c_aligned_shader_structure_cpu_model_data = (sizeof(ShaderStructureCPUModelData) + 255) & ~255;
		DX12Buffer* p_constant_buffer;

		std::vector<Mesh*> P_meshes;
		// If this bool is set to true, then that means this models is referencing someone elses mesh data.
		// So since it doesn't own it, this bool will signal to us that we should't delete the mesh data
		// inside of the destructor
		bool referenced_meshes;
		std::string file_name;

		XMFLOAT4 diffuse_color;
		bool is_dynamic;
		UINT8 most_updated_cbv_index;
		float speed;
		float amplitude;
};

