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
	centroid float3 position_world_space : POSITION3D; // needs to be in world space so that we can convert it to a voxel grid index
	centroid float3 position_view_space : POSITION; // needs to be in view space so that lighting can be calculated
	centroid float4 color : COLOR;
	centroid float3 normal_view_space : NORMAL; // needs to be in view space so that lighting can be calculated
	
};

struct VoxelType
{
	uint color;
	uint normal;
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

struct ShaderStructureGPUSpotLight
{
    float4 position_world_space;
    float4 position_view_space;
    float4 direction_world_space;
    float4 direction_view_space;
    float4 color;
	float intensity;
	float spot_angle;
	float attenuation;
	float padding;
};

struct IndirectCommandGPU
{
	uint2 cbv;
	uint4 draw_command;
};

struct ShaderStructureGPUVoxelDebugData
{
	uint voxel_index;
	matrix model;
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

static const float __hdrRange = 10.0f;

// Encode HDR color to a 32 bit uint
// Alpha is 1 bit + 7 bit HDR remapping
uint PackVoxelColor(in float4 color)
{
	// normalize color to LDR
	float hdr = length(color.rgb);
	color.rgb /= hdr;

	// encode LDR color and HDR range
	uint3 iColor = uint3(color.rgb * 255.0f);
	uint iHDR = (uint) (saturate(hdr / __hdrRange) * 127);
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

	color.rgb *= hdr * __hdrRange;

	color.a = (colorMask >> 31u) & 0x00000001;

	return color;
}

#endif