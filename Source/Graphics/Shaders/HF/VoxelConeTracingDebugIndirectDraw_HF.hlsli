
#ifndef _VOXEL_CONE_TRACING_DEBUG_INDIRECT_DRAW_HF_
#define _VOXEL_CONE_TRACING_DEBUG_INDIRECT_DRAW_HF_


struct VertexShaderInputVoxelConeTracingDebugIndirectDraw
{
	float3 position : POSITION;
	uint instance_id : SV_InstanceID;
};

struct PixelShaderInputVoxelConeTracingDebugIndirectDraw
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

#endif