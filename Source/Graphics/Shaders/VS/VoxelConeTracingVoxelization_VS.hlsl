
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracingVoxelization_HF.hlsli"


cbuffer perModelInstanceCB : register(b1)
{
	float4x4 World;
	float4 DiffuseColor;
};

GeometryShaderInputVoxelConeTracingVoxelization main(VertexShaderInputVoxelConeTracingVoxelization input)
{
	GeometryShaderInputVoxelConeTracingVoxelization output = (GeometryShaderInputVoxelConeTracingVoxelization) 0;
    
	output.position = mul(float4(input.position.xyz, 1), World);
	return output;
}