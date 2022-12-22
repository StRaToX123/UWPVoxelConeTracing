struct VertexShaderOutputVoxelizer
{
	float4 position_voxel_grid_space : SV_POSITION;
	centroid float3 position_world_space : POSITION1; // needs to be in world space so that we can convert it to a voxel grid index
	centroid float3 position_view_space : POSITION2; // needs to be in view space so that lighting can be calculated
	float3 normal_world_space : NORMAL;
	centroid float3 normal_view_space : NORMAL1; // needs to be in view space so that lighting can be calculated
	float4 color : COLOR;
	float2 tex_coord : TEXCOORD;
	centroid float4 spot_light_shadow_map_tex_coord : TEXCOORD1;
};

// Centroid interpolation is used to avoid floating voxels in some cases
struct GeometryShaderOutputVoxelizer
{
	float4 position_voxel_grid_space : SV_POSITION;
	centroid float3 position_world_space : POSITION1; // needs to be in world space so that we can convert it to a voxel grid index
	centroid float3 position_view_space : POSITION2; // needs to be in view space so that lighting can be calculated
	centroid float3 normal_view_space : NORMAL1; // needs to be in view space so that lighting can be calculated
	centroid float4 color : COLOR;
	float2 tex_coord : TEXCOORD;
	centroid float4 spot_light_shadow_map_tex_coord : TEXCOORD1;
};

