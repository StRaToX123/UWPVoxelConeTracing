#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\GBuffer_HF.hlsli"



cbuffer GPrepassCB : register(b0)
{
	float4x4 ViewProjection;
};

cbuffer perModelInstanceCB : register(b1)
{
	float4x4 World;
	float4 DiffuseColor;
};


PixelShaderInputGBuffer main(VertexShaderInputGBuffer input)
{
	PixelShaderInputGBuffer result;

	result.normal = mul(input.normal, (float3x3) World);
	result.tangent = mul(input.tangent, (float3x3) World);
	result.position = mul(float4(input.position.xyz, 1), World);
	result.worldPos = result.position.xyz;
	result.position = mul(result.position, ViewProjection);
	result.uv = input.uv;

	return result;
}