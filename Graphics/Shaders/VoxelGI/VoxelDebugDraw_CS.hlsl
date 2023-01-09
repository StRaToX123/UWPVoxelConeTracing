
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\RadianceGenerate3DMipChain_HF.hlsli"

SamplerState point_sampler : register(s0);
AppendStructuredBuffer<ShaderStructureGPUVoxelDebugData> voxel_debug_data_required_for_frame_draw : register(u0);
RWStructuredBuffer<IndirectCommandGPU> voxel_debug_indirect_command : register(u1);
// The ShaderStructureGPUGenerate3DMipChainData is used here to tell us from which mip level
// We should be sampling and generating debug voxel from
ConstantBuffer<ShaderStructureGPUGenerate3DMipChainData> generate_3d_mip_chain_data : register(b4);
Texture3D<float4> radiance_texture_3D_SRVs[] : register(t4);


[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	const uint3 voxelIndex = UnFlatten1DTo3DIndex(dispatchThreadID.x, generate_3d_mip_chain_data.output_resolution);
	float4 color = radiance_texture_3D_SRVs[generate_3d_mip_chain_data.input_mip_level][voxelIndex];
	[branch]
	if (color.a > 0.0f)
	{
		ShaderStructureGPUVoxelDebugData debugVoxel;
		debugVoxel.index = voxelIndex;
		debugVoxel.color = float4(color.rgb, 1.0f);
		debugVoxel.padding1 = 0;
		voxel_debug_data_required_for_frame_draw.Append(debugVoxel);
		InterlockedAdd(voxel_debug_indirect_command[0].instance_count, 1);
	}
}