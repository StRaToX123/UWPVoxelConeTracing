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
	// Calculate the normal vector against the world matrix only.
	// But apply only the rotations, hence to cast to float3x3
	output.normal = mul(input.normal, (float3x3)transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].model);
	output.lightViewPosition = output.position;
	output.position = mul(output.position, view_projection_matrix_buffer.view);
	output.position = mul(output.position, view_projection_matrix_buffer.projection);
	// Calculate the position of the vertice as viewed by the light source.
	output.lightViewPosition = mul(output.lightViewPosition, spot_light_buffer.spotlight_view_matrix);
	output.lightViewPosition = mul(output.lightViewPosition, spot_light_buffer.spotlight_projection_matrix);
    // Determine the light position based on the position of the light and the position of the vertex in the world.
	output.lightPos = spot_light_buffer.position_world_space.xyz - input.position;
    // Normalize the light position vector.
	output.lightPos = normalize(output.lightPos);

	return output;
}