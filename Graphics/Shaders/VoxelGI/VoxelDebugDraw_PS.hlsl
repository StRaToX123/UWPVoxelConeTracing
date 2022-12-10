#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelDebugDraw_HF.hlsli"


float4 main(VoxelDebugDrawPixelShaderInput input) : SV_TARGET
{
	return input.color;
}