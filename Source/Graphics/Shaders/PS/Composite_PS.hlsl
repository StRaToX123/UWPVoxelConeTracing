#include "Composite_HF.hlsli"



Texture2D<float4> lightBuffer : register(t0);

PixelShaderOutputComposite main(PixelShaderInputComposite input)
{
	PixelShaderOutputComposite output = (PixelShaderOutputComposite) 0;

	float3 color = lightBuffer[input.position.xy].rgb;
	output.colour.rgb = color;
	return output;
}