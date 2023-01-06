#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"

RWStructuredBuffer<VoxelType> input_output : register(u2);
RWTexture3D<float4> radiance_texture_3D_UAV : register(u3);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
StructuredBuffer<ShaderStructureGPUVoxelDebugData> voxel_debug_data : register(t1);
AppendStructuredBuffer<ShaderStructureGPUVoxelDebugData> voxel_debug_data_required_for_frame_draw : register(u0);
RWStructuredBuffer<IndirectCommandGPU> voxel_debug_indirect_command : register(u1);

[numthreads(256, 1, 1)]
void main(uint dispatchThreadID : SV_DispatchThreadID)
{
	VoxelType voxel = input_output[dispatchThreadID.x];
	const float4 color = UnpackVoxelColor(voxel.color);
	const uint3 writecoord = UnFlatten1DTo3DIndex(dispatchThreadID.x, voxel_grid_data.res);
	[branch]
	if (color.a > 0)
	{
		// Blend voxels with the previous frame's data to avoid popping artifacts for dynamic objects:
		// This operation requires Feature: Typed UAV additional format loads!
		radiance_texture_3D_UAV[writecoord] = lerp(radiance_texture_3D_UAV[writecoord], float4(color.rgb, 1), 0.2f);
		ShaderStructureGPUVoxelDebugData debugVoxel = voxel_debug_data[dispatchThreadID.x];
		voxel_debug_data_required_for_frame_draw.Append(debugVoxel);
		InterlockedAdd(voxel_debug_indirect_command[0].instance_count, 1);
	}
	else
	{
		radiance_texture_3D_UAV[writecoord] = 0;
	}

	// Delete emission data, but keep normals (no need to delete, we will only read normal values of filled voxels)
	input_output[dispatchThreadID.x].color = 0;
	input_output[dispatchThreadID.x].normal = 0;
}
