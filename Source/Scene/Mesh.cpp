#define NOMINMAX

#include "Mesh.h"


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

			mVertices.push_back(vertex);
			mIndices.push_back(counter);
			mPositions.push_back(position);
			mNormals.push_back(normal);
			mTangents.push_back(tangent);
			counter++;
		}
	}

	mNumOfIndices = mIndices.size();
	const UINT vertexBufferSize = static_cast<UINT>(mVertices.size()) * sizeof(Vertex);
	const UINT indexBufferSize = mNumOfIndices * sizeof(mIndices[0]);

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
		IID_PPV_ARGS(&mVertexBuffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);

	ThrowIfFailed(mVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, &mVertices[0], vertexBufferSize);
	mVertexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.StrideInBytes = sizeof(Vertex);
	mVertexBufferView.SizeInBytes = vertexBufferSize;

	ThrowIfFailed(_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mIndexBuffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pIndexDataBegin;

	ThrowIfFailed(mIndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
	memcpy(pIndexDataBegin, &mIndices[0], indexBufferSize);
	mIndexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view
	mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
	mIndexBufferView.Format = DXGI_FORMAT_R32_UINT/*DXGI_FORMAT_R16_UINT*/;
	mIndexBufferView.SizeInBytes = indexBufferSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Buffer.NumElements = mNumOfIndices;
	SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	mIndexBufferSRV = _descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_d3dDevice->CreateShaderResourceView(mIndexBuffer.Get(), &SRVDesc, mIndexBufferSRV.GetCPUHandle());

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDescVB = {};
	SRVDescVB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDescVB.Format = DXGI_FORMAT_UNKNOWN;
	SRVDescVB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDescVB.Buffer.NumElements = mVertices.size();
	SRVDescVB.Buffer.StructureByteStride = sizeof(Vertex);
	SRVDescVB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	mVertexBufferSRV = _descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_d3dDevice->CreateShaderResourceView(mVertexBuffer.Get(), &SRVDescVB, mVertexBufferSRV.GetCPUHandle());
}

Mesh::~Mesh()
{
	for (std::vector<XMFLOAT4>* vertexColors : mVertexColors)
	{
		delete vertexColors;
	}
}

const std::vector<XMFLOAT3>& Mesh::Vertices() const
{
	return mPositions;
}

const std::vector<XMFLOAT3>& Mesh::Normals() const
{
	return mNormals;
}

const std::vector<XMFLOAT3>& Mesh::Tangents() const
{
	return mTangents;
}

const std::vector<std::vector<XMFLOAT4>*>& Mesh::VertexColors() const
{
	return mVertexColors;
}

UINT Mesh::FaceCount() const
{
	return mFaceCount;
}

const std::vector<UINT>& Mesh::Indices() const
{
	return mIndices;
}