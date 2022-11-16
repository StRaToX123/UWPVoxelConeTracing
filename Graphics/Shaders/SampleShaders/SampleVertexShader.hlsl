#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\ShaderGlobalsCPUGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"


struct VertexShaderInput
{
	float3 pos : POSITION;
	float3 nor : NORMAL;
	float2 tex : TEXCOORD;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

ConstantBuffer<ShaderStructureGPURootConstants> root_constants : register(b0);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b1);
ConstantBuffer<ShaderStructureGPUTransformBuffer> transform_matrix_buffers[] : register(b2);

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	float4 pos = float4(input.pos, 1.0f);
	pos = mul(pos, transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].model);
	pos = mul(pos, view_projection_matrix_buffer.view);
	pos = mul(pos, view_projection_matrix_buffer.projection);
	output.pos = pos;
	
	output.tex = input.tex;
	
	return output;
}
