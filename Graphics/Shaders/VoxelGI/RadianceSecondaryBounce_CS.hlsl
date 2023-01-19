
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "C:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "C:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\RadianceSecondaryBounceCPUGPU.hlsli"



cbuffer CBufferShaderStructureGPUVoxelGridData : register(b1)
{
	ShaderStructureGPUVoxelGridData voxel_grid_data;
};

Texture3D<float4> radiance_texture_3D_SRV : register(t4);
RWTexture3D<float4> radiance_texture_3D_helper_UAV : register(u6);
RWStructuredBuffer<VoxelType> voxel_data_rw_structured_buffer : register(u2);
SamplerState linear_sampler : register(s1);


[numthreads(RADIANCE_SECONDARY_BOUNCE_DISPATCH_BLOCK_SIZE, RADIANCE_SECONDARY_BOUNCE_DISPATCH_BLOCK_SIZE, RADIANCE_SECONDARY_BOUNCE_DISPATCH_BLOCK_SIZE)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	float4 emission = radiance_texture_3D_SRV[dispatchThreadID];
	if (emission.a > 0)
	{
		float3 N = UnpackUnitVector(voxel_data_rw_structured_buffer[Flatten3DIndex(dispatchThreadID, voxel_grid_data.res)].normal);
		float3 P = (dispatchThreadID * voxel_grid_data.voxel_extent) + voxel_grid_data.voxel_half_extent;
		P.y *= -1.0f;

		float4 radiance = ConeTraceDiffuse(radiance_texture_3D_SRV, voxel_grid_data, linear_sampler, P, N);
		radiance_texture_3D_helper_UAV[dispatchThreadID] = emission + float4(radiance.rgb, 0);
	}
	else
	{
		radiance_texture_3D_helper_UAV[dispatchThreadID] = 0;
	}
}