#ifndef _GENERATE_3D_MIP_CHAIN_
#define _GENERATE_3D_MIP_CHAIN_

#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\RadianceGenerate3DMipChainCPUGPU.hlsli"

struct ShaderStructureGPUGenerate3DMipChainData
{
	uint output_resolution;
	float output_resolution_rcp;
	uint input_mip_level;
	uint output_mip_level;
};

#endif