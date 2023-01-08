#include "c:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelDebugDraw_HF.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\RadianceGenerate3DMipChain_HF.hlsli"


ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
RWStructuredBuffer<ShaderStructureGPUVoxelDebugData> voxel_debug_data_required_for_frame_draw : register(u0);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);
ConstantBuffer<ShaderStructureGPUGenerate3DMipChainData> generate_3d_mip_chain_data : register(b4);


VoxelDebugDrawPixelShaderInput main(VoxelDebugDrawVertexShaderInput input)
{
	VoxelDebugDrawPixelShaderInput output;
	// Scale the voxel debug cube which by default has a half extent of 1.0f
	const float mipVoxelExtent = voxel_grid_data.grid_extent * generate_3d_mip_chain_data.output_resolution_rcp;
	const float mipVoxelHalfExtent = mipVoxelExtent / 2.0f;
	output.position = float4(sign(input.pos) * mipVoxelHalfExtent, 1.0f);
	// Move the voxel debug cube to the first poisiton (index = 0, 0, 0)
	output.position.xyz += float3(voxel_grid_data.top_left_point_world_space.x + mipVoxelHalfExtent, voxel_grid_data.top_left_point_world_space.y - mipVoxelHalfExtent, voxel_grid_data.top_left_point_world_space.z + mipVoxelHalfExtent);
	// Move the voxel debug cube to the apropriate position in the greed based on its index
	output.position.xyz += float3(voxel_debug_data_required_for_frame_draw[input.instance_id].index.x * mipVoxelExtent, -1.0f * (voxel_debug_data_required_for_frame_draw[input.instance_id].index.y * mipVoxelExtent), voxel_debug_data_required_for_frame_draw[input.instance_id].index.z * mipVoxelExtent);
	output.position = mul(output.position, view_projection_matrix_buffer.view);
	output.position = mul(output.position, view_projection_matrix_buffer.projection);
	
	
	output.color = voxel_debug_data_required_for_frame_draw[input.instance_id].color;
	
	return output;
}