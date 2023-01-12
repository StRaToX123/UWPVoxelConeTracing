
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\FinalGather_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"
#include "C:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\ConeDirectionLineDraw_HF.hlsli"

cbuffer CBufferShaderStructureGPUVoxelGridData : register(b1)
{
	ShaderStructureGPUVoxelGridData voxel_grid_data;
};

Texture3D<float4> radiance_texture_3D_SRV : register(t4);
SamplerState linear_sampler : register(s1);
AppendStructuredBuffer<ShaderStructureGPUConeDirectionDebugData> indirect_draw_required_cone_direction_debug_data : register(u4);
RWStructuredBuffer<IndirectCommandGPU> indirect_command_cone_direction_debug : register(u5);

float4 main(VertexShaderOutputFinalGatherCopy input) : SV_TARGET
{
	// Determine blending factor
	// The closer the geometry is to the border of the voxel grid, the less the GI contribution will be 
	// This way if the grid moves around, the indirect contribution will smoothly fade in
	float3 voxelSpacePos = input.position_world_space - voxel_grid_data.center_world_space;
	voxelSpacePos *= voxel_grid_data.grid_half_extent_rcp;
	voxelSpacePos = saturate(abs(voxelSpacePos));
	float blend = 1 - pow(max(voxelSpacePos.x, max(voxelSpacePos.y, voxelSpacePos.z)), 4);

	// Diffuse:
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
		float distanceTraveled = voxel_grid_data.voxel_half_extent;
		// Offset by cone dir so that first sample of all cones are not the same
		float3 startingPosition = input.position_world_space + input.normal_world_space * voxel_grid_data.voxel_half_extent * SQRT2 * VOXEL_INITIAL_OFFSET; // sqrt2 is here because when multiplied by the voxel half extent equals the diagonal of the voxel

		// We will break off the loop if the sampling distance is too far for performance reasons:
		//uint kurcetina = 0;
		while (distanceTraveled < voxel_grid_data.max_distance && coneTraceAlpha < 1.0f)
		//while (kurcetina < 20)
		{
			float diameter = max(voxel_grid_data.voxel_half_extent, 2 * voxel_grid_data.cone_aperture * distanceTraveled);
			// The closer the diameter is to the resolution (size of the grid) the lower the mip we're sampling from should be
			// In other words the closer the cone gets to the edges of the grid the larger the mip. So we convert the diameter
			// to a grid resolution value in order to calculated a mip level from it
			float mip = log2(diameter * voxel_grid_data.voxel_half_extent_rcp);
			// Because we do the ray-marching in world space, we need to remap into 3d texture space before sampling:
			
			
			float3 voxelGridCoords = startingPosition + coneDirection * distanceTraveled;
			ShaderStructureGPUConeDirectionDebugData coneDirectionLine;
			coneDirectionLine.line_vertices_world_position[0] = voxelGridCoords;
			coneDirectionLine.line_vertices_world_position[1] = voxelGridCoords + (coneDirection * 0.05f);
			coneDirectionLine.padding1 = float2(0.0f, 0.0f);
			
			//voxelGridCoords = (voxelGridCoords - voxel_grid_data.center_world_space) * voxel_grid_data.grid_half_extent_rcp;
			//voxelGridCoords = (voxelGridCoords * float3(0.5f, -0.5f, 0.5f)) + 0.5f;
			voxelGridCoords = (voxelGridCoords - voxel_grid_data.top_left_point_world_space) * voxel_grid_data.grid_extent_rcp;
			voxelGridCoords.y *= -1;

			// break if the ray exits the voxel grid, or we sample from the last mip:
			if (!IsSaturated(voxelGridCoords) || mip >= (float) voxel_grid_data.mip_count)
				break;

			float4 sampledRadiance = radiance_texture_3D_SRV.SampleLevel(linear_sampler, voxelGridCoords, mip);
			
			if (((uint) input.pos.x == 600) && ((uint) input.pos.y == 383))
			{
				/*
				ShaderStructureGPUConeDirectionDebugData coneDirectionLine;
				coneDirectionLine.line_vertices_world_position[0] = input.position_world_space;
				coneDirectionLine.line_vertices_world_position[1] = input.position_world_space + coneDirection;
				coneDirectionLine.padding1 = float2(0.0f, 0.0f);
				*/
				
				if ((sampledRadiance.x > 0.0f) || (sampledRadiance.y > 0.0f) || (sampledRadiance.z > 0.0f) || (sampledRadiance.w > 0.0f))
				{
					coneDirectionLine.color = float4(1.0f, 0.0f, 0.0f, 1.0f);

				}
				else
				{
					coneDirectionLine.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
				}
				
				/*
				switch ((uint)floor(mip))
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
				*/
				indirect_draw_required_cone_direction_debug_data.Append(coneDirectionLine);
				InterlockedAdd(indirect_command_cone_direction_debug[0].instance_count, 1);
			}
			
			
			
			// This is the correct blending to avoid black-staircase artifact (ray stepped front-to back, so blend front to back):
			// In other words the amount of color accumulated over the tracing is reduced with each new sampling
			// Also the amount the alpha falls off over time is reduced, creating a smooth curve towards the alpha reaching zero
			float a = 1 - coneTraceAlpha;
			coneTraceColor += a * sampledRadiance.rgb;
			coneTraceAlpha += a * sampledRadiance.a;

			// Step along the ray:
			distanceTraveled += diameter * voxel_grid_data.ray_step_size;
			
			//kurcetina++;
		}

		indirectDiffuseColor.rgb += coneTraceColor;
		indirectDiffuseColor.a += coneTraceAlpha;
	}

	// final radiance is average of all the cones radiances
	indirectDiffuseColor *= voxel_grid_data.num_cones_rcp;
	indirectDiffuseColor.rgb = max(0, indirectDiffuseColor.rgb);
	indirectDiffuseColor.a = saturate(indirectDiffuseColor.a);
	
	
	return float4(lerp(input.color.rgb, indirectDiffuseColor.rgb * 3.0f, indirectDiffuseColor.a * blend), 1.0f);
	//return float4(input.color.rgb * indirectDiffuseColor.a * blend, 1.0f);
	
	
	
	
	//return float4(indirectDiffuseColor.rgb, 1.0f);
}