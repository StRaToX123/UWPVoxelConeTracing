#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\GBuffer_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShaderStructures_HF.hlsli"

ConstantBuffer<ShaderStructureGPUModelData> model_data : register(b1);

PixelShaderOutputGBuffer main(PixelShaderInputGBuffer input)
{
	PixelShaderOutputGBuffer output = (PixelShaderOutputGBuffer) 0;

	output.color = model_data.diffuse_color;
	output.normal = float4(input.normal, 1.0f);
	output.worldpos = float4(input.worldPos, 1.0f);
	return output;
}