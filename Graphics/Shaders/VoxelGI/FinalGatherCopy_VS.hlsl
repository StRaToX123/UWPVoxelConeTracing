#include "C:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\FinalGather_HF.hlsli"

ConstantBuffer<ShaderStructureGPURootConstants> root_constants : register(b0);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);
ConstantBuffer<ShaderStructureGPUTransformBuffer> transform_matrix_buffers[] : register(b5);

VertexShaderOutputFinalGatherCopy main(VertexShaderInputDefault input)
{
	VertexShaderOutputFinalGatherCopy output;
	matrix modelTransformMatrix = transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].model;
	output.position_world_space = mul(float4(input.position, 1.0f), modelTransformMatrix).xyz;
	output.pos = mul(float4(output.position_world_space, 1.0f), view_projection_matrix_buffer.view);
	output.position_view_space = output.pos.xyz;
	output.pos = mul(output.pos, view_projection_matrix_buffer.projection);
	output.spot_light_shadow_map_tex_coord = output.pos;
	output.normal_world_space = normalize(mul(input.normal, (float3x3) modelTransformMatrix));
	output.normal_view_space = mul(float4(input.normal, 1.0f), transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].inverse_transpose_model_view).xyz;
	output.normal_view_space = normalize(output.normal_view_space);
	output.color = float4(input.color, 1.0f);
	
	
	return output;
}