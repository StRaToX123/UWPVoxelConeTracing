#pragma once


// Constant buffer used to send MVP matrices to the vertex shader.
struct ShaderStructureModelViewProjectionConstantBuffer
{
	DirectX::XMFLOAT4X4 model;
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
};

struct ShaderStructureViewProjectionConstantBuffer
{
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
};

#define MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES 100

// Needs to be 256 byte alligned in order to be reference as a subresource
struct ShaderStructureModelTransformMatrix
{
	XMFLOAT4X4 model;
	XMFLOAT4X4 subresource_padding_01;
	XMFLOAT4X4 subresource_padding_02;
	XMFLOAT4X4 subresource_padding_03;
};

struct ShaderStructureModelTransformMatrixBuffer
{
	ShaderStructureModelTransformMatrix data[MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES];
};

// Used to send per-vertex data to the vertex shader.
struct ShaderStructureVertexPositionColor
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 color;
};
