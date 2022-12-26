#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\SpotLight_HF.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\SpotLightWriteOnlyDepth_HF.hlsli"



ConstantBuffer<ShaderStructureGPURootConstants> root_constants : register(b0);
ConstantBuffer<ShaderStructureGPUSpotLight> spot_light_buffer : register(b3);
ConstantBuffer<ShaderStructureGPUTransformBuffer> transform_matrix_buffers[] : register(b5);

VertexShaderOutputWriteOnlyDepth main(VertexShaderInputDefault input)
{
	VertexShaderOutputWriteOnlyDepth output;
	output.position = mul(float4(input.position, 1.0f), transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].model);
	output.position = mul(output.position, spot_light_buffer.spotlight_view_matrix);
	output.position = mul(output.position, spot_light_buffer.spotlight_projection_matrix);
	
	return output;
}