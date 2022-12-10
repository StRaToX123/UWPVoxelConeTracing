struct VoxelDebugDrawVertexShaderInput
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float2 tex_coord : TEXCOORD;
	float3 color : COLOR;
	uint instance_id : SV_InstanceID;
};

struct VoxelDebugDrawPixelShaderInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};