



struct VertexShaderOutputFinalGather
{
	float4 pos : SV_POSITION;
	float3 position_world_space : POSITION1;
	float3 normal_world_space : NORMAL;
	float4 color : COLOR;
};

struct VertexShaderOutputFinalGatherCopy
{
	float4 pos : SV_POSITION;
	float3 position_world_space : POSITION1;
	float3 normal_world_space : NORMAL;
	float4 color : COLOR;
	uint2 screen_space_coords : TEXCOORD1;
};