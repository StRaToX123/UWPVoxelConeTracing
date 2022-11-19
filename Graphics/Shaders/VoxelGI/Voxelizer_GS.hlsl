#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"


ConstantBuffer<ShaderStructureGPURootConstants> root_constants : register(b0);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);
ConstantBuffer<ShaderStructureGPUTransformBuffer> transform_matrix_buffers[] : register(b3);

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
		output.position = mul(output.position, transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].model);
		output.position_world_space = output.position.xyz;
		output.position_view_space = mul(float4(output.position_world_space, 1.0f), view_projection_matrix_buffer.view).xyz;
		// World space -> Voxel grid space:
		output.position = (output.position.xyz - voxel_grid_data.center_world_space) * voxel_grid_data.voxel_half_extent_rcp;
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

		output.color = input[i].color;
		output.normal_view_space = mul(float4(input[i].normal, 1.0f), transform_matrix_buffers[root_constants.transform_matrix_buffer_index].data[root_constants.transform_matrix_buffer_inner_index].inverse_transpose_model_view).xyz;

		outputStream.Append(output);
	}
}
