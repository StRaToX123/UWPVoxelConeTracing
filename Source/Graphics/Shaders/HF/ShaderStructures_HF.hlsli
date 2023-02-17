#define PI 3.14159265359f
#define SH_COSINE_LOBE_C0 0.886226925f // sqrt(pi)/2
#define SH_COSINE_LOBE_C1 1.02332671f // sqrt(pi/3)
#define SH_C0 0.282094792f // 1 / 2sqrt(pi)
#define SH_C1 0.488602512f // sqrt(3/pi) / 2

#define LPV_DIM 32
#define LPV_DIM_HALF 16
#define LPV_DIM_INVERSE 0.03125f
#define LPV_SCALE 0.25f




struct ShaderStructureGPUDirectionalLightData
{
	float4x4 view_projection;
	float4 inverse_direction_world_space; // Vector pointing towards the light
	float4 color;
	float intensity;
	float3 padding;
};

struct ShaderSTructureGPULightingData
{
	float2 shadow_texel_size;
	float2 padding;
};

struct ShaderStructureGPUCameraData
{
	float4x4 view_projection;
	float3 position_world_space;
	float padding;
};


struct ShaderStructureGPUModelData
{
	float4x4 model;
	float4 diffuse_color;
};

struct ShaderStructureGPUVoxelizationData
{
	float3 voxel_grid_top_left_back_point_world_space;
	uint voxel_grid_res;
	float voxel_grid_extent_world_space;
	float voxel_grid_half_extent_world_space_rcp;
	float voxel_extent_rcp;
	float voxel_scale;
};

struct ShaderStructureGPUMipMappingData
{
	int mip_dimension;
	int mip_level;
	float2 padding;
};

struct ShaderStructureGPUVCTMainData
{
	float2 upsample_ratio;
	float indirect_diffuse_strength;
	float indirect_specular_strength;
	float max_cone_trace_distance;
	float ao_falloff;
	float sampling_factor;
	float voxel_sample_offset;
};

struct ShaderStructureGPUIlluminationFlagsData
{
	int use_direct;
	int use_shadows;
	int use_vct;
	int use_vct_debug;
	float vct_gi_power;
	int show_only_ao;
	float2 padding;
};