#pragma once

namespace VoxelConeTracing
{
	// Constant buffer used to send MVP matrices to the vertex shader.
	struct ModelViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	struct ViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	#define PER_SCENE_OBJECT_MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES 100
	struct PerSceneObjectModelTransformMatrixBuffer
	{
		DirectX::XMFLOAT4X4 model[PER_SCENE_OBJECT_MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES];
	};

	// Used to send per-vertex data to the vertex shader.
	struct VertexPositionColor
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 color;
	};
}