#include "VoxelConeTracingVoxelization_HF.hlsli"
#include "ShaderStructures_HF.hlsli"

RWTexture3D<float4> outputTexture : register(u0);
Texture2D<float> shadowBuffer : register(t0);
SamplerComparisonState PcfShadowMapSampler : register(s0);
ConstantBuffer<ShaderStructureGPUVoxelizationData> voxelization_data : register(b0);
ConstantBuffer<ShaderStructureGPUDirectionalLightData> directional_light_data : register(b1);
ConstantBuffer<ShaderStructureGPUModelData> model_data : register(b2);



void main(PixelShaderInputVoxelConeTracingVoxelization input)
{
	int3 voxelIndex = (input.position_world_space - voxelization_data.voxel_grid_top_left_back_point_world_space) * voxelization_data.voxel_extent_rcp;
	voxelIndex.y *= -1;
	
	float4 color = float4(model_data.diffuse_color.rgb, 1.0f);
	float4 lightSpacePos = mul(float4(input.position_world_space, 1.0f), directional_light_data.view_projection);
	float4 shadowcoord = lightSpacePos / lightSpacePos.w;
	shadowcoord.rg = shadowcoord.rg * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
	float shadow = shadowBuffer.SampleCmpLevelZero(PcfShadowMapSampler, shadowcoord.xy, shadowcoord.z);

	outputTexture[voxelIndex] = color * float4(shadow, shadow, shadow, 1.0f);
}