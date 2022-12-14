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
	float attenuation;
	float paddig;
	matrix spotlight_view_matrix;
	matrix spotlight_projection_matrix;
};

#endif