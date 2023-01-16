



struct VertexShaderOutputFinalGather
{
	float4 pos : SV_POSITION;
	float3 position_world_space : POSITION1;
	float3 position_view_space : POSITION2; // needs to be in view space so that lighting can be calculated
	float3 normal_world_space : NORMAL;
	float3 normal_view_space : NORMAL1; // needs to be in view space so that lighting can be calculated
	float4 color : COLOR;
	float4 spot_light_shadow_map_tex_coord : TEXCOORD1;
};

struct VertexShaderOutputFinalGatherCopy
{
	float4 pos : SV_POSITION;
	float3 position_world_space : POSITION1;
	float3 position_view_space : POSITION2; // needs to be in view space so that lighting can be calculated
	float3 normal_world_space : NORMAL;
	float3 normal_view_space : NORMAL1; // needs to be in view space so that lighting can be calculated
	float4 color : COLOR;
	float4 spot_light_shadow_map_tex_coord : TEXCOORD1;
	uint2 screen_space_coords : TEXCOORD2;
};