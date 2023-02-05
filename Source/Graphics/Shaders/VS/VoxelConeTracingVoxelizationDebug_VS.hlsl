#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracingVoxelizationDebug_HF.hlsli"

RWTexture3D<float4> voxelTexture : register(u0);

GeometryShaderInputVoxelConeTracingVoxelizationDebug main(VertexShaderInputVoxelConeTracingVoxelizationDebug input)
{
	GeometryShaderInputVoxelConeTracingVoxelizationDebug output = (GeometryShaderInputVoxelConeTracingVoxelizationDebug) 0;
    
	uint width = 0;
	uint height = 0;
	uint depth = 0;
   
	voxelTexture.GetDimensions(width, height, depth);
    
	float3 centerVoxelPos;
	centerVoxelPos.x = input.vertexID % width;
	centerVoxelPos.y = input.vertexID / (width * width);
	centerVoxelPos.z = (input.vertexID / width) % width;
    
	output.position = float4(0.5f * centerVoxelPos + float3(0.5f, 0.5f, 0.5f), 1.0f);
	output.color = voxelTexture.Load(int4(centerVoxelPos, 0));
	return output;
}