#ifndef _VOXEL_GI_GLOBALS_
#define _VOXEL_GI_GLOBALS_

struct GeometryShaderInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 color : COLOR;
};

// Centroid interpolation is used to avoid floating voxels in some cases
struct GeometryShaderOutput
{
	float4 position : SV_POSITION;
	centroid float4 color : COLOR;
	centroid float3 normal : NORMAL;
	centroid float3 position_world_space : POSITION3D;
};

struct ShaderStructureGPUVoxelGridData
{
	uint res;
	float res_rcp;
	float voxel_size;
	float voxel_size_rcp;
	float3 center;
	float3 extents;
	uint num_cones;
	float ray_step_size;
	float max_distance;
	bool secondary_bounce_enabled;
	bool reflections_enabled;
	bool center_changed_this_frame;
	uint mips = 7;
};

inline bool IsSaturated(float value)
{
	return value == saturate(value);
}
inline bool IsSaturated(float2 value)
{
	return IsSaturated(value.x) && IsSaturated(value.y);
}
inline bool IsSaturated(float3 value)
{
	return IsSaturated(value.x) && IsSaturated(value.y) && IsSaturated(value.z);
}
inline bool IsSaturated(float4 value)
{
	return IsSaturated(value.x) && IsSaturated(value.y) && IsSaturated(value.z) && IsSaturated(value.w);
}

#endif