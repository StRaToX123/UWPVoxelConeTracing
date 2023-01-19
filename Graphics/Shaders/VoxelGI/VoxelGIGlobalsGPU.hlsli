#ifndef _VOXEL_GI_GLOBALS_
#define _VOXEL_GI_GLOBALS_

#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"

struct VoxelType
{
	uint color;
	uint normal;
};

struct ShaderStructureGPUVoxelGridData
{
	uint res;
	float res_rcp;
	float grid_extent;
	float grid_extent_rcp;
	float grid_half_extent_rcp;
	float voxel_extent;
	float voxel_extent_rcp;
	float voxel_half_extent;
	float voxel_half_extent_rcp;
	float3 padding01;
	float3 top_left_point_world_space;
	float padding02;
	float3 centre_world_space;
	float padding03;
	int num_cones;
	float num_cones_rcp;
	float ray_step_size;
	float max_distance;
	float cone_aperture;
	int secondary_bounce_enabled;
	uint mip_count;
	float trace_mip_level_bias;
	float indirect_diffuse_multiplier;
	float diffuse_ambient_intensity;
	float2 padding04;
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
	uint3 index;
	uint padding1;
	float4 color;
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

// voxels:			3D Texture containing voxel scene with direct diffuse lighting (or direct + secondary indirect bounce)
// P:				world-space position of receiving surface
// N:				world-space normal vector of receiving surface
// coneDirection:	world-space cone direction in the direction to perform the trace
// coneAperture:	tan(coneHalfAngle)
#define VOXEL_INITIAL_OFFSET 2
#define SQRT2 1.41421356237309504880
#define SQRT3 1.73205080757
inline float4 ConeTrace(in Texture3D<float4> radianceTexture3D, ShaderStructureGPUVoxelGridData voxelGridData, in SamplerState samp, float3 P, in float3 N, in float3 coneDirection)
{
	float3 color = 0;
	float alpha = 0;
	
	// We need to offset the cone start position to avoid sampling its own voxel (self-occlusion):
	// Unfortunately, it will result in disconnection between nearby surfaces :(
	float distanceTraveled = 0.0f; 
	// Offset by cone dir so that first sample of all cones are not the same
	float3 startingPosition = P + N * voxelGridData.voxel_half_extent * SQRT2 * VOXEL_INITIAL_OFFSET; // sqrt2 is here because when multiplied by the voxel half extent equals the diagonal of the voxel
	//float3 startingPosition = P + N * voxelGridData.tracing_starting_position_offset;
	
	// We will break off the loop if the sampling distance is too far for performance reasons:
	while (distanceTraveled < voxelGridData.max_distance && alpha < 1.0f)
	{
		float diameter = max(voxelGridData.voxel_half_extent, 2 * voxelGridData.cone_aperture * distanceTraveled);
		float mip = (float) (voxelGridData.mip_count - 1) - lerp(0.0f, (float) (voxelGridData.mip_count - 1), 1.0f - pow(distanceTraveled / voxelGridData.max_distance, voxelGridData.trace_mip_level_bias));
		// Because we do the ray-marching in world space, we need to remap into 3d texture space before sampling:
		float3 voxelGridCoords = startingPosition + coneDirection * distanceTraveled;
		voxelGridCoords = (voxelGridCoords - voxelGridData.top_left_point_world_space) * voxelGridData.grid_extent_rcp;
		voxelGridCoords.y *= -1;

		// break if the ray exits the voxel grid, or we sample from the last mip:
		if (!IsSaturated(voxelGridCoords) || mip >= (float)(voxelGridData.mip_count - 1))
			break;

		float4 sampledRadiance = radianceTexture3D.SampleLevel(samp, voxelGridCoords, mip);

		// This is the correct blending to avoid black-staircase artifact (ray stepped front-to back, so blend front to back):
		// In other words the amount of color accumulated over the tracing is reduced with each new sampling
		// Also the amount the alpha falls off over time is reduced, creating a smooth curve towards the alpha reaching zero
		float a = 1 - alpha;
		color += a * sampledRadiance.rgb;
		alpha += a * sampledRadiance.a;

		// Step along the ray:
		distanceTraveled += diameter * voxelGridData.ray_step_size;
	}

	return float4(color, alpha);
}

// A uniform 2D random generator for hemisphere sampling: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
//	idx	: iteration index
//	num	: number of iterations in total
inline float2 Hammersley2D(uint idx, uint num)
{
	uint bits = idx;
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	const float radicalInverse_VdC = float(bits) * 2.3283064365386963e-10; // / 0x100000000

	return float2(float(idx) / float(num), radicalInverse_VdC);
}


// Point on hemisphere with cosine-weighted distribution
//	u, v : in range [0, 1]
float3 HemispherePointUVtoTanglentSpaceCosWeightedDistribution(float u, float v)
{
	float phi = v * 2 * PI;
	float cosTheta = sqrt(1 - u);
	float sinTheta = sqrt(1 - cosTheta * cosTheta);
	return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}



// voxels:			3D Texture containing voxel scene with direct diffuse lighting (or direct + secondary indirect bounce)
// P:				world-space position of receiving surface
// N:				world-space normal vector of receiving surface
inline float4 ConeTraceDiffuse(in Texture3D<float4> radianceTexture3D, in ShaderStructureGPUVoxelGridData voxelGridData, in SamplerState samp, in float3 P, in float3 N)
{
	float4 amount = 0;
	float3x3 tangentSpaceToWorldSpaceMatrix = GetTangentSpaceToSuppliedNormalVectorSpace(N);

	for (uint cone = 0; cone < voxelGridData.num_cones; ++cone) // quality is between 1 and 16 cones
	{
		float2 hamm = Hammersley2D(cone, voxelGridData.num_cones);
		float3 hemisphere = HemispherePointUVtoTanglentSpaceCosWeightedDistribution(hamm.x, hamm.y);
		float3 coneDirection = mul(hemisphere, tangentSpaceToWorldSpaceMatrix);

		amount += ConeTrace(radianceTexture3D, voxelGridData, samp, P, N, coneDirection);
	}

	// final radiance is average of all the cones radiances
	amount *= voxelGridData.num_cones_rcp;
	amount.rgb = max(0, amount.rgb);
	amount.a = saturate(amount.a);

	return amount;
}


#endif