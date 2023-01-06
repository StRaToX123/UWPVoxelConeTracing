#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "c:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelDebugDraw_HF.hlsli"





Texture3D<float4> radiance_texture_3D_SRV : register(t4);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
RWStructuredBuffer<ShaderStructureGPUVoxelDebugData> voxel_debug_data_required_for_frame_draw : register(u0);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);

VoxelDebugDrawPixelShaderInput main(VoxelDebugDrawVertexShaderInput input)
{
	VoxelDebugDrawPixelShaderInput output;
	// Scale the voxel debug cube which by default has a half extent of 1.0f by the voxel half extent
	output.position = float4(sign(input.pos) * voxel_grid_data.voxel_half_extent, 1.0f);
	// Move the voxel debug cube to the first poisiton (index = 0, 0, 0)
	output.position.xyz += float3(voxel_grid_data.top_left_point_world_space.x + voxel_grid_data.voxel_half_extent, voxel_grid_data.top_left_point_world_space.y - voxel_grid_data.voxel_half_extent, voxel_grid_data.top_left_point_world_space.z + voxel_grid_data.voxel_half_extent);
	// Move the voxel debug cube to the apropriate position in the greed based on its index
	output.position.xyz += float3(voxel_debug_data_required_for_frame_draw[input.instance_id].index_x * voxel_grid_data.voxel_half_extent * 2.0f, -1.0f * (voxel_debug_data_required_for_frame_draw[input.instance_id].index_y * voxel_grid_data.voxel_half_extent * 2.0f), voxel_debug_data_required_for_frame_draw[input.instance_id].index_z * voxel_grid_data.voxel_half_extent * 2.0f);
	output.position = mul(output.position, view_projection_matrix_buffer.view);
	output.position = mul(output.position, view_projection_matrix_buffer.projection);
	
	
	output.color = radiance_texture_3D_SRV[uint3(voxel_debug_data_required_for_frame_draw[input.instance_id].index_x, voxel_debug_data_required_for_frame_draw[input.instance_id].index_y, voxel_debug_data_required_for_frame_draw[input.instance_id].index_z)];
	
	return output;
}