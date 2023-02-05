struct VertexShaderInputGBuffer
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD;
};

struct PixelShaderInputGBuffer
{
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD0;
	float3 worldPos : TEXCOORD1;
	float4 position : SV_POSITION;
};

cbuffer GPrepassCB : register(b0)
{
	float4x4 ViewProjection;
	float4x4 InverseViewProjection;
	float4  CameraPosition;
    float4 ScreenSize;
    float4 LightColor;
};

cbuffer perModelInstanceCB : register(b1)
{
	float4x4	World;
    float4		DiffuseColor;
};

//Texture2D<float4> Textures[] : register(t0);
//SamplerState SamplerLinear : register(s0);

PSInput VSMain(VSInput input)
{
    PSInput result;

	result.normal = mul(input.normal, (float3x3)World);
	result.tangent = mul(input.tangent, (float3x3)World);
	result.position = mul(float4(input.position.xyz, 1), World);
    result.worldPos = result.position.xyz;
	result.position = mul(result.position, ViewProjection);
	result.uv = input.uv;

    return result;
}

struct PSOutput
{
	float4 color : SV_Target0;
	float4 normal : SV_Target1;
	float4 worldpos : SV_Target2;
};

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;

	//TODO albedo texture support
    output.color = DiffuseColor;
    output.normal = float4(input.normal, 1.0f);
    output.worldpos = float4(input.worldPos, 1.0f);
    return output;
}
