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

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;
using namespace DirectX;


class Model
{
	__declspec(align(16)) struct ShaderStructureCPUModelData
	{
		XMFLOAT4X4	model;
		XMFLOAT4	diffuse_color;
	};

	public:
		Model(DX12DescriptorHeapManager* _descriptorHeapManager,
			DirectX::XMVECTOR positionWorldSpace,
			DirectX::XMVECTOR rotationLocalSpaceQuaternion,
			XMFLOAT4 color = XMFLOAT4(1, 0, 1, 1),
			bool isDynamic = false,
			float speed = 0.0f,
			float amplitude = 1.0f);
		Model(DX12DescriptorHeapManager* _descriptorHeapManager, 
			const std::string& filename, 
			DirectX::XMVECTOR positionWorldSpace,
			DirectX::XMVECTOR rotationLocalSpaceQuaternion,
			XMFLOAT4 color = XMFLOAT4(1, 0, 1, 1),
			bool isDynamic = false, 
			float speed = 0.0f, 
			float amplitude = 1.0f);
		~Model();

		void UpdateBuffers();
		DX12Buffer* GetCB() { return p_constant_buffer; }
		bool HasMeshes() const;
		bool HasMaterials() const;

		void Render(ID3D12GraphicsCommandList* commandList);

		const std::vector<Mesh*>& Meshes() const;
		const std::string GetFileName() { return mFilename; }
		const char* GetFileNameChar() { return mFilename.c_str(); }
		std::vector<XMFLOAT3> GenerateAABB();

		XMFLOAT4 GetDiffuseColor() { return shader_structure_cpu_model_data.diffuse_color; }
		bool GetIsDynamic() { return mIsDynamic; }
		float GetSpeed() { return mSpeed; }
		float GetAmplitude() { return mAmplitude; }

		Model& operator=(const Model& rhs);

		DirectX::XMVECTOR position_world_space;
		DirectX::XMVECTOR rotation_local_space_quaternion;
	private:
		Model(const Model& rhs);
	
		ShaderStructureCPUModelData shader_structure_cpu_model_data;
		static const UINT c_aligned_shader_structure_cpu_model_data = (sizeof(ShaderStructureCPUModelData) + 255) & ~255;
		DX12Buffer* p_constant_buffer;

		std::vector<Mesh*> mMeshes;
		// If this bool is set to true, then that means this models is referencing someone elses mesh data.
		// So since it doesn't own it, this bool will signal to us that we should't delete the mesh data
		// inside of the destructor
		bool referenced_meshes;
		std::string mFilename;

		XMFLOAT4 mDiffuseColor;
		bool mIsDynamic;
		float mSpeed;
		float mAmplitude;
};

