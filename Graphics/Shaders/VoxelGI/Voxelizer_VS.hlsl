#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"

ConstantBuffer<ShaderStructureGPURootConstants> root_constants : register(b0);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);
//ConstantBuffer<ShaderStructureGPUTransformBuffer> transform_matrix_buffers[] : register(b3);
ConstantBuffer<ShaderStructureGPUModelAndInverseTransposeModelView> transform_matrix_buffers : register(b3);

VoxelizerVertexShaderOutput main(VoxelizerVertexShaderInput input)
{
	VoxelizerVertexShaderOutput output;
	
	//output.position_world_space = mul(float4(input.position, 1.0f), transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].model).xyz;
	output.position_world_space = mul(float4(input.position, 1.0f), transform_matrix_buffers.model).xyz;
	output.position_view_space = mul(float4(output.position_world_space, 1.0f), view_projection_matrix_buffer.view).xyz;
	//output.normal_view_space = mul(float4(input.normal, 1.0f), transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].inverse_transpose_model_view).xyz;
	output.normal_view_space = mul(float4(input.normal, 1.0f), transform_matrix_buffers.inverse_transpose_model_view);
	// World space -> Voxel grid space:
	float3 voxelGridCenterWorldSpace = float3(voxel_grid_data.center_world_space_x, voxel_grid_data.center_world_space_y, voxel_grid_data.center_world_space_z);
	output.position_voxel_grid_space = float4(output.position_world_space - voxelGridCenterWorldSpace, 1.0f);
	
	
	output.normal = input.normal;
	output.color = float4(input.color, 1.0f);
	
	return output;
}