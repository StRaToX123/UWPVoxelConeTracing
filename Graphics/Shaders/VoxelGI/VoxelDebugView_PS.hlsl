
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 col : COLOR;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	return input.col;
}