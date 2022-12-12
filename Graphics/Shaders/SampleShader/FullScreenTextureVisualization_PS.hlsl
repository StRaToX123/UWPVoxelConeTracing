
struct VertexShaderOutputFullScreenTextureVisualization
{
	float4 position : SV_Position;
	float2 tex_coord : TEXCOORD;
};




Texture2D<float> texture_spot_light_depth_buffer : register(t3);
SamplerState samp : register(s0);




float4 main(VertexShaderOutputFullScreenTextureVisualization input) : SV_Target
{
	//return pow(abs(testTexture.Sample(samp, input.tex)), 1.0f / 2.2f);
	float value = texture_spot_light_depth_buffer.Sample(samp, input.tex_coord);
	return float4(value, value, value, 1.0f);
}