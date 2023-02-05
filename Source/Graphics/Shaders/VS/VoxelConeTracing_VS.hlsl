
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracing_HF.hlsli"


PixelShaderInputVoxelConeTracing main(VertexShaderInputVoxelConeTracing input)
{
	PixelShaderInputVoxelConeTracing result = (PixelShaderInputVoxelConeTracing) 0;

	result.position = float4(input.position.xyz, 1);
	result.uv = input.uv;
    
	return result;
}