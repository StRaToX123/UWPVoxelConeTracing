#include "VoxelConeTracingVoxelization_HF.hlsli"
#include "ShaderStructures_HF.hlsli"

ConstantBuffer<ShaderStructureGPUModelData> model_data : register(b2);

GeometryShaderInputVoxelConeTracingVoxelization main(VertexShaderInputVoxelConeTracingVoxelization input)
{
	GeometryShaderInputVoxelConeTracingVoxelization output = (GeometryShaderInputVoxelConeTracingVoxelization) 0;
    
	output.position_voxel_grid_space = mul(float4(input.position.xyz, 1), model_data.model);
	// Techincally if we want to support the voxel grid being able to move around and not always be in world space (0, 0, 0)
	// We would need to apply those transformations here, but for now the grid is stuck at (0, 0, 0)
	output.position_world_space = output.position_voxel_grid_space;
	output.normal_world_space = mul(input.normal, (float3x3)model_data.model);
	return output;
}