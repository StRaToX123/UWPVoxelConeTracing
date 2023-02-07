#define NOMINMAX

#include "Model.h"

Model::Model(DeviceResources* _dxrsGraphics,
	DX12DescriptorHeapManager* _descriptorHeapManager,
	XMMATRIX transformWorld,
	XMFLOAT4 color,
	bool isDynamic,
	float speed,
	float amplitude)
{
	referenced_meshes = false;
	mWorldMatrix = transformWorld;
	mIsDynamic = isDynamic;
	mSpeed = speed;
	mAmplitude = amplitude;
	mDiffuseColor = color;

	DX12Buffer::Description desc;
	desc.mElementSize = sizeof(ModelConstantBuffer);
	desc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
	desc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

	mBufferCB = new DX12Buffer(_dxrsGraphics->GetD3DDevice(), _descriptorHeapManager, _dxrsGraphics->GetCommandListGraphics(), desc, L"Model CB");

	ModelConstantBuffer cbData = {};
	XMStoreFloat4x4(&cbData.World, XMMatrixTranspose(mWorldMatrix));
	cbData.DiffuseColor = color;
	memcpy(mBufferCB->Map(), &cbData, sizeof(cbData));
}

Model::Model(DeviceResources* _dxrsGraphics, 
	DX12DescriptorHeapManager* _descriptorHeapManager, 
	const std::string& filename, 
	XMMATRIX transformWorld, 
	XMFLOAT4 color,
	bool isDynamic, 
	float speed, 
	float amplitude)
{
	referenced_meshes = false;
	mFilename = filename;
	mWorldMatrix = transformWorld;
	mIsDynamic = isDynamic;
	mSpeed = speed;
	mAmplitude = amplitude;
	mDiffuseColor = color;

	// Create the FBX SDK manager
	FbxManager* pFbxManager = FbxManager::Create();
	// Create an IOSettings object.
	FbxIOSettings* pIOsettings = FbxIOSettings::Create(pFbxManager, IOSROOT);
	pFbxManager->SetIOSettings(pIOsettings);
	// Create an importer.
	FbxImporter* pImporter = FbxImporter::Create(pFbxManager, "");
	// Initialize the importer.
	bool lImportStatus = pImporter->Initialize(filename.c_str(), -1, pFbxManager->GetIOSettings());
	if (!lImportStatus)
	{
		printf("Call to FbxImporter::Initialize() failed.\n");
		printf("Error returned: %s\n\n", pImporter->GetStatus().GetErrorString());
		exit(-1);
	}

	// Create a new scene so it can be populated by the imported file.
	FbxScene* pScene = FbxScene::Create(pFbxManager, "myScene");
	// Import the contents of the file into the scene.
	if (!pImporter->Import(pScene))
	{
		printf("Call to pImporter->Import(pScene) failed.\n");
		printf("Error returned: %s\n\n", pImporter->GetStatus().GetErrorString());
		exit(-1);
	}

	// The file has been imported; we can get rid of the importer.
	pImporter->Destroy();

	FbxGeometryConverter fbxGeomConverter(pFbxManager);
	FbxAxisSystem::DirectX.ConvertScene(pScene);
	if (pScene->GetGeometryCount())
	{
		for (UINT i = 0; i < pScene->GetGeometryCount(); i++)
		{
			Mesh* pMesh = new Mesh(_dxrsGraphics->GetD3DDevice(), _descriptorHeapManager, static_cast<FbxMesh*>(fbxGeomConverter.Triangulate(static_cast<FbxNodeAttribute*>(pScene->GetGeometry(i)), true, true)));
			mMeshes.push_back(pMesh);
		}
	}




	DX12Buffer::Description desc;
	desc.mElementSize = sizeof(ModelConstantBuffer);
	desc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
	desc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

	mBufferCB = new DX12Buffer(_dxrsGraphics->GetD3DDevice(), _descriptorHeapManager, _dxrsGraphics->GetCommandListGraphics(), desc, L"Model CB");

	ModelConstantBuffer cbData = {};
	XMStoreFloat4x4(&cbData.World, XMMatrixTranspose(mWorldMatrix));
	cbData.DiffuseColor = color;
	memcpy(mBufferCB->Map(), &cbData, sizeof(cbData));
}

Model::~Model()
{
	if (referenced_meshes == false)
	{
		for (Mesh* mesh : mMeshes)
		{
			delete mesh;
		}
	}
	
	delete mBufferCB;
}

void Model::UpdateWorldMatrix(XMMATRIX matrix)
{
	mWorldMatrix = matrix;

	ModelConstantBuffer cbData = {};
	XMStoreFloat4x4(&cbData.World, XMMatrixTranspose(mWorldMatrix));
	cbData.DiffuseColor = mDiffuseColor;
	memcpy(mBufferCB->Map(), &cbData, sizeof(cbData));
}

bool Model::HasMeshes() const
{
	return (mMeshes.size() > 0);
}

bool Model::HasMaterials() const
{
	//return (mMaterials.size() > 0);
	return false;
}

void Model::Render(ID3D12GraphicsCommandList* commandList)
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (Mesh* mesh : mMeshes)
	{
		commandList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
		commandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
		commandList->DrawIndexedInstanced(mesh->GetIndicesNum(), 1, 0, 0, 0);
	}
}

const std::vector<Mesh*>& Model::Meshes() const
{
	return mMeshes;
}

//const std::vector<DXRSModelMaterial*>& DXRSModel::Materials() const
//{
//	return mMaterials;
//}

std::vector<XMFLOAT3> Model::GenerateAABB()
{
	std::vector<XMFLOAT3> vertices;

	for (Mesh* mesh : mMeshes)
	{
		vertices.insert(vertices.end(), mesh->Vertices().begin(), mesh->Vertices().end());
	}


	XMFLOAT3 minVertex = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
	XMFLOAT3 maxVertex = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (UINT i = 0; i < vertices.size(); i++)
	{

		//Get the smallest vertex 
		minVertex.x = std::min(minVertex.x, vertices[i].x);    // Find smallest x value in model
		minVertex.y = std::min(minVertex.y, vertices[i].y);    // Find smallest y value in model
		minVertex.z = std::min(minVertex.z, vertices[i].z);    // Find smallest z value in model

		//Get the largest vertex 
		maxVertex.x = std::max(maxVertex.x, vertices[i].x);    // Find largest x value in model
		maxVertex.y = std::max(maxVertex.y, vertices[i].y);    // Find largest y value in model
		maxVertex.z = std::max(maxVertex.z, vertices[i].z);    // Find largest z value in model
	}

	std::vector<XMFLOAT3> AABB;

	// Our AABB [0] is the min vertex and [1] is the max
	AABB.push_back(minVertex);
	AABB.push_back(maxVertex);

	return AABB;
}

XMFLOAT3 Model::GetTranslation() {
	XMVECTOR translation, rot, scale;
	XMMatrixDecompose(&scale, &rot, &translation, mWorldMatrix);

	XMFLOAT3 translationF;
	XMStoreFloat3(&translationF, translation);
	return translationF;
}

Model& Model::operator=(const Model& rhs)
{
	this->mMeshes = rhs.mMeshes;
	this->mFilename = rhs.mFilename;
	referenced_meshes = true;
	return *this;
}