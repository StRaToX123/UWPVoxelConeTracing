#include "Composite_HF.hlsli"



PixelShaderInputComposite main(VertexShaderInputComposite input)
{
	PixelShaderInputComposite result;

	result.position = float4(input.position.xyz, 1);
	result.uv = input.uv;

	return result;
}