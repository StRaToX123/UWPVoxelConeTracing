#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\ShaderStructures_HF.hlsli"

#define NUM_CONES 6

static const float coneAperture = 0.577f; // 6 cones, 60deg each, tan(30deg) = aperture
static const float3 diffuseConeDirections[] =
{
	float3(0.0f, 1.0f, 0.0f),
    float3(0.0f, 0.5f, 0.866025f),
    float3(0.823639f, 0.5f, 0.267617f),
    float3(0.509037f, 0.5f, -0.7006629f),
    float3(-0.50937f, 0.5f, -0.7006629f),
    float3(-0.823639f, 0.5f, 0.267617f)
};
static const float diffuseConeWeights[] =
{
	0.25, 0.15, 0.15, 0.15, 0.15, 0.15
};

static const float specularOneDegree = 0.0174533f; //in radians
static const int specularMaxDegreesCount = 2;

Texture2D<float4> albedoBuffer : register(t0);
Texture2D<float4> normalBuffer : register(t1);
Texture2D<float4> worldPosBuffer : register(t2);
Texture3D<float4> voxelTexturePosX : register(t3);
Texture3D<float4> voxelTextureNegX : register(t4);
Texture3D<float4> voxelTexturePosY : register(t5);
Texture3D<float4> voxelTextureNegY : register(t6);
Texture3D<float4> voxelTexturePosZ : register(t7);
Texture3D<float4> voxelTextureNegZ : register(t8);
Texture3D<float4> voxelTexture : register(t9);
RWTexture2D<float4> result : register(u0);
SamplerState LinearSampler : register(s0);
ConstantBuffer<ShaderStructureGPUVoxelizationData> voxelization_data : register(b0);
ConstantBuffer<ShaderStructureGPUVCTMainData> vct_main_data : register(b1);
ConstantBuffer<ShaderStructureGPUCameraData> camera_data : register(b2);

float4 GetAnisotropicSample(float3 uv, float3 weight, float lod, bool posX, bool posY, bool posZ)
{
	int anisoLevel = max(lod - 1.0f, 0.0f);
    
	uint width;
	uint height;
	uint depth;
	voxelTexturePosX.GetDimensions(width, height, depth);
    
	width >>= anisoLevel;
	height >>= anisoLevel;
	depth >>= anisoLevel;
    
	float4 anisoSample =
    weight.x * ((posX) ? voxelTexturePosX.SampleLevel(LinearSampler, uv, anisoLevel) : voxelTextureNegX.SampleLevel(LinearSampler, uv, anisoLevel)) +
    weight.y * ((posY) ? voxelTexturePosY.SampleLevel(LinearSampler, uv, anisoLevel) : voxelTextureNegY.SampleLevel(LinearSampler, uv, anisoLevel)) +
    weight.z * ((posZ) ? voxelTexturePosZ.SampleLevel(LinearSampler, uv, anisoLevel) : voxelTextureNegZ.SampleLevel(LinearSampler, uv, anisoLevel));

	if (lod < 1.0f)
	{
		float4 baseColor = voxelTexture.SampleLevel(LinearSampler, uv, 0);
		anisoSample = lerp(baseColor, anisoSample, clamp(lod, 0.0f, 1.0f));
	}

	return anisoSample;
}

//float4 GetVoxel(float3 worldPosition, float3 weight, float lod, bool posX, bool posY, bool posZ)
//{
//	float3 offset = float3(vct_main_data.voxel_sample_offset, vct_main_data.voxel_sample_offset, vct_main_data.voxel_sample_offset);
//	float3 voxelTextureUV = worldPosition / voxelization_data.voxel_scale * 2.0f;
//	voxelTextureUV.y = -voxelTextureUV.y;
//	voxelTextureUV = voxelTextureUV * 0.5f + 0.5f + offset;
    
//	return GetAnisotropicSample(voxelTextureUV, weight, lod, posX, posY, posZ);
//}

float4 GetVoxel(float3 worldPosition, float3 weight, float lod, bool posX, bool posY, bool posZ)
{
	//float3 offset = float3(vct_main_data.voxel_sample_offset, vct_main_data.voxel_sample_offset, vct_main_data.voxel_sample_offset);
	int3 voxelIndex = (worldPosition - voxelization_data.voxel_grid_top_left_back_point_world_space) * voxelization_data.voxel_extent_rcp;
	voxelIndex.y *= -1;
	float3 voxelTextureUV = (float3) voxelIndex / (voxelization_data.voxel_grid_res - 1);
    
	return GetAnisotropicSample(voxelTextureUV, weight, lod, posX, posY, posZ);
}

