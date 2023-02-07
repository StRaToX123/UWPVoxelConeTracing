#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\Common_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShadowMapping_HF.hlsli"

ConstantBuffer<ShaderStructureGPUDirectionalLight> directional_light_data : register(b0);
cbuffer perModelInstanceCB : register(b1)
{
	float4x4 World;
	float4 DiffuseColor;
};

float4 main(VertexShaderInputShadowMapping input) : SV_Position
{
	float4 result;
	result = mul(float4(input.position.xyz, 1.0f), World);
	result = mul(result, directional_light_data.view_projection);

	return result;
}