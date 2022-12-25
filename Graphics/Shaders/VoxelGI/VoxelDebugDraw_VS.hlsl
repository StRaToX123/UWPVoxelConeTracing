#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "c:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelDebugDraw_HF.hlsli"




//RWTexture3D<float4> radiance_texture_3D_UAVs : register(u4);
//Texture3D<min16float4> radiance_texture_3D_SRVs[] : register(t4);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
RWStructuredBuffer<ShaderStructureGPUVoxelDebugData> voxel_debug_data_required_for_frame_draw : register(u0);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);

VoxelDebugDrawPixelShaderInput main(VoxelDebugDrawVertexShaderInput input)
{
	
	VoxelDebugDrawPixelShaderInput output;
	// Scale the voxel debug cube which by default has a half extent of 1.0f by the voxel half extent
	output.position = float4(sign(input.pos) * voxel_grid_data.voxel_half_extent, 1.0f);
	// Move the voxel debug cube to the first poisiton (index = 0, 0, 0)
	output.position += float4(voxel_grid_data.bottom_left_point_world_space_x + voxel_grid_data.voxel_half_extent, voxel_grid_data.bottom_left_point_world_space_y + voxel_grid_data.voxel_half_extent, voxel_grid_data.bottom_left_point_world_space_z + voxel_grid_data.voxel_half_extent, 0.0f);
	// Move the voxel debug cube to the apropriate position in the greed based on its index
	output.position += float4(voxel_debug_data_required_for_frame_draw[input.instance_id].index_x * voxel_grid_data.voxel_half_extent * 2.0f, voxel_debug_data_required_for_frame_draw[input.instance_id].index_y * voxel_grid_data.voxel_half_extent * 2.0f, voxel_debug_data_required_for_frame_draw[input.instance_id].index_z * voxel_grid_data.voxel_half_extent * 2.0f, 0.0f);
	output.position = mul(output.position, view_projection_matrix_buffer.view);
	output.position = mul(output.position, view_projection_matrix_buffer.projection);
	
	//output.color = radiance_texture_3D_UAVs[uint3(voxel_debug_data_required_for_frame_draw[input.instance_id].index_x, voxel_debug_data_required_for_frame_draw[input.instance_id].index_y, voxel_debug_data_required_for_frame_draw[input.instance_id].index_z)];
	output.color = float4(voxel_debug_data_required_for_frame_draw[input.instance_id].color_r, voxel_debug_data_required_for_frame_draw[input.instance_id].color_g, voxel_debug_data_required_for_frame_draw[input.instance_id].color_b, 1.0f);
	
	return output;
	
	
	/*
	VoxelDebugDrawPixelShaderInput output;
	// Scale the voxel debug cube which by default has a half extent of 1.0f by the voxel half extent
	output.position = float4(sign(input.pos) * voxel_grid_data.voxel_half_extent * 2.0f, 1.0f);
	// Move the voxel debug cube to the first poisiton (index = 0, 0, 0)
	output.position += float4(voxel_grid_data.bottom_left_point_world_space_x + voxel_grid_data.voxel_half_extent * 2.0f, voxel_grid_data.bottom_left_point_world_space_y + voxel_grid_data.voxel_half_extent * 2.0f, voxel_grid_data.bottom_left_point_world_space_z + voxel_grid_data.voxel_half_extent * 2.0f, 0.0f);
	// Move the voxel debug cube to the apropriate position in the greed based on its index
	output.position += float4(voxel_debug_data_required_for_frame_draw[input.instance_id].index_x * voxel_grid_data.voxel_half_extent * 4.0f, voxel_debug_data_required_for_frame_draw[input.instance_id].index_y * voxel_grid_data.voxel_half_extent * 4.0f, voxel_debug_data_required_for_frame_draw[input.instance_id].index_z * voxel_grid_data.voxel_half_extent * 4.0f, 0.0f);
	output.position = mul(output.position, view_projection_matrix_buffer.view);
	output.position = mul(output.position, view_projection_matrix_buffer.projection);
	
	output.color = radiance_texture_3D_UAVs[1][uint3(voxel_debug_data_required_for_frame_draw[input.instance_id].index_x, voxel_debug_data_required_for_frame_draw[input.instance_id].index_y, voxel_debug_data_required_for_frame_draw[input.instance_id].index_z)];
	
	return output;
	*/
	
}