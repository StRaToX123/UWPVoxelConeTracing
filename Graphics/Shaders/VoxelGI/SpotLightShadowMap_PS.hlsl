#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\SpotLightShadowMap_HF.hlsli"
#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\SpotLight_HF.hlsli"

Texture2D<float4> spot_light_depth_buffer : register(t3);
SamplerState samp : register(s0);
ConstantBuffer<ShaderStructureGPUSpotLight> spot_light_buffer : register(b3);

float main(VertexShaderOutputSpotLightShadowMap input) : SV_Target
{
	float4 color;
	float2 projectTexCoord;
	float depthValue;
	float vertexLightDepthValue;
	float lightIntensity;

	// Set the default output color to be black (shadow).
	color = 0.0f;
	// Calculate the projected texture coordinates.
	// we have to perform the perspective divide ourselves, and in order to remap the value from NDC to a texture coordinate
	// we have to divide by 2 and add 0.5f, which puts the values in a from -1 +1 range to a 0 1 range, we also have to 
	// flip the y axis since we will be using these coordinates to look up a texture, where the y axis grows downwards
	projectTexCoord.x = input.lightViewPosition.x / input.lightViewPosition.w / 2.0f + 0.5f;
	projectTexCoord.y = -input.lightViewPosition.y / input.lightViewPosition.w / 2.0f + 0.5f;
	// Determine if the projected coordinates are in the 0 to 1 range.  If so then this pixel is in the view of the light.
	if ((saturate(projectTexCoord.x) == projectTexCoord.x) && (saturate(projectTexCoord.y) == projectTexCoord.y))
	{
		// Sample the shadow map depth value from the depth texture using the sampler at the projected texture coordinate location.
		depthValue = spot_light_depth_buffer.Sample(samp, projectTexCoord);
		// Calculate the depth of the light.
		vertexLightDepthValue = input.lightViewPosition.z / input.lightViewPosition.w;
		// Compare the depth of the shadow map value and the depth of the light to determine whether to shadow or to light this pixel.
		// If the light is in front of the object then light the pixel, if not then shadow this pixel since an object (occluder) is casting a shadow on it.
		if (vertexLightDepthValue < depthValue)
		{
		    // Calculate the amount of light on this pixel.
			lightIntensity = saturate(dot(input.normal, input.lightPos));

			// If this pixel is illuminated then set it to pure white (non-shadow).
			if (lightIntensity > 0.0f)
			{
				color = 1.0f;
			}
		}
	}
	
	return color;
}