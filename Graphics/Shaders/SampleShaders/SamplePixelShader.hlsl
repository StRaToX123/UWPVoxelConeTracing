
Texture2D<float4> testTexture : register(t0);
sampler samp : register(s0);


struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	return testTexture.Sample(samp, input.tex);
}
