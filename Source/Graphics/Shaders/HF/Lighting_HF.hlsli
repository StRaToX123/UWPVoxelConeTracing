#ifndef _LIGHTING_HF_
#define _LIGHTING_HF_

struct VertexShaderInputLighting
{
	float4 position : POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderInputLighting
{
	float2 uv : TEXCOORD;
	float4 position : SV_POSITION;
};

struct PixelShaderOutputLighting
{
	float4 diffuse : SV_Target0;
};



#endif