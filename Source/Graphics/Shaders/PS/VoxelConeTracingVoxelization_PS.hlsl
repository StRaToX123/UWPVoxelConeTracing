#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracingVoxelization_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShaderStructures_HF.hlsli"

RWTexture3D<float4> outputTexture : register(u0);
Texture2D<float> shadowBuffer : register(t0);
SamplerComparisonState PcfShadowMapSampler : register(s0);
ConstantBuffer<ShaderStructureGPUVoxelizationData> voxelization_data : register(b0);
ConstantBuffer<ShaderStructureGPUDirectionalLightData> directional_light_data : register(b1);
ConstantBuffer<ShaderStructureGPUModelData> model_data : register(b2);


float3 VoxelToWorld(float3 pos)
{
	float3 result = pos;
	result *= voxelization_data.voxel_scale;

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
	float4 colorRes = float4(model_data.diffuse_color.rgb, 1.0f);
	voxelPos.y = -voxelPos.y;
    
	float4 worldPos = float4(VoxelToWorld(voxelPos), 1.0f);
	float4 lightSpacePos = mul(worldPos, directional_light_data.view_projection);
	float4 shadowcoord = lightSpacePos / lightSpacePos.w;
	shadowcoord.rg = shadowcoord.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
	float shadow = shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, shadowcoord.xy, shadowcoord.z);

	outputTexture[finalVoxelPos] = colorRes * float4(shadow, shadow, shadow, 1.0f);
}