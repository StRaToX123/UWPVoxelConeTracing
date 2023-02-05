#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\GBuffer_HF.hlsli"


cbuffer perModelInstanceCB : register(b1)
{
	float4x4 World;
	float4 DiffuseColor;
};

PixelShaderOutputGBuffer main(PixelShaderInputGBuffer input)
{
	PixelShaderOutputGBuffer output = (PixelShaderOutputGBuffer) 0;

	output.color = DiffuseColor;
	output.normal = float4(input.normal, 1.0f);
	output.worldpos = float4(input.worldPos, 1.0f);
	return output;
}