#ifndef _SPOT_LIGHT_
#define _SPOT_LIGHT_


struct ShaderStructureGPUSpotLight
{
	float4 position_world_space;
	float4 position_view_space;
	float4 direction_world_space;
	float4 direction_view_space;
	float4 color;
	float intensity;
	float spot_angle_degrees;
	float spot_angle_radians;
	float spot_half_angle_degrees;
	float spot_half_angle_radians;
	float attenuation;
	float2 padding;
	matrix spotlight_view_matrix;
	matrix spotlight_projection_matrix;
};

inline float DoDiffuse(float3 N, float3 L)
{
	return max(0, dot(N, L));
}

inline float DoAttenuation(float attenuation, float distance)
{
	return 1.0f / (1.0f + attenuation * distance * distance);
}

inline float DoSpotCone(float3 spotDir, float3 L, float spotAngle)
{
	float minCos = cos(spotAngle);
	float maxCos = (minCos + 1.0f) / 2.0f;
	float cosAngle = dot(spotDir, -L);
	return smoothstep(minCos, maxCos, cosAngle);
}

inline float4 DoSpotLight(ShaderStructureGPUSpotLight spotLightData, float3 V, float3 P, float3 N)
{
	float4 result;
	float3 L = (spotLightData.position_view_space.xyz - P);
	float d = length(L);
	L = normalize(L);

	float attenuation = DoAttenuation(spotLightData.attenuation, d);
	float spotIntensity = smoothstep(cos(spotLightData.spot_half_angle_radians), 1.0f, dot(spotLightData.direction_view_space.xyz, -L));
	result = DoDiffuse(N, L) * attenuation * spotIntensity * spotLightData.color * spotLightData.intensity;
	return result;
}

#endif