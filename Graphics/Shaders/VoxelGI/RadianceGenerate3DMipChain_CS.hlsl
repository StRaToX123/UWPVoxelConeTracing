#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\RadianceGenerate3DMipChain_HF.hlsli"


// Linear sampler
SamplerState linear_sampler : register(s1);
ConstantBuffer<ShaderStructureGPUGenerate3DMipChainData> generate_3d_mip_chain_data : register(b4);
Texture3D<float4> radiance_texture_3D_SRVs[] : register(t4);
RWTexture3D<float4> radiance_texture_3D_UAVs[] : register(u3);


[numthreads(GENERATE_3D_MIP_CHAIN_DISPATCH_BLOCK_SIZE, GENERATE_3D_MIP_CHAIN_DISPATCH_BLOCK_SIZE, GENERATE_3D_MIP_CHAIN_DISPATCH_BLOCK_SIZE)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	if (dispatchThreadID.x < generate_3d_mip_chain_data.output_resolution && dispatchThreadID.y < generate_3d_mip_chain_data.output_resolution && dispatchThreadID.z < generate_3d_mip_chain_data.output_resolution)
	{
		Texture3D<float4> input = radiance_texture_3D_SRVs[generate_3d_mip_chain_data.input_mip_index];
		RWTexture3D<float4> output = radiance_texture_3D_UAVs[generate_3d_mip_chain_data.output_mip_index];
		//float3 sampleCoords = ;
		// We need to invert the y axis uv coordinate, since when we'll be using the Texsture3D SampleLevel function
		// in order to read from the radiance 3d texture. This function looks at the 3D texture as a series of 2D ones
		// each slice being referenced by the standard DirectX texture uvw system, where the X axis grows to the right, 
		// the Z axis grows forwards and the Y axis grows downwards.
		// Now since we voxelized the scene using the bottom left point of the grid as reference, that means our Y axis
		// for the voxel indexes grows upwards, so here we have to flip the y coordinate in order for it to be inline
		// with the way we voxelized the scene, so that each slice of the 3D texture would be read from the bottom left point
		// instead of the top left one
		//sampleCoords.y = 1.0f - sampleCoords.y;
		float4 color = input.SampleLevel(linear_sampler, ((float3) dispatchThreadID + 0.5f) * (float3) generate_3d_mip_chain_data.output_resolution_rcp, 0);
		
		color.a = 1.0f;
		output[dispatchThreadID] = color;
	}
}