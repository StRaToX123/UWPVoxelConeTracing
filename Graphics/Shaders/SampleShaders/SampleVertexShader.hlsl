#include "ShaderGlobals.hlsli" // for the MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES macro



struct VertexShaderInput
{
	float3 pos : POSITION;
	float3 color : NORMAL;
	float2 tex : TEXCOORD;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

/*
struct ModelTransformMatrix
{
	matrix model;
	matrix subresource_padding_01;
	matrix subresource_padding_02;
	matrix subresource_padding_03;
	matrix subresource_padding_04;
	matrix subresource_padding_05;
	matrix subresource_padding_06;
	matrix subresource_padding_07;
};
*/

/*
struct ModelTransformMatrixBuffer
{
	ModelTransformMatrix data[100];
};
*/

cbuffer RootConstants : register(b0)
{
	uint model_transform_matrix_buffer_index;
	uint model_transform_matrix_buffer_inner_index;
};


cbuffer ViewProjectionConstantBuffer : register(b1)
{
	matrix view;
	matrix projection;
};

struct ModelTransformMarixBuffer
{
	matrix model[MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES];
};

ConstantBuffer<ModelTransformMarixBuffer> model_ransform_matrix_buffers[] : register(b2);

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	float4 pos = float4(input.pos, 1.0f);
	pos = mul(pos, model_ransform_matrix_buffers[model_transform_matrix_buffer_index].model[model_transform_matrix_buffer_inner_index]);
	pos = mul(pos, view);
	pos = mul(pos, projection);
	output.pos = pos;
	
	output.tex = input.tex;
	
	return output;
}
