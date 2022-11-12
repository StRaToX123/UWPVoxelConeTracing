#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"


ConstantBuffer<ShaderStructureGPURootConstants> root_constants : register(b0);
ConstantBuffer<ShaderStructureGPUModelTransformMarixBuffer> model_ransform_matrix_buffers[] : register(b1);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b2);

[maxvertexcount(3)]
void main(triangle GeometryShaderInput input[3], inout TriangleStream<GeometryShaderOutput> outputStream)
{
	float3 faceNormal = abs(input[0].normal + input[1].normal + input[2].normal);
	uint maxFaceNormalIndex = faceNormal[1] > faceNormal[0] ? 1 : 0;
	maxFaceNormalIndex = faceNormal[2] > faceNormal[maxFaceNormalIndex] ? 2 : maxFaceNormalIndex;

	for (uint i = 0; i < 3; ++i)
	{
		GeometryShaderOutput output;
		output.position = float4(input[i].position, 1.0f);
		output.position = mul(output.position, model_ransform_matrix_buffers[root_constants.model_transform_matrix_buffer_index].model[root_constants.model_transform_matrix_buffer_inner_index]);
		output.position_world_space = output.position.xyz;
		// World space -> Voxel grid space:
		output.position = (output.position.xyz - voxel_grid_data.center) * voxel_grid_data.voxel_size_rcp;
		// Project onto dominant axis:
		[flatten]
		if (maxFaceNormalIndex == 0)
		{
			output.position.xyz = output.position.zyx;
		}
		else if (maxFaceNormalIndex == 1)
		{
			output.position.xyz = output.position.xzy;
		}

		// Voxel grid space -> Clip space
		output.position.xy *= voxel_grid_data.res_rcp;
		output.position.zw = 1;

		// Append the rest of the parameters as is:
		output.color = input[i].color;
		output.normal = input[i].normal;

		outputStream.Append(output);
	}
}
