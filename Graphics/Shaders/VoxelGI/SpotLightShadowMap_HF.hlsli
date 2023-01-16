#ifndef _SPOT_LIGHT_SHADOW_MAP_
#define _SPOT_LIGHT_SHADOW_MAP_



struct VertexShaderOutputSpotLightShadowMap
{
	float4 position : SV_Position;
	float3 normal_world_space : NORMAL;
	float4 position_light_view_proj_space : TEXCOORD0;
	float3 light_pos_world_space : TEXCOORD1;
};

#endif