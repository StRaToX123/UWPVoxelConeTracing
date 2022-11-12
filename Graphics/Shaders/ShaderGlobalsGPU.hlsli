#ifndef _SHADER_GLOBALS_GPU_
#define _SHADER_GLOBALS_GPU_

#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\ShaderGlobalsCPUGPU.hlsli"

struct ShaderStructureGPURootConstants
{
	uint model_transform_matrix_buffer_index;
	uint model_transform_matrix_buffer_inner_index;
};

struct ShaderStructureGPUViewProjectionBuffer
{
	matrix view;
	matrix projection;
};

struct ShaderStructureGPUModelTransformMarixBuffer
{
	matrix model[MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES];
};


#endif