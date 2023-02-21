#define NOMINMAX

#include "Mesh.h"

Mesh::Mesh(DX12DescriptorHeapManager* _descriptorHeapManager)
{
	// A cube has six faces, each one pointing in a different direction.
	const int FaceCount = 6;

	static const XMVECTORF32 faceNormals[FaceCount] =
	{
		{ 0,  0,  1 },
		{ 0,  0, -1 },
		{ 1,  0,  0 },
		{ -1,  0,  0 },
		{ 0,  1,  0 },
		{ 0, -1,  0 },
	};

	static const XMVECTORF32 textureCoordinates[4] =
	{
		{ 1, 0 },
		{ 1, 1 },
		{ 0, 1 },
		{ 0, 0 },
	};

	vertices.reserve(6 * 4);
	indices.reserve(6 * 6);

	float cubeHalfExtent = 1.0f;

	// Create each face in turn.
	DirectX::XMFLOAT3 defaultTangent = { 0.0f, 0.0f, 0.0f };
	for (int i = 0; i < FaceCount; i++)
	{
		XMVECTOR normal = faceNormals[i];

		// Get two vectors perpendicular both to the face normal and to each other.
		XMVECTOR basis = (i >= 4) ? g_XMIdentityR2 : g_XMIdentityR1;

		XMVECTOR side1 = XMVector3Cross(normal, basis);
		XMVECTOR side2 = XMVector3Cross(normal, side1);

		// Six indices (two triangles) per face.
		size_t vbase = vertices.size();
		indices.emplace_back(static_cast<uint16_t>(vbase + 0));
		indices.emplace_back(static_cast<uint16_t>(vbase + 1));
		indices.emplace_back(static_cast<uint16_t>(vbase + 2));

		indices.emplace_back(static_cast<uint16_t>(vbase + 0));
		indices.emplace_back(static_cast<uint16_t>(vbase + 2));
		indices.emplace_back(static_cast<uint16_t>(vbase + 3));

		// Four vertices per face.
		DirectX::XMFLOAT3 position;
		DirectX::XMStoreFloat3(&position, (normal - side1 - side2) * cubeHalfExtent);
		DirectX::XMFLOAT3 normalF32;
		DirectX::XMStoreFloat3(&normalF32, normal);
		DirectX::XMFLOAT2 uvF32;
		DirectX::XMStoreFloat2(&uvF32, textureCoordinates[0]);
		vertices.emplace_back(Vertex{ position, normalF32, defaultTangent, uvF32 });
		positions.push_back(position);
		normals.push_back(normalF32);
		tangents.push_back(defaultTangent);

		DirectX::XMStoreFloat3(&position, (normal - side1 + side2) * cubeHalfExtent);
		DirectX::XMStoreFloat2(&uvF32, textureCoordinates[1]);
		vertices.emplace_back(Vertex{ position, normalF32, defaultTangent, uvF32 });
		positions.push_back(position);
		normals.push_back(normalF32);
		tangents.push_back(defaultTangent);

		DirectX::XMStoreFloat3(&position, (normal + side1 + side2) * cubeHalfExtent);
		DirectX::XMStoreFloat2(&uvF32, textureCoordinates[2]);
		vertices.emplace_back(Vertex{ position, normalF32, defaultTangent, uvF32 });
		positions.push_back(position);
		normals.push_back(normalF32);
		tangents.push_back(defaultTangent);

		DirectX::XMStoreFloat3(&position, (normal + side1 - side2) * cubeHalfExtent);
		DirectX::XMStoreFloat2(&uvF32, textureCoordinates[3]);
		vertices.emplace_back(Vertex{ position, normalF32, defaultTangent, uvF32 });
		positions.push_back(position);
		normals.push_back(normalF32);
		tangents.push_back(defaultTangent);
	}

	// Make sure to invert the indices and vertices winding orders, to match
	// directX's left hand coordinate system
	ReverseWinding(indices, vertices);

	const UINT vertexBufferSize = static_cast<UINT>(vertices.size()) * sizeof(Vertex);
	const UINT indexBufferSize = indices.size() * sizeof(indices[0]);

	ID3D12Device* _d3dDevice = DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice();
	// Note: using upload heaps to transfer static data like vert buffers is not 
	// recommended. Every time the GPU needs it, the upload heap will be marshalled 
	// over. Please read up on Default Heap usage. An upload heap is used here for 
	// code simplicity and because there are very few verts to actually transfer.
	ThrowIfFailed(_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertex_buffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);

	ThrowIfFailed(vertex_buffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, &vertices[0], vertexBufferSize);
	vertex_buffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
	vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	vertex_buffer_view.SizeInBytes = vertexBufferSize;

	ThrowIfFailed(_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&index_buffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pIndexDataBegin;

	ThrowIfFailed(index_buffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
	memcpy(pIndexDataBegin, &indices[0], indexBufferSize);
	index_buffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view
	index_buffer_view.BufferLocation = index_buffer->GetGPUVirtualAddress();
	index_buffer_view.Format = DXGI_FORMAT_R32_UINT/*DXGI_FORMAT_R16_UINT*/;
	index_buffer_view.SizeInBytes = indexBufferSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Buffer.NumElements = indices.size();
	SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	index_buffer_srv = _descriptorHeapManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_d3dDevice->CreateShaderResourceView(index_buffer.Get(), &SRVDesc, index_buffer_srv.GetCPUHandle());

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDescVB = {};
	SRVDescVB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDescVB.Format = DXGI_FORMAT_UNKNOWN;
	SRVDescVB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDescVB.Buffer.NumElements = vertices.size();
	SRVDescVB.Buffer.StructureByteStride = sizeof(Vertex);
	SRVDescVB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	vertex_buffer_srv = _descriptorHeapManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_d3dDevice->CreateShaderResourceView(vertex_buffer.Get(), &SRVDescVB, vertex_buffer_srv.GetCPUHandle());
}

Mesh::Mesh(DX12DescriptorHeapManager* _descriptorHeapManager, FbxMesh* _mesh)
{
	_mesh->GenerateTangentsDataForAllUVSets();
	UINT counter = 0;
	for (int polygonIndex = 0; polygonIndex < _mesh->GetPolygonCount(); polygonIndex++)
	{
		for (int vertexIndex = 0; vertexIndex < 3; vertexIndex++)
		{
			int controlPointIndex = _mesh->GetPolygonVertex(polygonIndex, vertexIndex);
			FbxDouble* fbxPosition = &_mesh->GetControlPointAt(controlPointIndex).mData[0];
			DirectX::XMFLOAT3 position = DirectX::XMFLOAT3((float)fbxPosition[0], (float)fbxPosition[1], (float)fbxPosition[2]);

			FbxVector4 fbxNormal;
			_mesh->GetPolygonVertexNormal(polygonIndex, vertexIndex, fbxNormal);
			DirectX::XMFLOAT3 normal = DirectX::XMFLOAT3((float)fbxNormal.mData[0], (float)fbxNormal.mData[1], (float)fbxNormal.mData[2]);

			FbxGeometryElementTangent* fbxTangent = _mesh->GetElementTangent(0);
			DirectX::XMFLOAT3 tangent;
			switch (fbxTangent->GetMappingMode())
			{
				case FbxGeometryElement::eByControlPoint:
				{
					switch (fbxTangent->GetReferenceMode())
					{
						case FbxGeometryElement::eDirect:
						{
							tangent.x = (float)fbxTangent->GetDirectArray().GetAt(controlPointIndex).mData[0];
							tangent.y = (float)fbxTangent->GetDirectArray().GetAt(controlPointIndex).mData[1];
							tangent.z = (float)fbxTangent->GetDirectArray().GetAt(controlPointIndex).mData[2];
							break;
						}


						case FbxGeometryElement::eIndexToDirect:
						{
							int index = fbxTangent->GetIndexArray().GetAt(controlPointIndex);
							tangent.x = (float)fbxTangent->GetDirectArray().GetAt(index).mData[0];
							tangent.y = (float)fbxTangent->GetDirectArray().GetAt(index).mData[1];
							tangent.z = (float)fbxTangent->GetDirectArray().GetAt(index).mData[2];
							break;
						}

						default:
						{
							throw std::exception("Invalid Reference");
						}
					}

					break;
				}

				case FbxGeometryElement::eByPolygonVertex:
				{
					switch (fbxTangent->GetReferenceMode())
					{
						case FbxGeometryElement::eDirect:
						{
							tangent.x = (float)fbxTangent->GetDirectArray().GetAt(vertexIndex).mData[0];
							tangent.y = (float)fbxTangent->GetDirectArray().GetAt(vertexIndex).mData[1];
							tangent.z = (float)fbxTangent->GetDirectArray().GetAt(vertexIndex).mData[2];
							break;
						}

						case FbxGeometryElement::eIndexToDirect:
						{
							int index = fbxTangent->GetIndexArray().GetAt(vertexIndex);
							tangent.x = (float)fbxTangent->GetDirectArray().GetAt(index).mData[0];
							tangent.y = (float)fbxTangent->GetDirectArray().GetAt(index).mData[1];
							tangent.z = (float)fbxTangent->GetDirectArray().GetAt(index).mData[2];
							break;
						}

						default:
						{
							throw std::exception("Invalid Reference");
						}
					}

					break;
				}

			}

			FbxVector2 fbxUV;
			bool isUnmapped;
			FbxStringList uvSetNames;
			_mesh->GetUVSetNames(uvSetNames);
			_mesh->GetPolygonVertexUV(polygonIndex, vertexIndex, uvSetNames.GetStringAt(0), fbxUV, isUnmapped);
			DirectX::XMFLOAT2 uv = DirectX::XMFLOAT2((float)fbxUV.mData[0], (float)fbxUV.mData[1]);

			Vertex vertex
			{
				position,
				normal,
				tangent,
				uv
			};

			vertices.push_back(vertex);
			indices.push_back(counter);
			positions.push_back(position);
			normals.push_back(normal);
			tangents.push_back(tangent);
			counter++;
		}
	}

	const UINT vertexBufferSize = static_cast<UINT>(vertices.size()) * sizeof(Vertex);
	const UINT indexBufferSize = indices.size() * sizeof(indices[0]);

	ID3D12Device* _d3dDevice = DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice();
	// Note: using upload heaps to transfer static data like vert buffers is not 
	// recommended. Every time the GPU needs it, the upload heap will be marshalled 
	// over. Please read up on Default Heap usage. An upload heap is used here for 
	// code simplicity and because there are very few verts to actually transfer.
	ThrowIfFailed(_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertex_buffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);

	ThrowIfFailed(vertex_buffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, &vertices[0], vertexBufferSize);
	vertex_buffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
	vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	vertex_buffer_view.SizeInBytes = vertexBufferSize;

	ThrowIfFailed(_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&index_buffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pIndexDataBegin;

	ThrowIfFailed(index_buffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
	memcpy(pIndexDataBegin, &indices[0], indexBufferSize);
	index_buffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view
	index_buffer_view.BufferLocation = index_buffer->GetGPUVirtualAddress();
	index_buffer_view.Format = DXGI_FORMAT_R32_UINT/*DXGI_FORMAT_R16_UINT*/;
	index_buffer_view.SizeInBytes = indexBufferSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Buffer.NumElements = indices.size();
	SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	index_buffer_srv = _descriptorHeapManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_d3dDevice->CreateShaderResourceView(index_buffer.Get(), &SRVDesc, index_buffer_srv.GetCPUHandle());

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDescVB = {};
	SRVDescVB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDescVB.Format = DXGI_FORMAT_UNKNOWN;
	SRVDescVB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDescVB.Buffer.NumElements = vertices.size();
	SRVDescVB.Buffer.StructureByteStride = sizeof(Vertex);
	SRVDescVB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	vertex_buffer_srv = _descriptorHeapManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_d3dDevice->CreateShaderResourceView(vertex_buffer.Get(), &SRVDescVB, vertex_buffer_srv.GetCPUHandle());
}

Mesh::~Mesh()
{
	for (std::vector<XMFLOAT4>* vertexColors : P_vertex_colors)
	{
		delete vertexColors;
	}
}

const std::vector<XMFLOAT3>& Mesh::Vertices() const
{
	return positions;
}

const std::vector<XMFLOAT3>& Mesh::Normals() const
{
	return normals;
}

const std::vector<XMFLOAT3>& Mesh::Tangents() const
{
	return tangents;
}

const std::vector<std::vector<XMFLOAT4>*>& Mesh::VertexColors() const
{
	return P_vertex_colors;
}

UINT Mesh::FaceCount() const
{
	return face_count;
}

const std::vector<UINT>& Mesh::Indices() const
{
	return indices;
}

void Mesh::ReverseWinding(std::vector<UINT>& indices, std::vector<Vertex>& vertices)
{
	assert((indices.size() % 3) == 0);
	for (auto it = indices.begin(); it != indices.end(); it += 3)
	{
		std::swap(*it, *(it + 2));
	}

	for (auto it = vertices.begin(); it != vertices.end(); ++it)
	{
		it->texcoord.x = (1.f - it->texcoord.x);
	}
}