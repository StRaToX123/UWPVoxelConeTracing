#ifndef _VOXEL_GI_GLOBALS_
#define _VOXEL_GI_GLOBALS_



struct VoxelType
{
	uint color;
	uint normal;
};

struct ShaderStructureGPUVoxelGridData
{
	uint res;
	float grid_extent;
	float grid_half_extent_rcp;
	float voxel_half_extent;
	float voxel_extent_rcp;
	float bottom_left_point_world_space_x;
	float bottom_left_point_world_space_y;
	float bottom_left_point_world_space_z;
	float center_world_space_x;
	float center_world_space_y;
	float center_world_space_z;
	int num_cones;
	float ray_step_size;
	float max_distance;
	int secondary_bounce_enabled;
	uint reflections_enabled;
	uint center_changed_this_frame;
	uint mips;
};

struct ShaderStructureGPUSpotLight
{
    float4 position_world_space;
    float4 position_view_space;
    float4 direction_world_space;
	float4 direction_view_space;
    float4 color;
	float intensity;
	float spot_angle_degrees;
	float attenuation;
	matrix spotlight_view_matrix;
	matrix spotlight_projection_matrix;
	float padding;
};

struct IndirectCommandGPU
{
	uint index_count_per_instance;
	uint instance_count;
	uint start_index_location;
	int base_vertex_location;
	uint start_instance_location;
};

struct ShaderStructureGPUVoxelDebugData
{
	uint index_x;
	uint index_y;
	uint index_z;
	float color_r;
	float color_g;
	float color_b;
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

static const float sc_hdr_range = 10.0f;

// Encode HDR color to a 32 bit uint
// Alpha is 1 bit + 7 bit HDR remapping
uint PackVoxelColor(in float4 color)
{
	// normalize color to LDR
	float hdr = length(color.rgb);
	color.rgb /= hdr;

	// encode LDR color and HDR range
	uint3 iColor = uint3(color.rgb * 255.0f);
	uint iHDR = (uint) (saturate(hdr / sc_hdr_range) * 127);
	uint colorMask = (iHDR << 24u) | (iColor.r << 16u) | (iColor.g << 8u) | iColor.b;

	// encode alpha into highest bit
	uint iAlpha = (color.a > 0 ? 1u : 0u);
	colorMask |= iAlpha << 31u;

	return colorMask;
}

// Decode 32 bit uint into HDR color with 1 bit alpha
float4 UnpackVoxelColor(in uint colorMask)
{
	float hdr;
	float4 color;

	hdr = (colorMask >> 24u) & 0x0000007f;
	color.r = (colorMask >> 16u) & 0x000000ff;
	color.g = (colorMask >> 8u) & 0x000000ff;
	color.b = colorMask & 0x000000ff;

	hdr /= 127.0f;
	color.rgb /= 255.0f;

	color.rgb *= hdr * sc_hdr_range;

	color.a = (colorMask >> 31u) & 0x00000001;

	return color;
}

#endif