#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"


ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);

[maxvertexcount(3)]
void main(triangle VoxelizerVertexShaderOutput input[3], inout TriangleStream<VoxelizerGeometryShaderOutput> outputStream)
{
	float3 faceNormal = abs(input[0].normal + input[1].normal + input[2].normal);
	uint maxFaceNormalIndex = faceNormal[1] > faceNormal[0] ? 1 : 0;
	maxFaceNormalIndex = faceNormal[2] > faceNormal[maxFaceNormalIndex] ? 2 : maxFaceNormalIndex;

	for (uint i = 0; i < 3; ++i)
	{
		VoxelizerGeometryShaderOutput output;
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
		output.position_voxel_grid_space.xy *= voxel_grid_data.grid_half_extent_rcp;
		output.position_voxel_grid_space.zw = 1; // Check if setting the depth to something that isn't a max 1 value will improve rasterization
		// Append the rest of the data
		output.color = input[i].color;
		output.normal_view_space = input[i].normal_view_space;
		output.position_view_space = input[i].position_view_space;
		output.position_world_space = input[i].position_world_space;

		outputStream.Append(output);
	}
}
