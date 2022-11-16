#ifndef _SHADER_GLOBALS_GPU_
#define _SHADER_GLOBALS_GPU_

#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\ShaderGlobalsCPUGPU.hlsli"

struct ShaderStructureGPURootConstants
{
	uint transform_matrix_buffer_index;
	uint transform_matrix_buffer_inner_index;
};

struct ShaderStructureGPUViewProjectionBuffer
{
	matrix view;
	matrix projection;
};

struct ShaderStructureGPUModelAndInverseTransposeModelView
{
	matrix model;
	matrix inverse_transpose_model_view;
};

struct ShaderStructureGPUTransformBuffer
{
	ShaderStructureGPUModelAndInverseTransposeModelView data[TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES];
};

inline uint PackUnitvector(in float3 value)
{
	uint retVal = 0;
	retVal |= (uint) ((value.x * 0.5 + 0.5) * 255.0) << 0u;
	retVal |= (uint) ((value.y * 0.5 + 0.5) * 255.0) << 8u;
	retVal |= (uint) ((value.z * 0.5 + 0.5) * 255.0) << 16u;
	return retVal;
}
inline float3 UnpackUnitvector(in uint value)
{
	float3 retVal;
	retVal.x = (float) ((value >> 0u) & 0xFF) / 255.0 * 2 - 1;
	retVal.y = (float) ((value >> 8u) & 0xFF) / 255.0 * 2 - 1;
	retVal.z = (float) ((value >> 16u) & 0xFF) / 255.0 * 2 - 1;
	return retVal;
}

// 3D array index to flattened 1D array index
inline uint Flatten3DIndex(uint3 coord, uint3 dim)
{
	return (coord.z * dim.x * dim.y) + (coord.y * dim.x) + coord.x;
}



#endif