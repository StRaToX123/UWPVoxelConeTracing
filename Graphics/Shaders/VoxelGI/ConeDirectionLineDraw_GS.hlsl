
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\ShaderGlobalsCPUGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\ConeDirectionLineDraw_HF.hlsli"

//RWStructuredBuffer<ShaderStructureGPUConeDebugDirectionData> cone_debug_direction_data_required_for_frame_draw : register(u4);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);

[maxvertexcount(2)]
void main(line VertexShaderOutputConeDirectionLineDraw input[2] : SV_POSITION, 
			inout LineStream<GeometryShaderOutputConeDirectionLineDraw> outputStream)
{
	for (uint i = 0; i < 2; ++i)
	{
		/*
		GeometryShaderOutputConeDirectionLineDraw output;
		output.position = float4(cone_debug_direction_data_required_for_frame_draw[input[i].instance_id].line_vertices_world_position[i], 1.0f);
		output.position = mul(output.position, view_projection_matrix_buffer.view);
		output.position = mul(output.position, view_projection_matrix_buffer.projection);
		
		output.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
		outputStream.Append(output);
		*/
		
		GeometryShaderOutputConeDirectionLineDraw output;
		output.position = float4(i * 2.0f, i * 2.0f, 0.0f, 1.0f);
		output.position = mul(output.position, view_projection_matrix_buffer.view);
		output.position = mul(output.position, view_projection_matrix_buffer.projection);
		output.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
		outputStream.Append(output);
	}
}