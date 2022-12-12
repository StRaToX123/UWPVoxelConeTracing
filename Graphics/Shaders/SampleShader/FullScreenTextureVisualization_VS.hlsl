#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"


struct VertexShaderOutputFullScreenTextureVisualization
{
	float4 position : SV_Position;
	float2 tex_coord : TEXCOORD;
};





VertexShaderOutputFullScreenTextureVisualization main(VertexShaderInputDefault input)
{
	VertexShaderOutputFullScreenTextureVisualization output;
	
	output.position = float4(input.position, 1.0f);
	output.tex_coord = input.tex_coord;
	
	return output;
}