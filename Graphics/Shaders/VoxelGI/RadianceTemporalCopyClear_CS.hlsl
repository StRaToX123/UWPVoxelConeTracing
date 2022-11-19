#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"

RWStructuredBuffer<VoxelType> input_output : register(u1);
RWTexture3D<float4> output_emission : register(u2);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
StructuredBuffer<IndirectCommandGPU> input_commands : register(t2);
AppendStructuredBuffer<IndirectCommandGPU> output_commands : register(u0);

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
		output_emission[writecoord] = lerp(output_emission[writecoord], float4(color.rgb, 1), 0.2f);
		// Append an indirect command that can draw a debug cube in the place of this voxel
		// These indirect commands are used by the voxel debug view 
		output_commands.Append(input_commands[dispatchThreadID.x]);
	}
	else
	{
		output_emission[writecoord] = 0;
	}

	// Delete emission data, but keep normals (no need to delete, we will only read normal values of filled voxels)
	input_output[dispatchThreadID.x].color = 0;
}
