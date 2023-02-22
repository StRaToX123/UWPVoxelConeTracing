#pragma once

#include "Common.h"
#include "Graphics\DirectX\DX12Buffer.h"
#include "fbxsdk.h"


class DX12Buffer;

class Mesh
{
	public:
		// Default constructor creates a cube
		Mesh(DX12DescriptorHeapManager* _descriptorHeapManager, ID3D12GraphicsCommandList* _commandList, std::string& name);
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

		Mesh(DX12DescriptorHeapManager* _descriptorHeapManager, ID3D12GraphicsCommandList* _commandList, FbxMesh* _mesh, std::string& name);
		

		ID3D12Resource* GetVertexBuffer() { return p_vertex_buffer->GetResource(); }
		ID3D12Resource* GetIndexBuffer() { return p_index_buffer->GetResource(); }

		D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() { return vertex_buffer_view; }
		D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() { return index_buffer_view; }

		const std::vector<XMFLOAT3>& Vertices() const;
		const std::vector<XMFLOAT3>& Normals() const;
		const std::vector<XMFLOAT3>& Tangents() const;
		const std::vector<std::vector<XMFLOAT4>*>& VertexColors() const;
		UINT FaceCount() const;
		const std::vector<UINT>& Indices() const;

		UINT GetIndicesNum() { return indices.size(); }

		//DX12DescriptorHandleBlock& GetIndexBufferSRV() { return index_buffer_srv; }
		//DX12DescriptorHandleBlock& GetVertexBufferSRV() { return vertex_buffer_srv; }
		void ReverseWinding(std::vector<UINT>& indices, std::vector<Vertex>& vertices);

	private:
		void CreateVertexAndIndexBuffersAndViews(DX12DescriptorHeapManager* _descriptorHeapManager,
			ID3D12GraphicsCommandList* _commandList,
			std::string& name);

		std::vector<Vertex> vertices;
		std::vector<XMFLOAT3> positions;
		std::vector<XMFLOAT3> normals;
		std::vector<XMFLOAT3> tangents;
		std::vector<std::vector<XMFLOAT4>*> P_vertex_colors;
		UINT face_count;
		std::vector<UINT> indices;

		DX12Buffer* p_vertex_buffer;
		DX12Buffer* p_index_buffer;

		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
		D3D12_INDEX_BUFFER_VIEW index_buffer_view;

		//DX12DescriptorHandleBlock index_buffer_srv;
		//DX12DescriptorHandleBlock vertex_buffer_srv;
};
