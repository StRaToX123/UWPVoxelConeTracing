#include "ShaderStructures_HF.hlsli"
#include "ShaderGlobalsCPUGPU.hlsli"

SamplerState bilinear_sampler : register(s0);
ConstantBuffer<ShaderStructureGPUVoxelizationData> voxelization_data : register(b0);
Texture3D<float4> texture_3D_vct_voxelization : register(t0);
AppendStructuredBuffer<ShaderStructureGPUVoxelDebugData> append_buffer_indirect_draw_voxel_debug_per_instance_data : register(u0);
RWStructuredBuffer<IndirectCommandGPU> indirect_command_buffer_voxel_debug : register(u1);


[numthreads(DISPATCH_BLOCK_SIZE_VCT_DEBUG_GENERATE_INDIRECT_DRAW_DATA, DISPATCH_BLOCK_SIZE_VCT_DEBUG_GENERATE_INDIRECT_DRAW_DATA, DISPATCH_BLOCK_SIZE_VCT_DEBUG_GENERATE_INDIRECT_DRAW_DATA)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	if (dispatchThreadID.x >= voxelization_data.voxel_grid_res || dispatchThreadID.y >= voxelization_data.voxel_grid_res || dispatchThreadID.z >= voxelization_data.voxel_grid_res)
	{
		return;
	}
	
	float4 color = texture_3D_vct_voxelization[dispatchThreadID];
	[branch]
	if (color.a == 1.0f)
	{
		ShaderStructureGPUVoxelDebugData debugVoxel;
		debugVoxel.position_world_space = voxelization_data.voxel_grid_top_left_back_point_world_space;
		debugVoxel.position_world_space += (((float3) dispatchThreadID * voxelization_data.voxel_extent) * float3(1.0f, -1.0f, 1.0f));
		debugVoxel.position_world_space += float3(voxelization_data.voxel_half_extent, -voxelization_data.voxel_half_extent, voxelization_data.voxel_half_extent);
		debugVoxel.color = color;
		debugVoxel.padding1 = 0;
		append_buffer_indirect_draw_voxel_debug_per_instance_data.Append(debugVoxel);
		InterlockedAdd(indirect_command_buffer_voxel_debug[0].instance_count, 1);
	}
}