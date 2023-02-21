#pragma once

#include "Common.h"
#include "Graphics\DirectX\DX12Buffer.h"
#include "fbxsdk.h"


class DX12Buffer;

class Mesh
{
	public:
		// Default constructor creates a cube
		Mesh(DX12DescriptorHeapManager* _descriptorHeapManager);
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

		Mesh(DX12DescriptorHeapManager* _descriptorHeapManager, FbxMesh* _mesh);
		

		ID3D12Resource* GetVertexBuffer() { return vertex_buffer.Get(); }
		ID3D12Resource* GetIndexBuffer() { return index_buffer.Get(); }

		D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() { return vertex_buffer_view; }
		D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() { return index_buffer_view; }

		const std::vector<XMFLOAT3>& Vertices() const;
		const std::vector<XMFLOAT3>& Normals() const;
		const std::vector<XMFLOAT3>& Tangents() const;
		const std::vector<std::vector<XMFLOAT4>*>& VertexColors() const;
		UINT FaceCount() const;
		const std::vector<UINT>& Indices() const;

		UINT GetIndicesNum() { return indices.size(); }

		DX12DescriptorHandleBlock& GetIndexBufferSRV() { return index_buffer_srv; }
		DX12DescriptorHandleBlock& GetVertexBufferSRV() { return vertex_buffer_srv; }
		void ReverseWinding(std::vector<UINT>& indices, std::vector<Vertex>& vertices);

	private:
		std::vector<Vertex> vertices;
		std::vector<XMFLOAT3> positions;
		std::vector<XMFLOAT3> normals;
		std::vector<XMFLOAT3> tangents;
		std::vector<std::vector<XMFLOAT4>*> P_vertex_colors;
		UINT face_count;
		std::vector<UINT> indices;

		ComPtr<ID3D12Resource> vertex_buffer;
		ComPtr<ID3D12Resource> index_buffer;

		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
		D3D12_INDEX_BUFFER_VIEW index_buffer_view;

		DX12DescriptorHandleBlock index_buffer_srv;
		DX12DescriptorHandleBlock vertex_buffer_srv;
};
