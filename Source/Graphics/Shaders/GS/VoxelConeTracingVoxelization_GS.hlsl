#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracingVoxelization_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShaderStructures_HF.hlsli"

ConstantBuffer<ShaderStructureGPUVoxelizationData> voxelization_data : register(b0);

[maxvertexcount(3)]
void main(triangle GeometryShaderInputVoxelConeTracingVoxelization input[3], inout TriangleStream<PixelShaderInputVoxelConeTracingVoxelization> OutputStream)
{
	float3 faceNormal = abs(input[0].normal_world_space + input[1].normal_world_space + input[2].normal_world_space);
	uint maxFaceNormalIndex = faceNormal[1] > faceNormal[0] ? 1 : 0;
	maxFaceNormalIndex = faceNormal[2] > faceNormal[maxFaceNormalIndex] ? 2 : maxFaceNormalIndex;

	[unroll]
	for (uint i = 0; i < 3; ++i)
	{
		PixelShaderInputVoxelConeTracingVoxelization output;
		output.position_voxel_grid_space = input[i].position_voxel_grid_space;
		
		// Project onto dominant axis:
		[flatten]
		if (maxFaceNormalIndex == 0)
		{
			output.position_voxel_grid_space.xyz = output.position_voxel_grid_space.zyx;
		}
		else if (maxFaceNormalIndex == 1)
		{
			output.position_voxel_grid_space.xyz = output.position_voxel_grid_space.xzy;
		}

		// Voxel grid space -> Clip space
		output.position_voxel_grid_space.xy *= voxelization_data.voxel_grid_half_extent_world_space_rcp;
		output.position_voxel_grid_space.zw = 1;
		// Append the rest of the data
		output.position_world_space = input[i].position_world_space.xyz;

		OutputStream.Append(output);
	}
}