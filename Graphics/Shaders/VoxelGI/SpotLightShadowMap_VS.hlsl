#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\SpotLightShadowMap_HF.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\SpotLight_HF.hlsli"


ConstantBuffer<ShaderStructureGPURootConstants> root_constants : register(b0);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);
ConstantBuffer<ShaderStructureGPUSpotLight> spot_light_buffer : register(b3);
ConstantBuffer<ShaderStructureGPUTransformBuffer> transform_matrix_buffers[] : register(b5);


VertexShaderOutputSpotLightShadowMap main(VertexShaderInputDefault input)
{
	VertexShaderOutputSpotLightShadowMap output;
	
	output.position = mul(float4(input.position, 1.0f), transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].model);
	// Determine the light position based on the position of the light and the position of the vertex in the world.
	output.light_pos_world_space = spot_light_buffer.position_world_space.xyz - output.position.xyz;
    // Normalize the light position vector.
	output.light_pos_world_space = normalize(output.light_pos_world_space);
	// Calculate the normal vector against the world matrix only.
	// But apply only the rotations, hence to cast to float3x3
	output.normal_world_space = mul(input.normal, (float3x3)transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].model);
	// Calculate the position of the vertice as viewed by the light source.
	output.position_light_view_proj_space = mul(output.position, spot_light_buffer.spotlight_view_matrix);
	output.position_light_view_proj_space = mul(output.position_light_view_proj_space, spot_light_buffer.spotlight_projection_matrix);
	// Calculate the position of the vertex as viewed by the camera
	output.position = mul(output.position, view_projection_matrix_buffer.view);
	output.position = mul(output.position, view_projection_matrix_buffer.projection);
	

	return output;
}