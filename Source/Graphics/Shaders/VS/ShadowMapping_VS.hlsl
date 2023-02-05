#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShadowMapping_HF.hlsli"

cbuffer ShadowMappingCB : register(b0)
{
	float4x4 LightViewProj;
	float4 LightColor;
	float4 LightDir;
};

cbuffer perModelInstanceCB : register(b1)
{
	float4x4 World;
	float4 DiffuseColor;
};

float4 main(VertexShaderInputShadowMapping input) : SV_Position
{
	float4 result;
	result = mul(float4(input.position.xyz, 1.0f), World);
	result = mul(result, LightViewProj);
    //result.z *= result.w;

	return result;
}