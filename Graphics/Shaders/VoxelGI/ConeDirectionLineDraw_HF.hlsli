#ifndef _CONE_DIRECTION_LINE_DRAW_HF_
#define _CONE_DIRECTION_LINE_DRAW_HF_

struct VertexShaderInputConeDirectionLineDraw
{
	float4 position : POSITION;
	uint instance_id : SV_InstanceID;
};


struct VertexShaderOutputConeDirectionLineDraw
{
	float4 position : SV_POSITION;
	uint instance_id : SV_InstanceID;
};

struct GeometryShaderOutputConeDirectionLineDraw
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

struct ShaderStructureGPUConeDirectionDebugData
{
	float3 line_vertices_world_position[2];
	float2 padding1;
	float4 color;
};

#endif