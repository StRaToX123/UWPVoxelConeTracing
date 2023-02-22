#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracingDebugIndirectDraw_HF.hlsli"


float4 main(PixelShaderInputVoxelConeTracingDebugIndirectDraw input) : SV_TARGET
{
	return input.color;
	//return pow(abs(input.color), 1.0f / 2.2f);
}