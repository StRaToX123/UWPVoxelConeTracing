#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracingDebugIndirectDraw_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShaderStructures_HF.hlsli"

ConstantBuffer<ShaderStructureGPUVoxelizationData> voxelization_data : register(b0);
ConstantBuffer<ShaderStructureGPUCameraData> camera_data : register(b1);
StructuredBuffer<ShaderStructureGPUVoxelDebugData> append_buffer_indirect_draw_voxel_debug_per_instance_data : register(t0);

PixelShaderInputVoxelConeTracingDebugIndirectDraw main(VertexShaderInputVoxelConeTracingDebugIndirectDraw input)
{
	PixelShaderInputVoxelConeTracingDebugIndirectDraw output;
	// Scale the voxel debug cube which by default has a half extent of 1.0f
	output.position = float4(input.position * voxelization_data.voxel_half_extent, 1.0f);
	// Move the voxel debug cube to the apropriate position in the greed based on it's instance id
	output.position.xyz += append_buffer_indirect_draw_voxel_debug_per_instance_data[input.instance_id].position_world_space;
	output.position = mul(output.position, camera_data.view_projection);

	output.color = append_buffer_indirect_draw_voxel_debug_per_instance_data[input.instance_id].color;
	
	return output;
}