#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"

RWStructuredBuffer<VoxelType> voxel_data_structured_buffer : register(u2);
//RWTexture3D<min16float4> radiance_texture_3D : register(u4);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
StructuredBuffer<ShaderStructureGPUVoxelDebugData> voxel_debug_data : register(t1);
AppendStructuredBuffer<ShaderStructureGPUVoxelDebugData> voxel_debug_data_required_for_frame_draw : register(u0);
RWStructuredBuffer<IndirectCommandGPU> voxel_debug_indirect_command : register(u1);

[numthreads(256, 1, 1)]
void main(uint dispatchThreadID : SV_DispatchThreadID)
{
	VoxelType voxelData = voxel_data_structured_buffer[dispatchThreadID.x];
	const min16float4 color = UnpackVoxelColor(voxelData.color);
	const uint3 writecoord = UnFlatten1DTo3DIndex(dispatchThreadID.x, voxel_grid_data.res);
	[branch]
	if (color.a > 0)
	{
		// Blend voxels with the previous frame's data to avoid popping artifacts for dynamic objects:
		// This operation requires Feature: Typed UAV additional format loads!
		//radiance_texture_3D[writecoord] = lerp(radiance_texture_3D[writecoord], min16float4(color.rgb, 1.0f), 0.2f);
		
		ShaderStructureGPUVoxelDebugData debugVoxel = voxel_debug_data[dispatchThreadID.x];
		debugVoxel.color_r = color.r;
		debugVoxel.color_g = color.g;
		debugVoxel.color_b = color.b;
		
		voxel_debug_data_required_for_frame_draw.Append(debugVoxel);
		InterlockedAdd(voxel_debug_indirect_command[0].instance_count, 1);
	}
	else
	{
		//radiance_texture_3D[writecoord] = 0.0f;
	}

	// Delete emission data, but keep normals (no need to delete, we will only read normal values of filled voxels)
	voxel_data_structured_buffer[dispatchThreadID.x].color = 0;
	voxel_data_structured_buffer[dispatchThreadID.x].normal = 0;

}
