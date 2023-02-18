#include "c:\users\stratox\documents\visual studio 2019\projects\uwpvoxelconetracing\Source\Graphics\Shaders\HF\ShaderStructures_HF.hlsli"
#include "c:\users\stratox\documents\visual studio 2019\projects\uwpvoxelconetracing\Source\Graphics\Shaders\ShaderGlobalsCPUGPU.hlsli"

Texture3D<float4> sourcePosX : register(t0);
Texture3D<float4> sourceNegX : register(t1);
Texture3D<float4> sourcePosY : register(t2);
Texture3D<float4> sourceNegY : register(t3);
Texture3D<float4> sourcePosZ : register(t4);
Texture3D<float4> sourceNegZ : register(t5);

RWTexture3D<float4> resultPosX : register(u0);
RWTexture3D<float4> resultNegX : register(u1);
RWTexture3D<float4> resultPosY : register(u2);
RWTexture3D<float4> resultNegY : register(u3);
RWTexture3D<float4> resultPosZ : register(u4);
RWTexture3D<float4> resultNegZ : register(u5);

ConstantBuffer<ShaderStructureGPUAnisotropicMipGenerationData> mip_mapping_data : register(b0);

static const int3 anisotropic_offsets[8] =
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

[numthreads(DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION, DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION, DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION)]
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
		values[i] = sourcePosX.Load(int4(sourcePos + anisotropic_offsets[i], 0));
	}
		
	resultPosX[DTid] =
        (values[4] + values[0] * (1 - values[4].a) + values[5] + values[1] * (1 - values[5].a) +
        values[6] + values[2] * (1 - values[6].a) + values[7] + values[3] * (1 - values[7].a)) * 0.25f;
    
    [unroll]
	for (int i = 0; i < 8; i++)
	{
		values[i] = sourceNegX.Load(int4(sourcePos + anisotropic_offsets[i], 0));

	}
	
	resultNegX[DTid] =
        (values[0] + values[4] * (1 - values[0].a) + values[1] + values[5] * (1 - values[1].a) +
		values[2] + values[6] * (1 - values[2].a) + values[3] + values[7] * (1 - values[3].a)) * 0.25f;
    
    [unroll]
	for (int i = 0; i < 8; i++)
	{
		values[i] = sourcePosY.Load(int4(sourcePos + anisotropic_offsets[i], 0));

	}
	resultPosY[DTid] =
	    (values[2] + values[0] * (1 - values[2].a) + values[3] + values[1] * (1 - values[3].a) +
    	values[7] + values[5] * (1 - values[7].a) + values[6] + values[4] * (1 - values[6].a)) * 0.25f;
    
    [unroll]
	for (int i = 0; i < 8; i++)
	{
		values[i] = sourceNegY.Load(int4(sourcePos + anisotropic_offsets[i], 0));

	}
	
	resultNegY[DTid] =
	    (values[0] + values[2] * (1 - values[0].a) + values[1] + values[3] * (1 - values[1].a) +
    	values[5] + values[7] * (1 - values[5].a) + values[4] + values[6] * (1 - values[4].a)) * 0.25f;
    
    [unroll]
	for (int i = 0; i < 8; i++)
	{
		values[i] = sourcePosZ.Load(int4(sourcePos + anisotropic_offsets[i], 0));

	}
	
	resultPosZ[DTid] =
	    (values[1] + values[0] * (1 - values[1].a) + values[3] + values[2] * (1 - values[3].a) +
    	values[5] + values[4] * (1 - values[5].a) + values[7] + values[6] * (1 - values[7].a)) * 0.25f;
    
    [unroll]
	for (int i = 0; i < 8; i++)
	{
		values[i] = sourceNegZ.Load(int4(sourcePos + anisotropic_offsets[i], 0));
	}
		
	resultNegZ[DTid] =
	    (values[0] + values[1] * (1 - values[0].a) + values[2] + values[3] * (1 - values[2].a) +
    	values[4] + values[5] * (1 - values[4].a) + values[6] + values[7] * (1 - values[6].a)) * 0.25f;
    
}