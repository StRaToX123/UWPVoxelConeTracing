#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracingVoxelization_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShaderStructures_HF.hlsli"

ConstantBuffer<ShaderStructureGPUVoxelizationData> voxelization_data : register(b0);

[maxvertexcount(3)]
void main(triangle GeometryShaderInputVoxelConeTracingVoxelization input[3], inout TriangleStream<PixelShaderInputVoxelConeTracingVoxelization> OutputStream)
{
	PixelShaderInputVoxelConeTracingVoxelization output[3];
	output[0] = (PixelShaderInputVoxelConeTracingVoxelization) 0;
	output[1] = (PixelShaderInputVoxelConeTracingVoxelization) 0;
	output[2] = (PixelShaderInputVoxelConeTracingVoxelization) 0;
    
	float3 p1 = input[1].position.rgb - input[0].position.rgb;
	float3 p2 = input[2].position.rgb - input[0].position.rgb;
	float3 n = abs(normalize(cross(p1, p2)));
       
	float axis = max(n.x, max(n.y, n.z));
    
    [unroll]
	for (uint i = 0; i < 3; i++)
	{
		output[0].voxelPos = input[i].position.xyz / voxelization_data.voxel_scale * 2.0f;
		output[1].voxelPos = input[i].position.xyz / voxelization_data.voxel_scale * 2.0f;
		output[2].voxelPos = input[i].position.xyz / voxelization_data.voxel_scale * 2.0f;
		if (axis == n.z)
		{
			output[i].position = float4(output[i].voxelPos.x, output[i].voxelPos.y, 0, 1);
		}
		else if (axis == n.x)
		{
			output[i].position = float4(output[i].voxelPos.y, output[i].voxelPos.z, 0, 1);
		}
		else
		{
			output[i].position = float4(output[i].voxelPos.x, output[i].voxelPos.z, 0, 1);
		}
			
        //output[i].normal = input[i].normal;
		OutputStream.Append(output[i]);
	}
	
	OutputStream.RestartStrip();
}