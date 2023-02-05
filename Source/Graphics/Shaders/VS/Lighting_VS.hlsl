
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\Lighting_HF.hlsli"



PixelShaderInputLighting main(VertexShaderInputLighting input)
{
	PixelShaderInputLighting result;

	result.position = float4(input.position.xyz, 1);
	result.uv = input.uv;
    
	return result;
}