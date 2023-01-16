
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\FinalGather_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "C:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\ConeDirectionLineDraw_HF.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\SpotLight_HF.hlsli"


cbuffer CBufferShaderStructureGPUVoxelGridData : register(b1)
{
	ShaderStructureGPUVoxelGridData voxel_grid_data;
};

cbuffer SpotLightDataCBuffer : register(b3)
{
	ShaderStructureGPUSpotLight spot_light_data;
}

Texture2D<float> spot_light_shadow_map : register(t2);
Texture3D<float4> radiance_texture_3D_SRV : register(t4);
SamplerState point_sampler : register(s0);
SamplerState linear_sampler : register(s1);
AppendStructuredBuffer<ShaderStructureGPUConeDirectionDebugData> indirect_draw_required_cone_direction_debug_data : register(u4);
RWStructuredBuffer<IndirectCommandGPU> indirect_command_cone_direction_debug : register(u5);
ConstantBuffer<ShaderStructureGPUViewProjectionBuffer> view_projection_matrix_buffer : register(b2);

/*
float4 main(VertexShaderOutputFinalGatherCopy input) : SV_TARGET
{
	// Get the dirrect lighting color
	float4 directDiffuseColor = float4(input.color.rgb + spot_light_data.color.rgb, 1.0f);
	directDiffuseColor.rgb *= DoSpotLight(spot_light_data, normalize(-input.position_view_space), input.position_view_space, input.normal_view_space) / PI;
	// Apply the spot light shadow map
	// Calculate the projected texture coordinates.
	// we have to perform the perspective divide ourselves, and in order to remap the value from NDC to a texture coordinate
	// we have to divide by 2 and add 0.5f, which puts the values in a from -1 +1 range to a 0 1 range, we also have to 
	// flip the y axis since we will be using these coordinates to look up a texture, where the y axis grows downwards
	float2 projectTexCoord;
	projectTexCoord.x = input.spot_light_shadow_map_tex_coord.x / input.spot_light_shadow_map_tex_coord.w / 2.0f + 0.5f;
	projectTexCoord.y = -input.spot_light_shadow_map_tex_coord.y / input.spot_light_shadow_map_tex_coord.w / 2.0f + 0.5f;
	// Determine if the projected coordinates are in the 0 to 1 range.  If so then this pixel is in the view of the light.
	directDiffuseColor.rgb *= spot_light_shadow_map.SampleLevel(point_sampler, projectTexCoord, 0);
	
	
	
	float4 indirectDiffuseColor = ConeTraceDiffuse(radiance_texture_3D_SRV, voxel_grid_data, linear_sampler, input.position_world_space, input.normal_world_space);
	
	
	
	
	
	
	
	return float4(saturate(directDiffuseColor.rgb + (indirectDiffuseColor.rgb * 10.0f)), 1.0f);
}
*/

