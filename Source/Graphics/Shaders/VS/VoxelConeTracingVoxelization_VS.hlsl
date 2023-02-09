#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracingVoxelization_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShaderStructures_HF.hlsli"

ConstantBuffer<ShaderStructureGPUModelData> model_data : register(b2);

GeometryShaderInputVoxelConeTracingVoxelization main(VertexShaderInputVoxelConeTracingVoxelization input)
{
	GeometryShaderInputVoxelConeTracingVoxelization output = (GeometryShaderInputVoxelConeTracingVoxelization) 0;
    
	output.position = mul(float4(input.position.xyz, 1), model_data.model);
	return output;
}