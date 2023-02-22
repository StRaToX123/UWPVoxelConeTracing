#define NOMINMAX

#include "Mesh.h"

Mesh::Mesh(DX12DescriptorHeapManager* _descriptorHeapManager, ID3D12GraphicsCommandList* _commandList, std::string& name)
{
	p_vertex_buffer = nullptr;
	p_index_buffer = nullptr;
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
	CreateVertexAndIndexBuffersAndViews(_descriptorHeapManager, _commandList, name);
}

Mesh::Mesh(DX12DescriptorHeapManager* _descriptorHeapManager, ID3D12GraphicsCommandList* _commandList, FbxMesh* _mesh, std::string& name)
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

	CreateVertexAndIndexBuffersAndViews(_descriptorHeapManager, _commandList, name);
}

Mesh::~Mesh()
{
	for (std::vector<XMFLOAT4>* vertexColors : P_vertex_colors)
	{
		delete vertexColors;
	}

	if (p_vertex_buffer != nullptr)
	{
		delete p_vertex_buffer;
	}

	if (p_index_buffer != nullptr)
	{
		delete p_index_buffer;
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

void Mesh::CreateVertexAndIndexBuffersAndViews(DX12DescriptorHeapManager* _descriptorHeapManager, 
	ID3D12GraphicsCommandList* _commandList, 
	std::string& name)
{
	std::wstring vertexBufferName(name.begin(), name.end());
	std::wstring indexBufferName = vertexBufferName;
	vertexBufferName += L" VertexBuffer";
	indexBufferName += L" IndexBuffer";
	const UINT vertexBufferSize = ((UINT)vertices.size()) * sizeof(Vertex);
	const UINT indexBufferSize = indices.size() * sizeof(indices[0]);

	// Create the vertex buffer
	DX12Buffer::Description vertexBufferDesc;
	vertexBufferDesc.element_size = vertexBufferSize;
	vertexBufferDesc.state = D3D12_RESOURCE_STATE_GENERIC_READ;
	p_vertex_buffer = new DX12Buffer(_descriptorHeapManager,
		vertexBufferDesc,
		1,
		vertexBufferName.c_str(),
		&vertices[0],
		vertexBufferSize,
		_commandList);

	// Create the vertex buffer view
	vertex_buffer_view.BufferLocation = p_vertex_buffer->GetResource()->GetGPUVirtualAddress();
	vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	vertex_buffer_view.SizeInBytes = vertexBufferSize;

	// Create the index buffer
	DX12Buffer::Description indexBufferDesc;
	indexBufferDesc.element_size = indexBufferSize;
	indexBufferDesc.state = D3D12_RESOURCE_STATE_GENERIC_READ;
	p_index_buffer = new DX12Buffer(_descriptorHeapManager,
		indexBufferDesc,
		1,
		indexBufferName.c_str(),
		&indices[0],
		indexBufferSize,
		_commandList);

	index_buffer_view.BufferLocation = p_index_buffer->GetResource()->GetGPUVirtualAddress();
	index_buffer_view.SizeInBytes = indexBufferSize;
	index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
}