float4 main(VertexShaderOutputFinalGatherCopy input) : SV_TARGET
{	
	// Get the dirrect lighting color
	float4 directDiffuseColor = float4(input.color.rgb + spot_light_data.color.rgb, 1.0f);
	directDiffuseColor.rgb *= DoSpotLight(spot_light_data, normalize(-input.position_view_space), input.position_view_space, input.normal_view_space) / PI;
	// Apply the spot light shadow map
	// Calculate the projected texture coordinates.
	// we have to perform the perspective divide ourselves, and in order to remap the value from NDC to a texture coordinate
	// we have to divide by 2 and add 0.5f, which puts the values in a from -1 +1 range to a 0 1 range, we also have to 
	// flip the y axis since we will be using these coordinates to look up a texture, where the y axis grows downwards
	float2 projectTexCoord;
	projectTexCoord.x = input.spot_light_shadow_map_tex_coord.x / input.spot_light_shadow_map_tex_coord.w / 2.0f + 0.5f;
	projectTexCoord.y = -input.spot_light_shadow_map_tex_coord.y / input.spot_light_shadow_map_tex_coord.w / 2.0f + 0.5f;
	// Determine if the projected coordinates are in the 0 to 1 range.  If so then this pixel is in the view of the light.
	directDiffuseColor.rgb *= spot_light_shadow_map.SampleLevel(point_sampler, projectTexCoord, 0);
	

	
	
	// Indirect Diffuse:
	float4 indirectDiffuseColor = 0;
	float3x3 tangentSpaceToWorldSpaceMatrix = GetTangentSpaceToSuppliedNormalVectorSpace(input.normal_world_space);
	for (uint coneIndex = 0; coneIndex < voxel_grid_data.num_cones; ++coneIndex) // quality is between 1 and 16 cones
	{
		float2 hamm = Hammersley2D(coneIndex, voxel_grid_data.num_cones);
		float3 hemisphere = HemispherePointUVtoTanglentSpaceCosWeightedDistribution(hamm.x, hamm.y);
		float3 coneDirection = normalize(mul(hemisphere, tangentSpaceToWorldSpaceMatrix));
		
		// Trace this cone
		float3 coneTraceColor = 0;
		float coneTraceAlpha = 0;
		// We need to offset the cone start position to avoid sampling its own voxel (self-occlusion):
		// Unfortunately, it will result in disconnection between nearby surfaces :(
		//float distanceTraveled = voxel_grid_data.voxel_half_extent;
		float distanceTraveled = 0.0f;
		// Offset by cone dir so that first sample of all cones are not the same
		float3 startingPosition = input.position_world_space + input.normal_world_space * voxel_grid_data.voxel_half_extent * SQRT2 * VOXEL_INITIAL_OFFSET; // sqrt2 is here because when multiplied by the voxel half extent equals the diagonal of the voxel base
		//float3 startingPosition = input.position_world_space + (input.normal_world_space * voxel_grid_data.tracing_starting_position_offset);
		//float3 pointInTheDistance = startingPosition + (coneDirection * voxel_grid_data.max_distance * 2.0f);
		
		
		//uint traceCounter = voxel_grid_data.trace_counter;
		
		
		
		// We will break off the loop if the sampling distance is too far for performance reasons:
		while ((distanceTraveled < voxel_grid_data.max_distance)  && (coneTraceAlpha < 1.0f))
		{
			float diameter = max(voxel_grid_data.voxel_half_extent, 2 * voxel_grid_data.cone_aperture * distanceTraveled);
			// The closer the diameter is to the resolution (size of the grid) the lower the mip we're sampling from should be
			// In other words the closer the cone gets to the edges of the grid the larger the mip. So we convert the diameter
			// to a grid resolution value in order to calculated a mip level from it
			//float mip = log2(diameter * voxel_grid_data.voxel_half_extent_rcp);
			float mip = (float) (voxel_grid_data.mip_count - 1) - lerp(0.0f, (float) (voxel_grid_data.mip_count - 1), 1.0f - pow(distanceTraveled / voxel_grid_data.max_distance, voxel_grid_data.trace_mip_level_bias));
			
			
			
			//float3 startingPosition = input.position_world_space + (input.normal_world_space * (voxel_grid_data.voxel_half_extent * pow(2.0f, floor(mip))) * SQRT3); // sqrt2 is here because when multiplied by the voxel half extent equals the diagonal of the voxel base
			//float3 coneDirection = normalize(pointInTheDistance - startingPosition);
			// Because we do the ray-marching in world space, we need to remap into 3d texture space before sampling:
			float3 voxelGridCoords = startingPosition + coneDirection * distanceTraveled;
			ShaderStructureGPUConeDirectionDebugData coneDirectionLine;
			coneDirectionLine.line_vertices_world_position[0] = voxelGridCoords;
			coneDirectionLine.line_vertices_world_position[1] = voxelGridCoords + (coneDirection * 0.01f);
			coneDirectionLine.padding1 = float2(0.0f, 0.0f);
			
			
			voxelGridCoords = (voxelGridCoords - voxel_grid_data.top_left_point_world_space) * voxel_grid_data.grid_extent_rcp;
			voxelGridCoords.y *= -1;

			// break if the ray exits the voxel grid, or we sample from the last mip:
			if (!IsSaturated(voxelGridCoords) || mip >= (float) (voxel_grid_data.mip_count - 1))
				break;

			float4 sampledRadiance = radiance_texture_3D_SRV.SampleLevel(linear_sampler, voxelGridCoords, mip);
			//if (traceCounter > 0)
			//{
			//	traceCounter--;
			//}
			//else
			//{
				// This is the correct blending to avoid black-staircase artifact (ray stepped front-to back, so blend front to back):
				// In other words the amount of color accumulated over the tracing is reduced with each new sampling
				// Also the amount the alpha falls off over time is reduced, creating a smooth curve towards the alpha reaching zero
				float a = 1.0f - coneTraceAlpha;
				coneTraceColor += a * sampledRadiance.rgb;
				coneTraceAlpha += a * sampledRadiance.a;
			
				if (((uint) input.pos.x == 600) && ((uint) input.pos.y == 383))
				{
					coneDirectionLine.color = float4(coneTraceAlpha, 0.0f, 0.0f, 1.0f);
					switch ((uint) floor(mip))
					{
						case 0:
						{
								coneDirectionLine.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
								break;
							}
						case 1:
						{
								coneDirectionLine.color = float4(0.0f, 0.0f, 1.0f, 1.0f);
								break;
							}
						case 2:
						{
								coneDirectionLine.color = float4(0.0f, 1.0f, 0.0f, 1.0f);
								break;
							}
						case 3:
						{
								coneDirectionLine.color = float4(0.0f, 1.0f, 1.0f, 1.0f);
								break;
							}
						case 4:
						{
								coneDirectionLine.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
								break;
							}
						case 5:
						{
								coneDirectionLine.color = float4(1.0f, 0.0f, 1.0f, 1.0f);
								break;
							}
						case 6:
						{
								coneDirectionLine.color = float4(1.0f, 1.0f, 0.0f, 1.0f);
								break;
							}
						case 7:
						{
								coneDirectionLine.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
								break;
							}
					}
				
					indirect_draw_required_cone_direction_debug_data.Append(coneDirectionLine);
					InterlockedAdd(indirect_command_cone_direction_debug[0].instance_count, 1);
				}
			//}
			
			
			// Step along the ray:
			distanceTraveled += diameter * voxel_grid_data.ray_step_size;

		}

		indirectDiffuseColor.rgb += coneTraceColor;
		indirectDiffuseColor.a += coneTraceAlpha;
	}

	// final radiance is average of all the cones radiances
	indirectDiffuseColor *= voxel_grid_data.num_cones_rcp;
	indirectDiffuseColor.rgb = max(0, indirectDiffuseColor.rgb);
	indirectDiffuseColor.a = saturate(indirectDiffuseColor.a);
	
	
	//return float4(lerp(directDiffuseColor.xyz, directDiffuseColor.xyz + indirectDiffuseColor.rgb, indirectDiffuseColor.a * blend), 1.0f);
	//return float4(input.color.rgb * indirectDiffuseColor.a * blend, 1.0f);
	
	
	
	
	return float4(saturate(directDiffuseColor.rgb + (input.color.rgb * voxel_grid_data.diffuse_ambient_intensity) + (indirectDiffuseColor.rgb * indirectDiffuseColor.a * voxel_grid_data.indirect_diffuse_multiplier)), 1.0f);
}