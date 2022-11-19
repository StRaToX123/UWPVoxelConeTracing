#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"

struct VertexShaderInput
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float2 tex_coord : TEXCOORD;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 col : COLOR;
};


RWTexture3D<float4> radiance_texture_3d : register(u2);
ConstantBuffer<ShaderStructureGPUVoxelDebugData> voxel_debug_data : register(b0);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);

float4 main(VertexShaderInput input)
{
	PixelShaderInput output;
	float4 pos = float4(input.pos, 1.0f);
	pos = mul(pos, voxel_debug_data.model);
	pos = mul(pos, view_projection_matrix_buffer.view);
	pos = mul(pos, view_projection_matrix_buffer.projection);
	output.pos = pos;
	
	output.col = radiance_texture_3d[UnFlatten1DTo3DIndex(voxel_debug_data.voxel_index, voxel_grid_data.res)];
	
	return output;
}