#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShadowMapping_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShaderStructures_HF.hlsli"

ConstantBuffer<ShaderStructureGPUDirectionalLightData> directional_light_data : register(b0);
ConstantBuffer<ShaderStructureGPUModelData> model_data : register(b1);

float4 main(VertexShaderInputShadowMapping input) : SV_Position
{
	float4 result;
	result = mul(float4(input.position.xyz, 1.0f), model_data.model);
	result = mul(result, directional_light_data.view_projection);

	return result;
}