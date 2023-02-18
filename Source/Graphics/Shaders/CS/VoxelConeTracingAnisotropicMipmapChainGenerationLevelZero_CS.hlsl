#include "c:\users\stratox\documents\visual studio 2019\projects\uwpvoxelconetracing\Source\Graphics\Shaders\HF\ShaderStructures_HF.hlsli"

Texture3D<float4> source : register(t0);

RWTexture3D<float4> sourceResultPosX : register(u0);
RWTexture3D<float4> sourceResultNegX : register(u1);
RWTexture3D<float4> sourceResultPosY : register(u2);
RWTexture3D<float4> sourceResultNegY : register(u3);
RWTexture3D<float4> sourceResultPosZ : register(u4);
RWTexture3D<float4> sourceResultNegZ : register(u5);

ConstantBuffer<ShaderStructureGPUAnisotropicMipGenerationData> mip_mapping_data : register(b0);

static const int3 anisoOffsets[8] = 
	{
	int3(1, 1, 1),
	int3(1, 1, 0),
	int3(1, 0, 1),
	int3(1, 0, 0),
	int3(0, 1, 1),
	int3(0, 1, 0),
	int3(0, 0, 1),
	int3(0, 0, 0)
};

[numthreads(8, 8, 8)]
void main(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= mip_mapping_data.mip_dimension || DTid.y >= mip_mapping_data.mip_dimension || DTid.z >= mip_mapping_data.mip_dimension)
	{
		return;
	}	
    
	float4 values[8];
	int3 sourcePos = DTid * 2;
	
    [unroll]
	for (int i = 0; i < 8; i++)
	{
		values[i] = source.Load(int4(sourcePos + anisoOffsets[i], 0));
	}
		
	sourceResultPosX[DTid] =
        (values[4] + values[0] * (1 - values[4].a) + values[5] + values[1] * (1 - values[5].a) +
        values[6] + values[2] * (1 - values[6].a) + values[7] + values[3] * (1 - values[7].a)) * 0.25f;
    
	sourceResultNegX[DTid] =
        (values[0] + values[4] * (1 - values[0].a) + values[1] + values[5] * (1 - values[1].a) +
		values[2] + values[6] * (1 - values[2].a) + values[3] + values[7] * (1 - values[3].a)) * 0.25f;
    
	sourceResultPosY[DTid] =
	    (values[2] + values[0] * (1 - values[2].a) + values[3] + values[1] * (1 - values[3].a) +
    	values[7] + values[5] * (1 - values[7].a) + values[6] + values[4] * (1 - values[6].a)) * 0.25f;
    
	sourceResultNegY[DTid] =
	    (values[0] + values[2] * (1 - values[0].a) + values[1] + values[3] * (1 - values[1].a) +
    	values[5] + values[7] * (1 - values[5].a) + values[4] + values[6] * (1 - values[4].a)) * 0.25f;
    
	sourceResultPosZ[DTid] =
	    (values[1] + values[0] * (1 - values[1].a) + values[3] + values[2] * (1 - values[3].a) +
    	values[5] + values[4] * (1 - values[5].a) + values[7] + values[6] * (1 - values[7].a)) * 0.25f;
    
	sourceResultNegZ[DTid] =
	    (values[0] + values[1] * (1 - values[0].a) + values[2] + values[3] * (1 - values[2].a) +
    	values[4] + values[5] * (1 - values[4].a) + values[6] + values[7] * (1 - values[6].a)) * 0.25f;
    
}