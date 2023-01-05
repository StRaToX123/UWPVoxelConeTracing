
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\FinalGather_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"


cbuffer CBufferShaderStructureGPUVoxelGridData : register(b1)
{
	ShaderStructureGPUVoxelGridData voxel_grid_data;
};

Texture3D<float4> radiance_texture_3D_SRV : register(t4);
SamplerState linear_sampler : register(s1);

float4 main(VertexShaderOutputFinalGather input) : SV_TARGET
{
	// Determine blending factor
	// The closer the geometry is to the border of the voxel grid, the less the GI contribution will be 
	// This way if the grid moves around, the indirect contribution will smoothly fade in
	float3 voxelSpacePos = input.position_world_space - voxel_grid_data.center_world_space;
	//voxelSpacePos *= voxel_grid_data.voxel_half_extent_rcp;
	voxelSpacePos *= voxel_grid_data.grid_half_extent_rcp;
	voxelSpacePos = saturate(abs(voxelSpacePos));
	float blend = 1 - pow(max(voxelSpacePos.x, max(voxelSpacePos.y, voxelSpacePos.z)), 4);

	// Diffuse:
	float4 trace = ConeTraceDiffuse(radiance_texture_3D_SRV, voxel_grid_data, linear_sampler, input.position_world_space, input.normal_world_space);
	//return float4(lerp(input.color.rgb, trace.rgb, trace.a * blend), 1.0f);
	//return float4(input.color.rgb * trace.a * blend, 1.0f);
	return float4(trace.rgb, 1.0f);
	
	
	/*
		// specular:
		[branch]
	if (GetFrame().options & OPTION_BIT_VOXELGI_REFLECTIONS_ENABLED)
	{
		float4 trace = ConeTraceSpecular(texture_voxelgi, surface.P, surface.N, surface.V, surface.roughness);
		lighting.indirect.specular = lerp(lighting.indirect.specular, trace.rgb * surface.F, trace.a * blend);
	}
*/
}