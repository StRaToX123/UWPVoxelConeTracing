#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"





//RWTexture3D<float4> radiance_texture_3d : register(u3);
RWStructuredBuffer<VoxelType> voxel_data_structured_buffer : register(u2);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
RWStructuredBuffer<ShaderStructureGPUVoxelDebugData> voxel_debug_data_required_for_frame_draw : register(u0);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);

VoxelDebugDrawPixelShaderInput main(VoxelDebugDrawVertexShaderInput input)
{
	VoxelDebugDrawPixelShaderInput output;
	float4 pos = float4(input.pos * voxel_grid_data.voxel_extent, 1.0f);
	pos = mul(pos, voxel_debug_data_required_for_frame_draw[input.instance_id].model);
	pos = mul(pos, view_projection_matrix_buffer.view);
	pos = mul(pos, view_projection_matrix_buffer.projection);
	output.position = pos;
	
	//output.color = radiance_texture_3d[UnFlatten1DTo3DIndex(voxel_debug_data_required_for_frame_draw[input.instance_id].voxel_index, voxel_grid_data.res)];
	output.color = UnpackVoxelColor(voxel_data_structured_buffer[voxel_debug_data_required_for_frame_draw[input.instance_id].voxel_index].color);
	
	return output;
}