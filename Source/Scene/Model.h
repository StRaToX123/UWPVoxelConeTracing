#pragma once

#include "Common.h"
#include "Graphics\DirectX\DX12DeviceResources.h"
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
	__declspec(align(16)) struct ModelConstantBuffer
	{
		XMFLOAT4X4	World;
		XMFLOAT4	DiffuseColor;
	};

	public:
		Model(DX12DeviceResources* _dxrsGraphics,
			DX12DescriptorHeapManager* _descriptorHeapManager,
			XMMATRIX tranformWorld = XMMatrixIdentity(),
			XMFLOAT4 color = XMFLOAT4(1, 0, 1, 1),
			bool isDynamic = false,
			float speed = 0.0f,
			float amplitude = 1.0f);
		Model(DX12DeviceResources* _dxrsGraphics,
			DX12DescriptorHeapManager* _descriptorHeapManager, 
			const std::string& filename, 
			XMMATRIX tranformWorld = XMMatrixIdentity(),
			XMFLOAT4 color = XMFLOAT4(1, 0, 1, 1),
			bool isDynamic = false, 
			float speed = 0.0f, 
			float amplitude = 1.0f);
		~Model();

		void UpdateWorldMatrix(XMMATRIX matrix);
		DX12Buffer* GetCB() { return mBufferCB; }
		bool HasMeshes() const;
		bool HasMaterials() const;

		void Render(ID3D12GraphicsCommandList* commandList);

		const std::vector<Mesh*>& Meshes() const;
		const std::string GetFileName() { return mFilename; }
		const char* GetFileNameChar() { return mFilename.c_str(); }
		std::vector<XMFLOAT3> GenerateAABB();
		XMMATRIX GetWorldMatrix() { return mWorldMatrix; }
		XMFLOAT3 GetTranslation();

		XMFLOAT4 GetDiffuseColor() { return mDiffuseColor; }
		bool GetIsDynamic() { return mIsDynamic; }
		float GetSpeed() { return mSpeed; }
		float GetAmplitude() { return mAmplitude; }

		Model& operator=(const Model& rhs);
	private:
		Model(const Model& rhs);
	

		DX12Buffer* mBufferCB;

		std::vector<Mesh*> mMeshes;
		// If this bool is set to true, then that means this models is referencing someone elses mesh data.
		// So since it doesn't own it, this bool will signal to us that we should't delete the mesh data
		// inside of the destructor
		bool referenced_meshes;
		std::string mFilename;

		XMFLOAT4 mDiffuseColor;
		XMMATRIX mWorldMatrix = XMMatrixIdentity();
		bool mIsDynamic;
		float mSpeed;
		float mAmplitude;
};

