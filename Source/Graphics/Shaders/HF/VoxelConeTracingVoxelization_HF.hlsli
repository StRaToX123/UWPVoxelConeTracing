#ifndef _VOXEL_CONE_TRACING_VOXELIZATION_HF_
#define _VOXEL_CONE_TRACING_VOXELIZATION_HF_


struct VertexShaderInputVoxelConeTracingVoxelization
{
	float3 position : POSITION;
	float3 normal : NORMAL;
};

struct GeometryShaderInputVoxelConeTracingVoxelization
{
	float4 position : SV_POSITION;
	float3 normal : TEXCOORD0;
};

struct PixelShaderInputVoxelConeTracingVoxelization
{
	float4 position : SV_POSITION;
	float3 voxelPos : VOXEL_POSITION;
};


#endif