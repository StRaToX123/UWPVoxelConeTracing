
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracingVoxelization_HF.hlsli"

RWTexture3D<float4> outputTexture : register(u0);
Texture2D<float> shadowBuffer : register(t0);
SamplerComparisonState PcfShadowMapSampler : register(s0);
cbuffer perModelInstanceCB : register(b1)
{
	float4x4 World;
	float4 DiffuseColor;
};

cbuffer VoxelizationCB : register(b0)
{
	float4x4 WorldVoxelCube;
	float4x4 ViewProjection;
	float4x4 ShadowViewProjection;
	float WorldVoxelScale;
};

float3 VoxelToWorld(float3 pos)
{
	float3 result = pos;
	result *= WorldVoxelScale;

	return result * 0.5f;
}

void main(PixelShaderInputVoxelConeTracingVoxelization input)
{
	uint width;
	uint height;
	uint depth;
    
	outputTexture.GetDimensions(width, height, depth);
	float3 voxelPos = input.voxelPos.rgb;
	voxelPos.y = -voxelPos.y;
    
	int3 finalVoxelPos = width * float3(0.5f * voxelPos + float3(0.5f, 0.5f, 0.5f));
	float4 colorRes = float4(DiffuseColor.rgb, 1.0f);
	voxelPos.y = -voxelPos.y;
    
	float4 worldPos = float4(VoxelToWorld(voxelPos), 1.0f);
	float4 lightSpacePos = mul(worldPos, ShadowViewProjection);
	float4 shadowcoord = lightSpacePos / lightSpacePos.w;
	shadowcoord.rg = shadowcoord.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
	float shadow = shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, shadowcoord.xy, shadowcoord.z);

	outputTexture[finalVoxelPos] = colorRes * float4(shadow, shadow, shadow, 1.0f);
}