float4 TraceCone(float3 pos, float3 normal, float3 direction, float aperture, out float occlusion, bool calculateAO, uint voxelResolution)
{
	float lod = 0.0;
	float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);

	occlusion = 0.0f;
	//float voxelWorldSize = voxelization_data.voxel_scale / voxelResolution;
	float voxelWorldSize = 0.5f;
	float dist = voxelWorldSize;
	float3 startPos = pos + normal * dist;
    
	float3 weight = direction * direction;

	while (dist < vct_main_data.max_cone_trace_distance && color.a < 1.0f)
	{
		float diameter = 2.0f * aperture * dist;
		float lodLevel = log2(diameter / voxelWorldSize);
		float4 voxelColor = GetVoxel(startPos + dist * direction, weight, lodLevel, direction.x > 0.0, direction.y > 0.0, direction.z > 0.0);
    
        // front-to-back
		color += (1.0 - color.a) * voxelColor;
		if (occlusion < 1.0f && calculateAO)
			occlusion += ((1.0f - occlusion) * voxelColor.a) / (1.0f + vct_main_data.ao_falloff * diameter);
        
		dist += diameter * vct_main_data.sampling_factor;
	}

	return color;
}

float4 CalculateIndirectSpecular(float3 worldPos, float3 normal, float4 specular, uint voxelResolution)
{
	float4 result;
	float3 viewDirection = normalize(camera_data.position_world_space - worldPos);
	float3 coneDirection = normalize(reflect(-viewDirection, normal));
        
	float aperture = clamp(tan(PI * 0.5 * (1.0f - /*specular.a*/0.9f)), specularOneDegree * specularMaxDegreesCount, PI);

	float ao = -1.0f;
	result = TraceCone(worldPos, normal, coneDirection, aperture, ao, false, voxelResolution);
    
	return vct_main_data.indirect_specular_strength * result * float4(specular.rgb, 1.0f) * specular.a;
}

float4 CalculateIndirectDiffuse(float3 worldPos, float3 normal, out float ao, uint voxelResolution)
{
	float4 result;
	float3 coneDirection;
    
	float3 upDir = float3(0.0f, 1.0f, 0.0f);
	if (abs(dot(normal, upDir)) == 1.0f)
		upDir = float3(0.0f, 0.0f, 1.0f);

	float3 right = normalize(upDir - dot(normal, upDir) * normal);
	float3 up = cross(right, normal);
    
	float finalAo = 0.0f;
	float tempAo = 0.0f;
    
	for (int i = 0; i < NUM_CONES; i++)
	{
		coneDirection = normal;
		coneDirection += diffuseConeDirections[i].x * right + diffuseConeDirections[i].z * up;
		coneDirection = normalize(coneDirection);

		result += TraceCone(worldPos, normal, coneDirection, coneAperture, tempAo, true, voxelResolution) * diffuseConeWeights[i];
		finalAo += tempAo * diffuseConeWeights[i];
	}
    
	ao = finalAo;
    
	return vct_main_data.indirect_diffuse_strength * result;
}

[numthreads(8, 8, 1)]
void main(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	float2 inPos = DTid.xy;
    
	float3 normal = normalize(normalBuffer[inPos * vct_main_data.upsample_ratio].rgb);
	float4 worldPos = worldPosBuffer[inPos * vct_main_data.upsample_ratio];
	float4 albedo = albedoBuffer[inPos * vct_main_data.upsample_ratio];
    
	uint width;
	uint height;
	uint depth;
	voxelTexture.GetDimensions(width, height, depth);
    
	float ao = 0.0f;
	float4 indirectDiffuse = CalculateIndirectDiffuse(worldPos.rgb, normal.rgb, ao, width);
	float4 indirectSpecular = CalculateIndirectSpecular(worldPos.rgb, normal.rgb, albedo, width);

	result[inPos] = saturate(float4(indirectDiffuse.rgb * albedo.rgb + indirectSpecular.rgb, ao));
}