#pragma once

#include "Common.h"
#include "Graphics\DirectX\DX12Buffer.h"
#include "fbxsdk.h"


class DX12Buffer;

class Mesh
{
	public:
		Mesh(const Mesh& rhs);
		~Mesh();

		struct Vertex
		{
			XMFLOAT3 position;
			XMFLOAT3 normal;
			XMFLOAT3 tangent;
			XMFLOAT2 texcoord;
		};

		struct MeshInfo
		{
			XMFLOAT4 color;
		};

		Mesh(ID3D12Device* _device, DX12DescriptorHeapManager* _descriptorHeapManager, FbxMesh* _mesh);
		

		ID3D12Resource* GetVertexBuffer() { return mVertexBuffer.Get(); }
		ID3D12Resource* GetIndexBuffer() { return mIndexBuffer.Get(); }

		D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() { return mVertexBufferView; }
		D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() { return mIndexBufferView; }

		const std::vector<XMFLOAT3>& Vertices() const;
		const std::vector<XMFLOAT3>& Normals() const;
		const std::vector<XMFLOAT3>& Tangents() const;
		const std::vector<std::vector<XMFLOAT4>*>& VertexColors() const;
		UINT FaceCount() const;
		const std::vector<UINT>& Indices() const;

		UINT GetIndicesNum() { return mIndices.size(); }

		DX12DescriptorHandle& GetIndexBufferSRV() { return mIndexBufferSRV; }
		DX12DescriptorHandle& GetVertexBufferSRV() { return mVertexBufferSRV; }

	private:
		std::vector<Vertex> mVertices;
		std::vector<XMFLOAT3> mPositions;
		std::vector<XMFLOAT3> mNormals;
		std::vector<XMFLOAT3> mTangents;
		std::vector<std::vector<XMFLOAT4>*> mVertexColors;
		UINT mFaceCount;
		std::vector<UINT> mIndices;

		UINT mNumOfIndices;

		ComPtr<ID3D12Resource> mVertexBuffer;
		ComPtr<ID3D12Resource> mIndexBuffer;

		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

		DX12DescriptorHandle mIndexBufferSRV;
		DX12DescriptorHandle mVertexBufferSRV;
};
