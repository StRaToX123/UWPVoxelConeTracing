#ifndef _VOXEL_CONE_TRACING_VOXELIZATION_HF_
#define _VOXEL_CONE_TRACING_VOXELIZATION_HF_


struct VertexShaderInputVoxelConeTracingVoxelization
{
	float3 position : POSITION;
	float3 normal : NORMAL;
};

struct GeometryShaderInputVoxelConeTracingVoxelization
{
	float4 position_voxel_grid_space : SV_POSITION;
	float4 position_world_space : POSITION1;
	float3 normal_world_space : NORMAL;
};

struct PixelShaderInputVoxelConeTracingVoxelization
{
	float4 position_voxel_grid_space : SV_POSITION;
	float3 position_world_space : POSITION1;
};


#endif