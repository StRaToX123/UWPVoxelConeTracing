#ifndef _GBUFFER_HF_
#define _GBUFFER_HF_

struct VertexShaderInputGBuffer
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD;
};

struct PixelShaderInputGBuffer
{
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD0;
	float3 worldPos : TEXCOORD1;
	float4 position : SV_POSITION;
};


struct PixelShaderOutputGBuffer
{
	float4 color : SV_Target0;
	float4 normal : SV_Target1;
	float4 worldpos : SV_Target2;
};

#endif