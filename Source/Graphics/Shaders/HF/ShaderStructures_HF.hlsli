#define PI 3.14159265359f
#define SH_COSINE_LOBE_C0 0.886226925f // sqrt(pi)/2
#define SH_COSINE_LOBE_C1 1.02332671f // sqrt(pi/3)
#define SH_C0 0.282094792f // 1 / 2sqrt(pi)
#define SH_C1 0.488602512f // sqrt(3/pi) / 2

#define LPV_DIM 32
#define LPV_DIM_HALF 16
#define LPV_DIM_INVERSE 0.03125f
#define LPV_SCALE 0.25f




struct ShaderStructureGPUDirectionalLight
{
	float4x4 view_projection;
	float4 inverse_direction_world_space; // Vector pointing towards the light
	float4 color;
	float intensity;
	float3 padding;
};

struct ShaderSTructureGPULightingData
{
	float2 shadow_texel_size;
	float2 padding;
};

struct ShaderStructureGPUCameraData
{
	float4x4 view_projection;
};


struct ShaderStructureGPUModelData
{
	float4x4 model;
	float4 diffuse_color;
};


static const float FLT_MAX = asfloat(0x7F7FFFFF);

float3 ReconstructWorldPosFromDepth(float2 uv, float depth, float4x4 invProj, float4x4 invView)
{
    float4 viewPos = mul(float4(uv.x, uv.y, depth, 1.0f), invProj);
    viewPos = viewPos / viewPos.w;
	return mul(viewPos, invView).xyz;
}

struct Payload
{
    bool skipShading;
    float rayHitT;
};

struct ShadowPayload
{
    bool isHit;
};

float4 DirToSH(float3 direction)
{
    return float4(SH_C0, -SH_C1 * direction.y, SH_C1 * direction.z, -SH_C1 * direction.x);
}

float4 DirCosLobeToSH(float3 direction)
{
    return float4(SH_COSINE_LOBE_C0, -SH_COSINE_LOBE_C1 * direction.y, SH_COSINE_LOBE_C1 * direction.z, -SH_COSINE_LOBE_C1 * direction.x);
}