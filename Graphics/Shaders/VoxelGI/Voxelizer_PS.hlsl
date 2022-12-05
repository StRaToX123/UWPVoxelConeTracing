#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\ShaderGlobalsGPU.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\VoxelGIGlobalsGPU.hlsli"


Texture2D<float4> test_texture : register(t0);
SamplerState samp : register(s0);
RWStructuredBuffer<VoxelType> output : register(u2);
ConstantBuffer<ShaderStructureGPUVoxelGridData> voxel_grid_data : register(b1);
//ConstantBuffer<ShaderStructureGPUSpotLight> spot_light_data : register(b3);

/*
float DoDiffuse(float3 N, float3 L)
{
	return max(0, dot(N, L));
}

float DoAttenuation(float attenuation, float distance)
{
	return 1.0f / (1.0f + attenuation * distance * distance);
}

float DoSpotCone(float3 spotDir, float3 L, float spotAngle)
{
	float minCos = cos(spotAngle);
	float maxCos = (minCos + 1.0f) / 2.0f;
	float cosAngle = dot(spotDir, -L);
	return smoothstep(minCos, maxCos, cosAngle);
}

float4 DoSpotLight(float3 V, float3 P, float3 N)
{
	float4 result;
	float3 L = (spot_light_data.position_view_space.xyz - P);
	float d = length(L);
	L = L / d;

	float attenuation = DoAttenuation(spot_light_data.attenuation, d);

	float spotIntensity = DoSpotCone(spot_light_data.direction_view_space.xyz, L, spot_light_data.spot_angle);

	result = DoDiffuse(N, L) * attenuation * spotIntensity * spot_light_data.color * spot_light_data.intensity;
	return result;
}
*/

void main(VoxelizerGeometryShaderOutput input)
{
	float3 voxelGridBottomLeftPointWorldSpace = float3(voxel_grid_data.bottom_left_point_world_space_x, voxel_grid_data.bottom_left_point_world_space_y, voxel_grid_data.bottom_left_point_world_space_z);
	int3 voxelIndex = (input.position_world_space - voxelGridBottomLeftPointWorldSpace) * voxel_grid_data.voxel_extent_rcp;
	
	float4 color = input.color;
	//color.rgb *= DoSpotLight(normalize(-input.position_view_space), input.position_view_space, normalize(input.normal_view_space)) / PI;

	//uint colorEncoded = PackVoxelColor(color);
	uint colorEncoded = PackVoxelColor(pow(abs(test_texture.Sample(samp, input.tex_coord)), 1.0f / 2.2f));
	uint normalEncoded = PackUnitvector(input.normal_view_space);
	
	// output:
	uint id = Flatten3DIndex(voxelIndex, voxel_grid_data.res);
	// The way the voxelIndex is calculated, it doesn't take into account what happens when the input.position_world_space
	// is out of bounds of the grid (we don't want to render those voxels).
	// If an index goes out of bounds by being positive, that means the index is > voxel_grid_data.res
	// it will wrap around when flattened in the Flatten3DIndex call and end up drawing a voxel on the other side of the axis.
	// In order to stop this we will draw only voxels whose axis index returns a negative value when subtracted by the grid res.
	// In other words if an axis index hasn't gone out of bounds then it is < grid res, hence it's sign of this subtraction will be negative.
	// We do this for each axis and add those values together, if the result is -3 then no axis index went out of bounds on the positive direction.
	// We also need to perform an out of bounds check for the negative direction.
	// We will add all the axis signs without subtracting, which if none went out of bounds (none are smaller than 0) should equal to 3
	// If the voxel didn't go out of bounds on the negative and on the positive direction, adding all of these values together should equal 0
	int isVoxelIndexOutOfBounds = sign(voxelIndex.x) + sign(voxelIndex.y) + sign(voxelIndex.z) + sign((int) voxelIndex.x - (int) voxel_grid_data.res) + sign((int) voxelIndex.y - (int) voxel_grid_data.res) + sign((int) voxelIndex.z - (int) voxel_grid_data.res);
	if(isVoxelIndexOutOfBounds == 0)
	{
		InterlockedMax(output[id].color, colorEncoded);
		InterlockedMax(output[id].normal, normalEncoded);
	}
}
