
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\UWPVoxelConeTracing\Source\Graphics\Shaders\HF\VoxelConeTracingVoxelizationDebug_HF.hlsli"

cbuffer VoxelizationDebug : register(b0)
{
	float4x4 WorldVoxelCube;
	float4x4 ViewProjection;
	float WorldVoxelScale;
};

[maxvertexcount(36)]
void main(point GeometryShaderInputVoxelConeTracingVoxelizationDebug input[1], inout TriangleStream<PixelShaderInputVoxelConeTracingVoxelizationDebug> OutputStream)
{
	PixelShaderInputVoxelConeTracingVoxelizationDebug output[36];
	for (int i = 0; i < 36; i++)
	{
		output[i] = (PixelShaderInputVoxelConeTracingVoxelizationDebug) 0;
		output[i].color = input[0].color;
	}
	input[0].position.rgb -= float3(0.5f, 0.5f, 0.5f);

	float4 v1 = mul((input[0].position + float4(-0.5, 0.5, 0.5, 0)), WorldVoxelCube);
	float4 v2 = mul((input[0].position + float4(0.5, 0.5, 0.5, 0)), WorldVoxelCube);
	float4 v3 = mul((input[0].position + float4(-0.5, -0.5, 0.5, 0)), WorldVoxelCube);
	float4 v4 = mul((input[0].position + float4(0.5, -0.5, 0.5, 0)), WorldVoxelCube);
	float4 v5 = mul((input[0].position + float4(-0.5, 0.5, -0.5, 0)), WorldVoxelCube);
	float4 v6 = mul((input[0].position + float4(0.5, 0.5, -0.5, 0)), WorldVoxelCube);
	float4 v7 = mul((input[0].position + float4(-0.5, -0.5, -0.5, 0)), WorldVoxelCube);
	float4 v8 = mul((input[0].position + float4(0.5, -0.5, -0.5, 0)), WorldVoxelCube);
    
	v1 = mul(v1, ViewProjection);
	v2 = mul(v2, ViewProjection);
	v3 = mul(v3, ViewProjection);
	v4 = mul(v4, ViewProjection);
	v5 = mul(v5, ViewProjection);
	v6 = mul(v6, ViewProjection);
	v7 = mul(v7, ViewProjection);
	v8 = mul(v8, ViewProjection);
    
    // +Z
	output[0].pos = v1;
	OutputStream.Append(output[0]);
	output[1].pos = v3;
	OutputStream.Append(output[1]);
	output[2].pos = v4;
	OutputStream.Append(output[2]);
	OutputStream.RestartStrip();
	output[3].pos = v1;
	OutputStream.Append(output[3]);
	output[4].pos = v4;
	OutputStream.Append(output[4]);
	output[5].pos = v2;
	OutputStream.Append(output[5]);
	OutputStream.RestartStrip();

    // -Z
	output[6].pos = v6;
	OutputStream.Append(output[6]);
	output[7].pos = v8;
	OutputStream.Append(output[7]);
	output[8].pos = v7;
	OutputStream.Append(output[8]);
	OutputStream.RestartStrip();
	output[9].pos = v6;
	OutputStream.Append(output[9]);
	output[10].pos = v7;
	OutputStream.Append(output[10]);
	output[11].pos = v5;
	OutputStream.Append(output[11]);
	OutputStream.RestartStrip();

    // +X
	output[12].pos = v2;
	OutputStream.Append(output[12]);
	output[13].pos = v4;
	OutputStream.Append(output[13]);
	output[14].pos = v8;
	OutputStream.Append(output[14]);
	OutputStream.RestartStrip();
	output[15].pos = v2;
	OutputStream.Append(output[15]);
	output[16].pos = v8;
	OutputStream.Append(output[16]);
	output[17].pos = v6;
	OutputStream.Append(output[17]);
	OutputStream.RestartStrip();

    // -X
	output[18].pos = v5;
	OutputStream.Append(output[18]);
	output[19].pos = v7;
	OutputStream.Append(output[19]);
	output[20].pos = v3;
	OutputStream.Append(output[20]);
	OutputStream.RestartStrip();
	output[21].pos = v5;
	OutputStream.Append(output[21]);
	output[22].pos = v3;
	OutputStream.Append(output[22]);
	output[23].pos = v1;
	OutputStream.Append(output[23]);
	OutputStream.RestartStrip();

    // +Y
	output[24].pos = v5;
	OutputStream.Append(output[24]);
	output[25].pos = v1;
	OutputStream.Append(output[25]);
	output[26].pos = v2;
	OutputStream.Append(output[26]);
	OutputStream.RestartStrip();
	output[27].pos = v5;
	OutputStream.Append(output[27]);
	output[28].pos = v2;
	OutputStream.Append(output[28]);
	output[29].pos = v6;
	OutputStream.Append(output[29]);
	OutputStream.RestartStrip();

    // -Y
	output[30].pos = v3;
	OutputStream.Append(output[30]);
	output[31].pos = v7;
	OutputStream.Append(output[31]);
	output[32].pos = v8;
	OutputStream.Append(output[32]);
	OutputStream.RestartStrip();
	output[33].pos = v3;
	OutputStream.Append(output[33]);
	output[34].pos = v8;
	OutputStream.Append(output[34]);
	output[35].pos = v4;
	OutputStream.Append(output[35]);
	OutputStream.RestartStrip();
}