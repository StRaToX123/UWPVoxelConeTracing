#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\Voxelizer_HF.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\SpotLight_HF.hlsli"

ConstantBuffer<ShaderStructureGPURootConstants> root_constants : register(b0);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);
ConstantBuffer<ShaderStructureGPUSpotLight> spot_light_buffer : register(b3);
ConstantBuffer<ShaderStructureGPUTransformBuffer> transform_matrix_buffers[] : register(b5);

VertexShaderOutputVoxelizer main(VertexShaderInputDefault input)
{
	VertexShaderOutputVoxelizer output;
	matrix modelTransformMatrix = transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].model;
	output.position_world_space = mul(float4(input.position, 1.0f), modelTransformMatrix).xyz;
	output.position_view_space = mul(float4(output.position_world_space, 1.0f), view_projection_matrix_buffer.view).xyz;
	output.position_spot_light_view_proj_space = mul(float4(output.position_world_space, 1.0f), spot_light_buffer.spotlight_view_matrix);
	output.position_spot_light_view_proj_space = mul(output.position_spot_light_view_proj_space, spot_light_buffer.spotlight_projection_matrix);
	output.normal_world_space = mul(input.normal, (float3x3)modelTransformMatrix);
	output.normal_view_space = mul(float4(input.normal, 1.0f), transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].inverse_transpose_model_view).xyz;
	output.normal_view_space = normalize(output.normal_view_space);
	
	// World space -> Voxel grid space:
	output.position_voxel_grid_space = float4(output.position_world_space - voxel_grid_data.centre_world_space, 1.0f);
	
	
	
	output.color = float4(input.color, 1.0f);
	output.tex_coord = input.tex_coord;
	
	return output;
}