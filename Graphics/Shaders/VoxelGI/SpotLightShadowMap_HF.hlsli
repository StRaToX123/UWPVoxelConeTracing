#ifndef _SPOT_LIGHT_SHADOW_MAP_
#define _SPOT_LIGHT_SHADOW_MAP_



struct VertexShaderOutputSpotLightShadowMap
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float4 lightViewPosition : TEXCOORD0;
	float3 lightPos : TEXCOORD1;
};

#endif