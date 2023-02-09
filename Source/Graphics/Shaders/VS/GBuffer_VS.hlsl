#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\GBuffer_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShaderStructures_HF.hlsli"

ConstantBuffer<ShaderStructureGPUCameraData> camera_data : register(b0);
ConstantBuffer<ShaderStructureGPUModelData> model_data : register(b1);

PixelShaderInputGBuffer main(VertexShaderInputGBuffer input)
{
	PixelShaderInputGBuffer result;

	result.normal = mul(input.normal, (float3x3)model_data.model);
	result.tangent = mul(input.tangent, (float3x3)model_data.model);
	result.position = mul(float4(input.position.xyz, 1), model_data.model);
	result.worldPos = result.position.xyz;
	result.position = mul(result.position, camera_data.view_projection);
	result.uv = input.uv;

	return result;
}