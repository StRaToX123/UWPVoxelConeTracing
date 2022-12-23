#ifndef _GENERATE_3D_MIP_CHAIN_
#define _GENERATE_3D_MIP_CHAIN_

#define GENERATE_3D_MIP_CHAIN_DISPATCH_BLOCK_SIZE 4

struct ShaderStructureGPUGenerate3DMipChainData
{
	uint3 output_resolution;
	float3 output_resolution_rcp;
	uint input_mip_index;
	uint output_mip_index;
};

#endif