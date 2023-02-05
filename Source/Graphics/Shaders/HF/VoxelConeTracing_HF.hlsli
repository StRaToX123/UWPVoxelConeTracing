
#ifndef _VOXEL_CONE_TRACING_HF_
#define _VOXEL_CONE_TRACING_HF_

struct VertexShaderInputVoxelConeTracing
{
	float4 position : POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderInputVoxelConeTracing
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderOutputVoxelConeTracing
{
	float4 result : SV_Target0;
};



#endif

