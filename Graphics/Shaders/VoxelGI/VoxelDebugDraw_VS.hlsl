#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"

struct VertexShaderInput
{
	float3 pos : POSITION;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 col : COLOR;
};



ConstantBuffer<ShaderStructureGPUVoxelDebugData> voxel_debug_data : register(b0);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b1);
RWStructuredBuffer<VoxelType> voxel_data_structured_buffer : register(u0);

float4 main(VertexShaderInput input)
{
	PixelShaderInput output;
	float4 pos = float4(input.pos, 1.0f);
	pos = mul(pos, voxel_debug_data.model);
	pos = mul(pos, view_projection_matrix_buffer.view);
	pos = mul(pos, view_projection_matrix_buffer.projection);
	output.pos = pos;
	
	output.col = UnpackVoxelColor(voxel_data_structured_buffer[voxel_debug_data.voxel_index].color);
	
	return output;
}