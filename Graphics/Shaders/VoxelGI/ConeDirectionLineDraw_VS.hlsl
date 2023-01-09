#include "C:\Users\StRaToX\Documents\Visual Studio 2019\Projects\VoxelConeTracing\Graphics\Shaders\VoxelGI\ConeDirectionLineDraw_HF.hlsli"




VertexShaderOutputConeDirectionLineDraw main(VertexShaderInputConeDirectionLineDraw input)
{
	VertexShaderOutputConeDirectionLineDraw output;
	output.position = input.position;
	output.instance_id = input.instance_id;
	
	return output;
}
