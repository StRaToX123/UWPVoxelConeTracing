#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"


RWStructuredBuffer<VoxelType> output : register(u0);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b2);
ConstantBuffer<ShaderStructureGPUSpotLight> spot_light_data : register(b3);


float DoDiffuse(float3 N, float3 L)
{
	return max(0, dot(N, L));
}

float DoAttenuation(float attenuation, float distance)
{
	return 1.0f / (1.0f + attenuation * distance * distance);
}

float DoSpotCone(float3 spotDir, float3 L, float spotAngle)
{
	float minCos = cos(spotAngle);
	float maxCos = (minCos + 1.0f) / 2.0f;
	float cosAngle = dot(spotDir, -L);
	return smoothstep(minCos, maxCos, cosAngle);
}

float4 DoSpotLight(float3 V, float3 P, float3 N)
{
	float4 result;
	float3 L = (spot_light_data.position_view_space.xyz - P);
	float d = length(L);
	L = L / d;

	float attenuation = DoAttenuation(spot_light_data.attenuation, d);

	float spotIntensity = DoSpotCone(spot_light_data.direction_view_space.xyz, L, spot_light_data.spot_angle);

	result = DoDiffuse(N, L) * attenuation * spotIntensity * spot_light_data.color * spot_light_data.intensity;
	return result;
}

void main(GeometryShaderOutput input)
{
	float3 diff = (input.position_world_space - voxel_grid_data.center) * voxel_grid_data.res_rcp * voxel_grid_data.voxel_size_rcp;
	float3 uvw = diff * float3(0.5f, -0.5f, 0.5f) + 0.5f;

	[branch]
	if (IsSaturated(uvw))
	{
		float4 color = input.color;
		color.rgb *= DoSpotLight(normalize(-input.position_view_space), input.position_view_space, normalize(input.normal_view_space)) / PI;

		uint colorEncoded = PackVoxelColor(color);
		uint normalEncoded = PackUnitvector(input.normal_view_space);

		// output:
		uint3 writecoord = floor(uvw * voxel_grid_data.res);
		uint id = Flatten3DIndex(writecoord, voxel_grid_data.res);
		InterlockedMax(output[id].color, colorEncoded);
		InterlockedMax(output[id].normal, colorEncoded);
	}
}
