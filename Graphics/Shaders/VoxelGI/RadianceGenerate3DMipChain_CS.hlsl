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
		Texture3D<float4> input = radiance_texture_3D_SRVs[generate_3d_mip_chain_data.input_mip_level];
		RWTexture3D<float4> output = radiance_texture_3D_UAVs[generate_3d_mip_chain_data.output_mip_level];
		float4 color = input.SampleLevel(linear_sampler, ((float3) dispatchThreadID + 0.5f) * (float3) generate_3d_mip_chain_data.output_resolution_rcp, 0);
		
		output[dispatchThreadID] = color;
	}
}