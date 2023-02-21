
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracingVoxelizationDebug_HF.hlsli"

PixelShaderOutputVoxelConeTracingVoxelizationDebug main(PixelShaderInputVoxelConeTracingVoxelizationDebug input)
{
	PixelShaderOutputVoxelConeTracingVoxelizationDebug output = (PixelShaderOutputVoxelConeTracingVoxelizationDebug) 0;
	if (input.color.a < 0.5f)
	{
		discard;
	}
		
	output.result = float4(input.color.rgb, 1.0f);
	return output;
}