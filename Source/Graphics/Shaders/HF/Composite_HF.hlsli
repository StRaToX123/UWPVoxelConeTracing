#ifndef _COMPOSITE_HF_
#define _COMPOSITE_HF_

struct VertexShaderInputComposite
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderInputComposite
{
	float2 uv : TEXCOORD;
	float4 position : SV_POSITION;
};

struct PixelShaderOutputComposite
{
	float4 colour : SV_Target0;
};




#endif