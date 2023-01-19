#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"


RWStructuredBuffer<VoxelType> voxel_data_structured_buffer : register(u2);



[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	voxel_data_structured_buffer[dispatchThreadID.x].normal = 0;
}