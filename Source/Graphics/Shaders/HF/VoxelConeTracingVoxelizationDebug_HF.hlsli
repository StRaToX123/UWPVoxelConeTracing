#ifndef _VOXEL_CONE_TRACING_VOXELIZATION_DEBUG_HF_
#define _VOXEL_CONE_TRACING_VOXELIZATION_DEBUG_HF_

struct VertexShaderInputVoxelConeTracingVoxelizationDebug
{
	uint vertexID : SV_VertexID;
};

struct GeometryShaderInputVoxelConeTracingVoxelizationDebug
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

struct PixelShaderInputVoxelConeTracingVoxelizationDebug
{
	float4 pos : SV_Position;
	float4 color : COLOR;
};

struct PixelShaderOutputVoxelConeTracingVoxelizationDebug
{
	float4 result : SV_Target0;
};


#endif