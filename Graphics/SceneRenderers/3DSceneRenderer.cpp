#include "Graphics/SceneRenderers/3DSceneRenderer.h"




// Indices into the application state map.
Platform::String^ AngleKey = "Angle";
Platform::String^ TrackingKey = "Tracking";

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
SceneRenderer3D::SceneRenderer3D(const std::shared_ptr<DeviceResources>& deviceResources, Camera& camera, UINT voxelGridResolution) :
	device_resources(deviceResources),
	fence_command_list_direct_progress_latest_unused_value(1),
	fence_command_list_compute_progress_latest_unused_value(1),
	fence_command_list_copy_normal_priority_progress_latest_unused_value(1),
	fence_command_list_copy_high_priority_progress_latest_unused_value(1),
	event_wait_for_gpu(NULL),
	command_allocator_copy_normal_priority_already_reset(true),
	command_list_copy_normal_priority_requires_reset(false),
	command_allocator_copy_high_priority_already_reset(true),
	command_list_copy_high_priority_requires_reset(false),
	previous_frame_index(c_frame_count - 1),
	descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors(0),
	descriptor_heap_cbv_srv_uav_all_other_resources_number_of_filled_descriptors(0),
	temp(0)
{
	voxel_grid_data.UpdateRes(voxelGridResolution);
	voxel_grid_data.UpdateGirdExtent(voxel_grid_data.grid_extent);
	voxel_grid_data.UpdateNumOfCones(voxel_grid_data.num_cones);
	radiance_texture_3D_mip_count_copy = voxel_grid_data.mip_count;


	LoadState();

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources(camera);
}

SceneRenderer3D::~SceneRenderer3D()
{
	if (event_wait_for_gpu != NULL)
	{
		CloseHandle(event_wait_for_gpu);
	}
}

void SceneRenderer3D::CreateDeviceDependentResources()
{
	auto d3dDevice = device_resources->GetD3DDevice();

	CD3DX12_DESCRIPTOR_RANGE rangeCBVviewProjectionMatrix;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVspotLightData;
	CD3DX12_DESCRIPTOR_RANGE rangeSRVspotLighShadowMapAndDepthBuffer;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVVoxelGridData;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVRequiredVoxelDebugDataForDrawInstanced;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVRequiredConeDebugDataForDrawInstanced;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVVoxelDataStructuredBuffer;
	CD3DX12_DESCRIPTOR_RANGE rangeSRVradianceTexture3DMips;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVradianceTexture3DMips;
	CD3DX12_DESCRIPTOR_RANGE rangeSRVTestTexture;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVIndirectCommandVoxelDebug;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVIndirectCommandConeDebug;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVTransformMatrix;
	
	rangeCBVviewProjectionMatrix.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);
	rangeCBVspotLightData.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 3);
	rangeSRVspotLighShadowMapAndDepthBuffer.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 2);
	rangeCBVVoxelGridData.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	rangeUAVRequiredVoxelDebugDataForDrawInstanced.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
	rangeUAVRequiredConeDebugDataForDrawInstanced.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4);
	rangeUAVVoxelDataStructuredBuffer.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
	rangeSRVradianceTexture3DMips.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
	rangeUAVradianceTexture3DMips.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);
	rangeSRVTestTexture.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	rangeUAVIndirectCommandVoxelDebug.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
	rangeUAVIndirectCommandConeDebug.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 5);
	rangeCBVTransformMatrix.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 5);

	CD3DX12_ROOT_PARAMETER parameterRootConstants; // contains transformMatrixBufferIndex, transformMatrixBufferInternalIndex
	CD3DX12_ROOT_PARAMETER parameterGeometryVisibleDescriptorTableViewProjectionMatrixBuffer;
	CD3DX12_ROOT_PARAMETER parameterGeometryVisibleDescriptorTableSpotLightDataBuffer;
	CD3DX12_ROOT_PARAMETER parameterGeometryVisibleDescriptorTableSpotLightShadowMapAndDepthBuffer;
	CD3DX12_ROOT_PARAMETER parameterGeometryVisibleDescriptorTableVoxelGridDataBuffer;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableRequiredVoxelDebugDataForDrawInstanced;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableRequiredConeDebugDataForDrawInstanced;
	CD3DX12_ROOT_PARAMETER parameterAllVisibleDescriptorTableVoxelDataStructuredBufferUAV;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableRadianceTexture3DMipSRVs;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableRadianceTexture3DMipUAVs;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleRootConstantsRadianceTexture3DGenerateMipChain; // contains the indexes for input mip and output mip for the radiance_texture_3D generate mip chain compute shader 
	CD3DX12_ROOT_PARAMETER parameterPixelVisibleDescriptorTableTestTexture; // test texture and spot light shadow map
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableIndirectCommandVoxelDebug;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableIndirectCommandConeDebug;
	CD3DX12_ROOT_PARAMETER parameterGeometryVisibleDescriptorTableTransformMatrixBuffers;

	parameterRootConstants.InitAsConstants(2, 0, 0, D3D12_SHADER_VISIBILITY_ALL);
	parameterGeometryVisibleDescriptorTableViewProjectionMatrixBuffer.InitAsDescriptorTable(1, &rangeCBVviewProjectionMatrix, D3D12_SHADER_VISIBILITY_ALL);
	parameterGeometryVisibleDescriptorTableSpotLightDataBuffer.InitAsDescriptorTable(1, &rangeCBVspotLightData, D3D12_SHADER_VISIBILITY_ALL);
	parameterGeometryVisibleDescriptorTableSpotLightShadowMapAndDepthBuffer.InitAsDescriptorTable(1, &rangeSRVspotLighShadowMapAndDepthBuffer, D3D12_SHADER_VISIBILITY_ALL);
	parameterGeometryVisibleDescriptorTableVoxelGridDataBuffer.InitAsDescriptorTable(1, &rangeCBVVoxelGridData, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableRequiredVoxelDebugDataForDrawInstanced.InitAsDescriptorTable(1, &rangeUAVRequiredVoxelDebugDataForDrawInstanced, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableRequiredConeDebugDataForDrawInstanced.InitAsDescriptorTable(1, &rangeUAVRequiredConeDebugDataForDrawInstanced, D3D12_SHADER_VISIBILITY_ALL);
	parameterAllVisibleDescriptorTableVoxelDataStructuredBufferUAV.InitAsDescriptorTable(1, &rangeUAVVoxelDataStructuredBuffer, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableRadianceTexture3DMipSRVs.InitAsDescriptorTable(1, &rangeSRVradianceTexture3DMips, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableRadianceTexture3DMipUAVs.InitAsDescriptorTable(1, &rangeUAVradianceTexture3DMips, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleRootConstantsRadianceTexture3DGenerateMipChain.InitAsConstants(sizeof(ShaderStructureCPUGenerate3DMipChainData) / sizeof(UINT32), 4, 0, D3D12_SHADER_VISIBILITY_ALL);
	parameterPixelVisibleDescriptorTableTestTexture.InitAsDescriptorTable(1, &rangeSRVTestTexture, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableIndirectCommandVoxelDebug.InitAsDescriptorTable(1, &rangeUAVIndirectCommandVoxelDebug, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableIndirectCommandConeDebug.InitAsDescriptorTable(1, &rangeUAVIndirectCommandConeDebug, D3D12_SHADER_VISIBILITY_ALL);
	parameterGeometryVisibleDescriptorTableTransformMatrixBuffers.InitAsDescriptorTable(1, &rangeCBVTransformMatrix, D3D12_SHADER_VISIBILITY_ALL);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // Only the input assembler stage needs access to the constant buffer.
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
	CD3DX12_ROOT_PARAMETER parameterArray[15] = { parameterRootConstants,
		parameterGeometryVisibleDescriptorTableViewProjectionMatrixBuffer,
		parameterGeometryVisibleDescriptorTableSpotLightDataBuffer,
		parameterGeometryVisibleDescriptorTableSpotLightShadowMapAndDepthBuffer,
		parameterGeometryVisibleDescriptorTableVoxelGridDataBuffer,
		parameterComputeVisibleDescriptorTableRequiredVoxelDebugDataForDrawInstanced,
		parameterComputeVisibleDescriptorTableRequiredConeDebugDataForDrawInstanced,
		parameterAllVisibleDescriptorTableVoxelDataStructuredBufferUAV,
		parameterComputeVisibleDescriptorTableRadianceTexture3DMipSRVs,
		parameterComputeVisibleDescriptorTableRadianceTexture3DMipUAVs,
		parameterComputeVisibleRootConstantsRadianceTexture3DGenerateMipChain,
		parameterPixelVisibleDescriptorTableTestTexture,
		parameterComputeVisibleDescriptorTableIndirectCommandVoxelDebug,
		parameterComputeVisibleDescriptorTableIndirectCommandConeDebug,
		parameterGeometryVisibleDescriptorTableTransformMatrixBuffers
	};

	D3D12_STATIC_SAMPLER_DESC staticSamplers[2];
	// Create a texture sampler
	D3D12_STATIC_SAMPLER_DESC staticSamplerDesc = {};
	staticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	staticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplerDesc.MipLODBias = 0;
	staticSamplerDesc.MaxAnisotropy = 0;
	staticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR::D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	staticSamplerDesc.MinLOD = 0.0f;
	staticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplerDesc.ShaderRegister = 0;
	staticSamplerDesc.RegisterSpace = 0;
	staticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	staticSamplers[0] = staticSamplerDesc;

	// Create a linear clamp texture sampler used for mip map generation
	staticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplerDesc.ShaderRegister = 1;

	staticSamplers[1] = staticSamplerDesc;

	descRootSignature.Init(_countof(parameterArray), parameterArray, 2, staticSamplers, rootSignatureFlags);

	ComPtr<ID3DBlob> pSignature;
	ComPtr<ID3DBlob> pError;
	D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf());
	if (pError.Get() != NULL)
	{
		DisplayDebugMessage("@@@@@@@@@@@@@@@@@@@@\n%s\n@@@@@@@@@@@@@@@@@@@@@\n", (char*)pError->GetBufferPointer());
		throw ref new FailureException();
	}

	ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
	root_signature->SetName(L"SceneRenderer3D root_signature");

	wstring wStringInstallPath(Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data());
	string standardStringInstallPath(wStringInstallPath.begin(), wStringInstallPath.end());

	#pragma region Unlit Pipeline
	// Create a default pipeline state that anybody can use
	// Create the pipeline state 
	char* pVertexShaderByteCode;
	int vertexShaderByteCodeLength;
	char* pPixelShaderByteCode;
	int pixelShaderByteCodeLength;
	LoadShaderByteCode((standardStringInstallPath + "\\Unlit_VS.cso").c_str(), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\Unlit_PS.cso").c_str(), pPixelShaderByteCode, pixelShaderByteCodeLength);


	D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
	state.InputLayout = { ShaderStructureCPUVertexPositionNormalTextureColor::input_elements, ShaderStructureCPUVertexPositionNormalTextureColor::input_element_count };
	state.pRootSignature = root_signature.Get();
	state.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderByteCode, vertexShaderByteCodeLength);
	state.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderByteCode, pixelShaderByteCodeLength);
	state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	state.SampleMask = UINT_MAX;
	state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	state.NumRenderTargets = 1;
	state.RTVFormats[0] = device_resources->GetBackBufferFormat();
	state.DSVFormat = device_resources->GetDepthBufferFormat();
	state.SampleDesc.Count = 1;

	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipeline_state_unlit)));
	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
	#pragma endregion

	#pragma region Voxelizer Pipeline State
	// Load the shaders
	
	char* pGeometryShaderByteCode;
	int geometryShaderByteCodeLength;
	
	LoadShaderByteCode((standardStringInstallPath + "\\Voxelizer_VS.cso").c_str(), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\Voxelizer_GS.cso").c_str(), pGeometryShaderByteCode, geometryShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\Voxelizer_PS.cso").c_str(), pPixelShaderByteCode, pixelShaderByteCodeLength);

	// Create the pipeline state once the shaders are loaded.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC voxelizerState = {};
	voxelizerState.InputLayout = { ShaderStructureCPUVertexPositionNormalTextureColor::input_elements, ShaderStructureCPUVertexPositionNormalTextureColor::input_element_count };
	voxelizerState.pRootSignature = root_signature.Get();
	voxelizerState.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderByteCode, vertexShaderByteCodeLength);
	voxelizerState.GS = CD3DX12_SHADER_BYTECODE(pGeometryShaderByteCode, geometryShaderByteCodeLength);
	voxelizerState.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderByteCode, pixelShaderByteCodeLength);
	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D12_FILL_MODE::D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_NONE;
	rasterizerDesc.FrontCounterClockwise = true;
	rasterizerDesc.DepthBias = 0;
	rasterizerDesc.DepthBiasClamp = 0;
	rasterizerDesc.SlopeScaledDepthBias = 0;
	rasterizerDesc.DepthClipEnable = false;
	rasterizerDesc.MultisampleEnable = true;
	rasterizerDesc.AntialiasedLineEnable = false;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE::D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	rasterizerDesc.ForcedSampleCount = 8;
	voxelizerState.RasterizerState = rasterizerDesc;
	voxelizerState.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.StencilEnable = false;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	voxelizerState.DepthStencilState = depthStencilDesc;
	voxelizerState.SampleMask = UINT_MAX;
	voxelizerState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	voxelizerState.NumRenderTargets = 0;
	voxelizerState.DSVFormat = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
	voxelizerState.SampleDesc.Count = 1;

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateGraphicsPipelineState(&voxelizerState, IID_PPV_ARGS(&pipeline_state_voxelizer)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pVertexShaderByteCode;
	delete[] pGeometryShaderByteCode;
	delete[] pPixelShaderByteCode;
	#pragma endregion

	#pragma region Voxel Radiance Smooth Temporal Copy Clear Pipeline State
	// Load the geometry shader
	char* pComputeShaderByteCode;
	int computeShaderByteCodeLength;
	LoadShaderByteCode((standardStringInstallPath + "\\RadianceTemporalCopyClear_CS.cso").c_str(), pComputeShaderByteCode, computeShaderByteCodeLength);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {};
	computePipelineStateDesc.pRootSignature = root_signature.Get();
	computePipelineStateDesc.CS = CD3DX12_SHADER_BYTECODE(pComputeShaderByteCode, computeShaderByteCodeLength);
	computePipelineStateDesc.NodeMask = 0;
	computePipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	ThrowIfFailed(d3dDevice->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&pipeline_state_radiance_temporal_clear)));


	// Shader data can be deleted once the pipeline state is created.
	delete[] pComputeShaderByteCode;
	#pragma endregion

	#pragma region Voxel Radiance Texture 3D Mip Chain Generation Pipeline State
	// Load the geometry shader
	LoadShaderByteCode((standardStringInstallPath + "\\RadianceGenerate3DMipChain_CS.cso").c_str(), pComputeShaderByteCode, computeShaderByteCodeLength);
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateTexture3DMipChainGenerationDesc = {};
	computePipelineStateTexture3DMipChainGenerationDesc.pRootSignature = root_signature.Get();
	computePipelineStateTexture3DMipChainGenerationDesc.CS = CD3DX12_SHADER_BYTECODE(pComputeShaderByteCode, computeShaderByteCodeLength);
	computePipelineStateTexture3DMipChainGenerationDesc.NodeMask = 0;
	computePipelineStateTexture3DMipChainGenerationDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(d3dDevice->CreateComputePipelineState(&computePipelineStateTexture3DMipChainGenerationDesc, IID_PPV_ARGS(&pipeline_state_radiance_mip_chain_generation)));
	// Shader data can be deleted once the pipeline state is created.
	delete[] pComputeShaderByteCode;
	#pragma endregion

	#pragma region Voxel Debug Visualization Pipeline State
	// Load the vertex shader
	LoadShaderByteCode((standardStringInstallPath + "\\VoxelDebugDraw_VS.cso").c_str(), pVertexShaderByteCode, vertexShaderByteCodeLength);
	// Load the pixel shader
	LoadShaderByteCode((standardStringInstallPath + "\\VoxelDebugDraw_PS.cso").c_str(), pPixelShaderByteCode, pixelShaderByteCodeLength);


	// Create the pipeline state once the shaders are loaded.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC voxelDebugVisualizationPiepelineStateDesc = {};
	voxelDebugVisualizationPiepelineStateDesc.InputLayout = { ShaderStructureCPUVertexPositionNormalTextureColor::input_elements, ShaderStructureCPUVertexPositionNormalTextureColor::input_element_count };
	voxelDebugVisualizationPiepelineStateDesc.pRootSignature = root_signature.Get();
	voxelDebugVisualizationPiepelineStateDesc.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderByteCode, vertexShaderByteCodeLength);
	voxelDebugVisualizationPiepelineStateDesc.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderByteCode, pixelShaderByteCodeLength);
	voxelDebugVisualizationPiepelineStateDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	voxelDebugVisualizationPiepelineStateDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	voxelDebugVisualizationPiepelineStateDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	voxelDebugVisualizationPiepelineStateDesc.SampleMask = UINT_MAX;
	voxelDebugVisualizationPiepelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	voxelDebugVisualizationPiepelineStateDesc.NumRenderTargets = 1;
	voxelDebugVisualizationPiepelineStateDesc.RTVFormats[0] = device_resources->GetBackBufferFormat();
	voxelDebugVisualizationPiepelineStateDesc.DSVFormat = device_resources->GetDepthBufferFormat();
	voxelDebugVisualizationPiepelineStateDesc.SampleDesc.Count = 1;


	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&voxelDebugVisualizationPiepelineStateDesc, IID_PPV_ARGS(&pipeline_voxel_debug_draw)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
	#pragma endregion

	#pragma region Spot Light Write Only Depth Pipeline State
	LoadShaderByteCode((standardStringInstallPath + "\\SpotLightWriteOnlyDepth_VS.cso").c_str(), pVertexShaderByteCode, vertexShaderByteCodeLength);
	//LoadShaderByteCode((standardStringInstallPath + "\\SpotLightWriteOnlyDepth_PS.cso").c_str(), pPixelShaderByteCode, pixelShaderByteCodeLength);
	D3D12_GRAPHICS_PIPELINE_STATE_DESC stateSpotLightWriteOnlyDepth = {};
	stateSpotLightWriteOnlyDepth.InputLayout = { ShaderStructureCPUVertexPositionNormalTextureColor::input_elements, ShaderStructureCPUVertexPositionNormalTextureColor::input_element_count };
	stateSpotLightWriteOnlyDepth.pRootSignature = root_signature.Get();
	stateSpotLightWriteOnlyDepth.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderByteCode, vertexShaderByteCodeLength);
	//stateSpotLightWriteOnlyDepth.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderByteCode, pixelShaderByteCodeLength);
	stateSpotLightWriteOnlyDepth.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	stateSpotLightWriteOnlyDepth.RasterizerState.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_FRONT;
	stateSpotLightWriteOnlyDepth.RasterizerState.SlopeScaledDepthBias = 0.1f;
	stateSpotLightWriteOnlyDepth.RasterizerState.DepthBiasClamp = 0.1f;
	stateSpotLightWriteOnlyDepth.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	stateSpotLightWriteOnlyDepth.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	stateSpotLightWriteOnlyDepth.SampleMask = UINT_MAX;
	stateSpotLightWriteOnlyDepth.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	stateSpotLightWriteOnlyDepth.NumRenderTargets = 0;
	//stateSpotLightWriteOnlyDepth.RTVFormats[0] = DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
	stateSpotLightWriteOnlyDepth.DSVFormat = device_resources->GetDepthBufferFormat();
	stateSpotLightWriteOnlyDepth.SampleDesc.Count = 1;

	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&stateSpotLightWriteOnlyDepth, IID_PPV_ARGS(&pipeline_state_spot_light_write_only_depth)));

	delete[] pVertexShaderByteCode;
	#pragma endregion

	#pragma region Spot Light Shadow Pass Pipeline State
	LoadShaderByteCode((standardStringInstallPath + "\\SpotLightShadowMap_VS.cso").c_str(), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\SpotLightShadowMap_PS.cso").c_str(), pPixelShaderByteCode, pixelShaderByteCodeLength);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC stateSpotLightShadowPass = {};
	stateSpotLightShadowPass.InputLayout = { ShaderStructureCPUVertexPositionNormalTextureColor::input_elements, ShaderStructureCPUVertexPositionNormalTextureColor::input_element_count };
	stateSpotLightShadowPass.pRootSignature = root_signature.Get();
	stateSpotLightShadowPass.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderByteCode, vertexShaderByteCodeLength);
	stateSpotLightShadowPass.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderByteCode, pixelShaderByteCodeLength);
	stateSpotLightShadowPass.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	stateSpotLightShadowPass.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	stateSpotLightShadowPass.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	stateSpotLightShadowPass.SampleMask = UINT_MAX;
	stateSpotLightShadowPass.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	stateSpotLightShadowPass.NumRenderTargets = 1;
	stateSpotLightShadowPass.RTVFormats[0] = DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
	stateSpotLightShadowPass.DSVFormat = device_resources->GetDepthBufferFormat();
	stateSpotLightShadowPass.SampleDesc.Count = 1;

	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&stateSpotLightShadowPass, IID_PPV_ARGS(&pipeline_state_spot_light_shadow_pass)));

	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
	#pragma endregion

	#pragma region Full Screen Texture Visualization Pipeline Pass
	LoadShaderByteCode((standardStringInstallPath + "\\FullScreenTextureVisualization_VS.cso").c_str(), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\FullScreenTextureVisualization_PS.cso").c_str(), pPixelShaderByteCode, pixelShaderByteCodeLength);


	D3D12_GRAPHICS_PIPELINE_STATE_DESC stateFullScreenTextureVisualization = {};
	stateFullScreenTextureVisualization.InputLayout = { ShaderStructureCPUVertexPositionNormalTextureColor::input_elements, ShaderStructureCPUVertexPositionNormalTextureColor::input_element_count };
	stateFullScreenTextureVisualization.pRootSignature = root_signature.Get();
	stateFullScreenTextureVisualization.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderByteCode, vertexShaderByteCodeLength);
	stateFullScreenTextureVisualization.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderByteCode, pixelShaderByteCodeLength);
	stateFullScreenTextureVisualization.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	stateFullScreenTextureVisualization.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	stateFullScreenTextureVisualization.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	stateFullScreenTextureVisualization.SampleMask = UINT_MAX;
	stateFullScreenTextureVisualization.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	stateFullScreenTextureVisualization.NumRenderTargets = 1;
	stateFullScreenTextureVisualization.RTVFormats[0] = device_resources->GetBackBufferFormat();
	stateFullScreenTextureVisualization.DSVFormat = device_resources->GetDepthBufferFormat();
	stateFullScreenTextureVisualization.SampleDesc.Count = 1;

	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&stateFullScreenTextureVisualization, IID_PPV_ARGS(&pipeline_state_full_screen_texture_visualization)));

	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
	#pragma endregion

	#pragma region Final Gather Pipeline State
	LoadShaderByteCode((standardStringInstallPath + "\\FinalGather_VS.cso").c_str(), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\FinalGather_PS.cso").c_str(), pPixelShaderByteCode, pixelShaderByteCodeLength);


	D3D12_GRAPHICS_PIPELINE_STATE_DESC stateFinalGather = {};
	stateFinalGather.InputLayout = { ShaderStructureCPUVertexPositionNormalTextureColor::input_elements, ShaderStructureCPUVertexPositionNormalTextureColor::input_element_count };
	stateFinalGather.pRootSignature = root_signature.Get();
	stateFinalGather.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderByteCode, vertexShaderByteCodeLength);
	stateFinalGather.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderByteCode, pixelShaderByteCodeLength);
	stateFinalGather.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	stateFinalGather.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	stateFinalGather.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	stateFinalGather.SampleMask = UINT_MAX;
	stateFinalGather.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	stateFinalGather.NumRenderTargets = 1;
	stateFinalGather.RTVFormats[0] = device_resources->GetBackBufferFormat();
	stateFinalGather.DSVFormat = device_resources->GetDepthBufferFormat();
	stateFinalGather.SampleDesc.Count = 1;

	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&stateFinalGather, IID_PPV_ARGS(&pipeline_state_final_gather)));

	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
	#pragma endregion

	#pragma region Final Gather Copy Pipeline State
	LoadShaderByteCode((standardStringInstallPath + "\\FinalGatherCopy_VS.cso").c_str(), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\FinalGatherCopy_PS.cso").c_str(), pPixelShaderByteCode, pixelShaderByteCodeLength);


	D3D12_GRAPHICS_PIPELINE_STATE_DESC stateFinalGatherCopy = {};
	stateFinalGatherCopy.InputLayout = { ShaderStructureCPUVertexPositionNormalTextureColor::input_elements, ShaderStructureCPUVertexPositionNormalTextureColor::input_element_count };
	stateFinalGatherCopy.pRootSignature = root_signature.Get();
	stateFinalGatherCopy.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderByteCode, vertexShaderByteCodeLength);
	stateFinalGatherCopy.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderByteCode, pixelShaderByteCodeLength);
	stateFinalGatherCopy.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	stateFinalGatherCopy.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	stateFinalGatherCopy.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	stateFinalGatherCopy.SampleMask = UINT_MAX;
	stateFinalGatherCopy.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	stateFinalGatherCopy.NumRenderTargets = 1;
	stateFinalGatherCopy.RTVFormats[0] = device_resources->GetBackBufferFormat();
	stateFinalGatherCopy.DSVFormat = device_resources->GetDepthBufferFormat();
	stateFinalGatherCopy.SampleDesc.Count = 1;

	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&stateFinalGatherCopy, IID_PPV_ARGS(&pipeline_state_final_gather_copy)));

	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
	#pragma endregion

	#pragma region Voxel Debug Draw Compute Pipeline State
	// Load the geometry shader
	LoadShaderByteCode((standardStringInstallPath + "\\VoxelDebugDraw_CS.cso").c_str(), pComputeShaderByteCode, computeShaderByteCodeLength);
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateVoxelDebugDrawDesc = {};
	computePipelineStateVoxelDebugDrawDesc.pRootSignature = root_signature.Get();
	computePipelineStateVoxelDebugDrawDesc.CS = CD3DX12_SHADER_BYTECODE(pComputeShaderByteCode, computeShaderByteCodeLength);
	computePipelineStateVoxelDebugDrawDesc.NodeMask = 0;
	computePipelineStateVoxelDebugDrawDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(d3dDevice->CreateComputePipelineState(&computePipelineStateVoxelDebugDrawDesc, IID_PPV_ARGS(&pipeline_state_voxel_debug_draw_compute)));
	// Shader data can be deleted once the pipeline state is created.
	delete[] pComputeShaderByteCode;
	#pragma endregion

	#pragma region Cone Direction Debug Line Draw Pipeline State
	LoadShaderByteCode((standardStringInstallPath + "\\ConeDirectionLineDraw_VS.cso").c_str(), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\ConeDirectionLineDraw_GS.cso").c_str(), pGeometryShaderByteCode, geometryShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\ConeDirectionLineDraw_PS.cso").c_str(), pPixelShaderByteCode, pixelShaderByteCodeLength);


	D3D12_GRAPHICS_PIPELINE_STATE_DESC stateConeDirectionLineDraw = {};
	stateConeDirectionLineDraw.InputLayout = { ShaderStructureCPUVertexPositionNormalTextureColor::input_elements, ShaderStructureCPUVertexPositionNormalTextureColor::input_element_count };
	stateConeDirectionLineDraw.pRootSignature = root_signature.Get();
	stateConeDirectionLineDraw.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderByteCode, vertexShaderByteCodeLength);
	stateConeDirectionLineDraw.GS = CD3DX12_SHADER_BYTECODE(pGeometryShaderByteCode, geometryShaderByteCodeLength);
	stateConeDirectionLineDraw.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderByteCode, pixelShaderByteCodeLength);
	stateConeDirectionLineDraw.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	stateConeDirectionLineDraw.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	stateConeDirectionLineDraw.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	stateConeDirectionLineDraw.SampleMask = UINT_MAX;
	stateConeDirectionLineDraw.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	stateConeDirectionLineDraw.NumRenderTargets = 1;
	stateConeDirectionLineDraw.RTVFormats[0] = device_resources->GetBackBufferFormat();
	stateConeDirectionLineDraw.DSVFormat = device_resources->GetDepthBufferFormat();
	stateConeDirectionLineDraw.SampleDesc.Count = 1;

	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&stateConeDirectionLineDraw, IID_PPV_ARGS(&pipeline_state_cone_direction_debug_line_draw)));
	delete[] pVertexShaderByteCode;
	delete[] pGeometryShaderByteCode;
	delete[] pPixelShaderByteCode;
	#pragma endregion



	// Create direct command list
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, device_resources->GetCommandAllocatorDirect(), pipeline_state_unlit.Get(), IID_PPV_ARGS(&command_list_direct)));
	command_list_direct->SetName(L"SceneRenderer3D command_list_direct");
	ThrowIfFailed(command_list_direct->Close());
	// Create copy command lists.
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, device_resources->GetCommandAllocatorCopyNormalPriority(), pipeline_state_unlit.Get(), IID_PPV_ARGS(&command_list_copy_normal_priority)));
	command_list_copy_normal_priority->SetName(L"SceneRenderer3D command_list_copy_normal_priority");
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, device_resources->GetCommandAllocatorCopyHighPriority(), pipeline_state_unlit.Get(), IID_PPV_ARGS(&command_list_copy_high_priority)));
	command_list_copy_high_priority->SetName(L"SceneRenderer3D command_list_copy_high_priority");
	// Create compute command list.
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, device_resources->GetCommandAllocatorCompute(), pipeline_state_radiance_temporal_clear.Get(), IID_PPV_ARGS(&command_list_compute)));
	command_list_compute->SetName(L"SceneRenderer3D command_list_compute");
	command_list_compute->Close();
	// Create a fences to go along with the copy command lists.
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_direct_progress)));
	fence_command_list_direct_progress->SetName(L"SceneRenderer3D fence_command_list_direct_progress");
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_compute_progress)));
	fence_command_list_compute_progress->SetName(L"SceneRenderer3D fence_command_list_compute_progress");
	// This fence will be used to signal when a resource has been successfully uploaded to the gpu
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_copy_normal_priority_progress)));
	fence_command_list_copy_normal_priority_progress->SetName(L"SceneRenderer3D fence_command_list_copy_normal_priority_progress");
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_copy_high_priority_progress)));
	fence_command_list_copy_high_priority_progress->SetName(L"SceneRenderer3D fence_command_list_copy_high_priority_progress");
	for (int i = 0; i < c_frame_count; i++)
	{
		fence_per_frame_command_list_copy_high_priority_values[i] = 0;
	}

	// Create an event we can wait on 
	event_wait_for_gpu = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (event_wait_for_gpu == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	// Create the indirect command signature
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	// Voxelizer Show Debug View Indirect Command Signature
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof(IndirectCommand);

	ThrowIfFailed(d3dDevice->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&indirect_draw_command_signature)));
	indirect_draw_command_signature->SetName(L"SceneRenderer3D voxel_debug_command_signature");

	// Allocate a buffer that can be used to reset the UAV counters and initialize them to 0.
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indirect_draw_counter_reset_buffer)));

	UINT8* pMappedCounterReset = nullptr;
	ThrowIfFailed(indirect_draw_counter_reset_buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pMappedCounterReset)));
	std::ZeroMemory(pMappedCounterReset, sizeof(UINT));
	indirect_draw_counter_reset_buffer->Unmap(0, nullptr);

	// Create a descriptor heap for the voxelizer resources
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDescVoxelizer = {};
	descriptorHeapDescVoxelizer.NumDescriptors = 30;
	descriptorHeapDescVoxelizer.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDescVoxelizer.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&descriptorHeapDescVoxelizer, IID_PPV_ARGS(&descriptor_heap_cbv_srv_uav_voxelizer)));
	descriptor_heap_cbv_srv_uav_voxelizer->SetName(L"SceneRenderer3D descriptor_heap_cbv_srv_uav_voxelizer");

	// Create a descriptor heap for all the other resources
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDescAllOtherResources = {};
	descriptorHeapDescAllOtherResources.NumDescriptors = 20;
	descriptorHeapDescAllOtherResources.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDescAllOtherResources.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&descriptorHeapDescAllOtherResources, IID_PPV_ARGS(&descriptor_heap_cbv_srv_uav_all_other_resources)));
	descriptor_heap_cbv_srv_uav_voxelizer->SetName(L"SceneRenderer3D descriptor_heap_cbv_srv_uav_voxelizer");

	descriptor_cpu_handle_cbv_srv_uav_voxelizer.InitOffsetted(descriptor_heap_cbv_srv_uav_voxelizer->GetCPUDescriptorHandleForHeapStart(), 0);

	#pragma region Voxel Grid Data Constant Buffer
	// Create the camera view projection constant buffer
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(c_frame_count * c_aligned_shader_structure_cpu_voxel_grid_data),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&voxel_grid_data_constant_buffer)));

	voxel_grid_data_constant_buffer->SetName(L"SceneRenderer3D voxel_grid_data_constant_buffer");

	// Create constant buffer views to access the upload buffer.
	D3D12_GPU_VIRTUAL_ADDRESS voxelGridDataConstantBufferGPUAddress = voxel_grid_data_constant_buffer->GetGPUVirtualAddress();
	for (int i = 0; i < c_frame_count; i++)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.BufferLocation = voxelGridDataConstantBufferGPUAddress;
		desc.SizeInBytes = c_aligned_shader_structure_cpu_voxel_grid_data;
		d3dDevice->CreateConstantBufferView(&desc, descriptor_cpu_handle_cbv_srv_uav_voxelizer);

		voxelGridDataConstantBufferGPUAddress += desc.SizeInBytes;
		descriptor_cpu_handle_cbv_srv_uav_voxelizer.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
		descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors++;
	}

	// Map the constant buffers.
	// We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.
	ThrowIfFailed(voxel_grid_data_constant_buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&voxel_grid_data_constant_mapped_buffer)));
	for (int i = 0; i < c_frame_count; i++)
	{
		memcpy(voxel_grid_data_constant_mapped_buffer + (i * c_aligned_shader_structure_cpu_voxel_grid_data), &voxel_grid_data, sizeof(ShaderStructureCPUVoxelGridData));
	}

	voxel_grid_data_constant_buffer_frame_index_containing_most_updated_version = 0;
	#pragma endregion

	UpdateBuffers(true, true);
	ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_unlit.Get()));

	descriptor_cpu_handle_cbv_srv_uav_all_other_resources.InitOffsetted(descriptor_heap_cbv_srv_uav_all_other_resources->GetCPUDescriptorHandleForHeapStart(), 0);

	#pragma region Test Texture
	// Create and upload the test texture
	TexMetadata metadata;
	ScratchImage scratchImage;
	//ThrowIfFailed(LoadFromWICFile((wStringInstallPath + L"\\Assets\\woah.jpg").c_str(), WIC_FLAGS::WIC_FLAGS_NONE, &metadata, scratchImage));
	ThrowIfFailed(LoadFromWICFile((wStringInstallPath + L"\\Assets\\me.jpg").c_str(), WIC_FLAGS::WIC_FLAGS_NONE, &metadata, scratchImage));
	D3D12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata.format,
		static_cast<UINT64>(metadata.width),
		static_cast<UINT>(metadata.height),
		static_cast<UINT16>(metadata.arraySize));

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&test_texture)));

	std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
	const DirectX::Image* pImages = scratchImage.GetImages();
	for (int i = 0; i < scratchImage.GetImageCount(); i++)
	{
		auto& subresource = subresources[i];
		subresource.RowPitch = pImages[i].rowPitch;
		subresource.SlicePitch = pImages[i].slicePitch;
		subresource.pData = pImages[i].pixels;
	}

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(GetRequiredIntermediateSize(test_texture.Get(), 0, static_cast<uint32_t>(subresources.size()))),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&test_texture_upload)));


	UpdateSubresources(command_list_direct.Get(), test_texture.Get(), test_texture_upload.Get(), 0, 0, static_cast<uint32_t>(subresources.size()), subresources.data());
	CD3DX12_RESOURCE_BARRIER testTextureBarrier = CD3DX12_RESOURCE_BARRIER::Transition(test_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	command_list_direct.Get()->ResourceBarrier(1, &testTextureBarrier);

	// Create an SRV for the test_texture
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = metadata.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	device_resources->GetD3DDevice()->CreateShaderResourceView(test_texture.Get(), &srvDesc, descriptor_cpu_handle_cbv_srv_uav_all_other_resources);
	descriptor_cpu_handle_cbv_srv_uav_all_other_resources.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
	descriptor_heap_cbv_srv_uav_all_other_resources_number_of_filled_descriptors++;
	#pragma endregion

	#pragma region Voxel Debug Cube
	voxel_debug_cube.InitializeAsCube(2.0f);
	// Upload the vertex buffer to the GPU.
	const UINT voxelDebugCubeVertexBufferSize = voxel_debug_cube.vertices.size() * sizeof(ShaderStructureCPUVertexPositionNormalTextureColor);
	CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(voxelDebugCubeVertexBufferSize);
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&voxel_debug_cube.vertex_buffer)));

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&voxel_debug_cube.vertex_buffer_upload)));


	D3D12_SUBRESOURCE_DATA voxelDebugCubeSubSubresourceVertexData = {};
	voxelDebugCubeSubSubresourceVertexData.pData = reinterpret_cast<BYTE*>(&voxel_debug_cube.vertices[0]);
	voxelDebugCubeSubSubresourceVertexData.RowPitch = voxelDebugCubeVertexBufferSize;
	voxelDebugCubeSubSubresourceVertexData.SlicePitch = voxelDebugCubeSubSubresourceVertexData.RowPitch;

	UpdateSubresources(command_list_direct.Get(), voxel_debug_cube.vertex_buffer.Get(), voxel_debug_cube.vertex_buffer_upload.Get(), 0, 0, 1, &voxelDebugCubeSubSubresourceVertexData);

	command_list_direct.Get()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(voxel_debug_cube.vertex_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// Upload the index buffer to the GPU.
	const UINT indexBufferSize = sizeof(uint16_t) * voxel_debug_cube.indices.size();
	CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&indexBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&voxel_debug_cube.index_buffer)));

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&indexBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&voxel_debug_cube.index_buffer_upload)));

	D3D12_SUBRESOURCE_DATA voxelDebugCubeSubresourceIndexData = {};
	voxelDebugCubeSubresourceIndexData.pData = reinterpret_cast<BYTE*>(&voxel_debug_cube.indices[0]);
	voxelDebugCubeSubresourceIndexData.RowPitch = indexBufferSize;
	voxelDebugCubeSubresourceIndexData.SlicePitch = voxelDebugCubeSubresourceIndexData.RowPitch;

	UpdateSubresources(command_list_direct.Get(), voxel_debug_cube.index_buffer.Get(), voxel_debug_cube.index_buffer_upload.Get(), 0, 0, 1, &voxelDebugCubeSubresourceIndexData);

	command_list_direct.Get()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(voxel_debug_cube.index_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
	#pragma endregion

	#pragma region Cone Direction Debug Line
	cone_direction_debug_line.InitializeAsLine(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f));
	// Upload the vertex buffer to the GPU.
	const UINT coneDirectionDebugLineVertexBufferSize = cone_direction_debug_line.vertices.size() * sizeof(ShaderStructureCPUVertexPositionNormalTextureColor);
	CD3DX12_RESOURCE_DESC vertexBufferDescConeDirectionLine = CD3DX12_RESOURCE_DESC::Buffer(coneDirectionDebugLineVertexBufferSize);
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferDescConeDirectionLine,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&cone_direction_debug_line.vertex_buffer)));
	cone_direction_debug_line.vertex_buffer->SetName(L"SceneRenderer3D cone_direction_debug_line.vertex_buffer");

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferDescConeDirectionLine,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&cone_direction_debug_line.vertex_buffer_upload)));


	D3D12_SUBRESOURCE_DATA coneDirectionDebugLineSubSubresourceVertexData = {};
	coneDirectionDebugLineSubSubresourceVertexData.pData = reinterpret_cast<BYTE*>(&cone_direction_debug_line.vertices[0]);
	coneDirectionDebugLineSubSubresourceVertexData.RowPitch = voxelDebugCubeVertexBufferSize;
	coneDirectionDebugLineSubSubresourceVertexData.SlicePitch = coneDirectionDebugLineSubSubresourceVertexData.RowPitch;

	UpdateSubresources(command_list_direct.Get(), cone_direction_debug_line.vertex_buffer.Get(), cone_direction_debug_line.vertex_buffer_upload.Get(), 0, 0, 1, &coneDirectionDebugLineSubSubresourceVertexData);

	command_list_direct.Get()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(cone_direction_debug_line.vertex_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// Upload the index buffer to the GPU.
	const UINT indexBufferSizeConeDirectionLine = sizeof(uint16_t) * cone_direction_debug_line.indices.size();
	CD3DX12_RESOURCE_DESC indexBufferDescConeDirectionLine = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSizeConeDirectionLine);
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&indexBufferDescConeDirectionLine,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&cone_direction_debug_line.index_buffer)));

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&indexBufferDescConeDirectionLine,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&cone_direction_debug_line.index_buffer_upload)));

	D3D12_SUBRESOURCE_DATA coneDirectionDebugLineSubresourceIndexData = {};
	coneDirectionDebugLineSubresourceIndexData.pData = reinterpret_cast<BYTE*>(&cone_direction_debug_line.indices[0]);
	coneDirectionDebugLineSubresourceIndexData.RowPitch = indexBufferSize;
	coneDirectionDebugLineSubresourceIndexData.SlicePitch = coneDirectionDebugLineSubresourceIndexData.RowPitch;

	UpdateSubresources(command_list_direct.Get(), cone_direction_debug_line.index_buffer.Get(), cone_direction_debug_line.index_buffer_upload.Get(), 0, 0, 1, &coneDirectionDebugLineSubresourceIndexData);

	command_list_direct.Get()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(cone_direction_debug_line.index_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
	#pragma endregion

	#pragma region Indirect Command Buffer Voxel Debug
	// Create UAVs for each frame IndirectCommand
	IndirectCommand indirectCommand;
	indirectCommand.draw_indexed_arguments.IndexCountPerInstance = voxel_debug_cube.indices.size();
	indirectCommand.draw_indexed_arguments.StartIndexLocation = 0;
	indirectCommand.draw_indexed_arguments.StartInstanceLocation = 0;
	indirectCommand.draw_indexed_arguments.BaseVertexLocation = 0;

	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(AlignForUavCounter(sizeof(IndirectCommand)), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&indirect_command_buffer_voxel_debug)));
	NAME_D3D12_OBJECT(indirect_command_buffer_voxel_debug);

	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(AlignForUavCounter(sizeof(IndirectCommand))),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indirect_command_upload_buffer_voxel_debug)));
	NAME_D3D12_OBJECT(indirect_command_upload_buffer_voxel_debug);

	D3D12_SUBRESOURCE_DATA perFrameIndirectCommandSubresourceData;
	perFrameIndirectCommandSubresourceData.pData = &indirectCommand;
	perFrameIndirectCommandSubresourceData.RowPitch = sizeof(IndirectCommand);
	perFrameIndirectCommandSubresourceData.SlicePitch = perFrameIndirectCommandSubresourceData.RowPitch;
	UpdateSubresources(command_list_direct.Get(),
		indirect_command_buffer_voxel_debug.Get(),
		indirect_command_upload_buffer_voxel_debug.Get(),
		0,
		0,
		1,
		&perFrameIndirectCommandSubresourceData);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1;
	uavDesc.Buffer.StructureByteStride = sizeof(IndirectCommand);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	d3dDevice->CreateUnorderedAccessView(
		indirect_command_buffer_voxel_debug.Get(),
		nullptr,
		&uavDesc,
		descriptor_cpu_handle_cbv_srv_uav_all_other_resources);

	descriptor_cpu_handle_cbv_srv_uav_all_other_resources.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
	descriptor_heap_cbv_srv_uav_all_other_resources_number_of_filled_descriptors++;
	#pragma endregion

	#pragma region Indirect Command Buffer Cone Direction Debug Line Draw
	// Create UAVs for each frame IndirectCommand
	IndirectCommand indirectCommandConeDirectionDebug;
	indirectCommandConeDirectionDebug.draw_indexed_arguments.IndexCountPerInstance = cone_direction_debug_line.indices.size();
	indirectCommandConeDirectionDebug.draw_indexed_arguments.StartIndexLocation = 0;
	indirectCommandConeDirectionDebug.draw_indexed_arguments.StartInstanceLocation = 0;
	indirectCommandConeDirectionDebug.draw_indexed_arguments.BaseVertexLocation = 0;

	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(AlignForUavCounter(sizeof(IndirectCommand)), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&indirect_command_buffer_cone_direction_debug)));
	indirect_command_buffer_cone_direction_debug->SetName(L"SceneRenderer3D indirect_command_buffer_cone_direction_debug");

	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(AlignForUavCounter(sizeof(IndirectCommand))),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indirect_command_upload_buffer_cone_direction_debug)));
	indirect_command_upload_buffer_cone_direction_debug->SetName(L"SceneRenderer3D indirect_command_upload_buffer_cone_direction_debug");

	D3D12_SUBRESOURCE_DATA indirectCommandConeDirectionDebugSubresourceData;
	indirectCommandConeDirectionDebugSubresourceData.pData = &indirectCommand;
	indirectCommandConeDirectionDebugSubresourceData.RowPitch = sizeof(IndirectCommand);
	indirectCommandConeDirectionDebugSubresourceData.SlicePitch = perFrameIndirectCommandSubresourceData.RowPitch;
	UpdateSubresources(command_list_direct.Get(),
		indirect_command_buffer_cone_direction_debug.Get(),
		indirect_command_upload_buffer_cone_direction_debug.Get(),
		0,
		0,
		1,
		&indirectCommandConeDirectionDebugSubresourceData);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescConeDirectionDebug = {};
	uavDescConeDirectionDebug.Format = DXGI_FORMAT_UNKNOWN;
	uavDescConeDirectionDebug.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDescConeDirectionDebug.Buffer.FirstElement = 0;
	uavDescConeDirectionDebug.Buffer.NumElements = 1;
	uavDescConeDirectionDebug.Buffer.StructureByteStride = sizeof(IndirectCommand);
	uavDescConeDirectionDebug.Buffer.CounterOffsetInBytes = 0;
	uavDescConeDirectionDebug.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	d3dDevice->CreateUnorderedAccessView(
		indirect_command_buffer_cone_direction_debug.Get(),
		nullptr,
		&uavDescConeDirectionDebug,
		descriptor_cpu_handle_cbv_srv_uav_all_other_resources);

	descriptor_cpu_handle_cbv_srv_uav_all_other_resources.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
	descriptor_heap_cbv_srv_uav_all_other_resources_number_of_filled_descriptors++;
	#pragma endregion

	#pragma region Transform Martix Buffers
	// Create and setup a constant buffer that holds each scene object's transform matrixes
	for (int i = 0; i < (TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES); i++)
	{
		free_slots_preallocated_array[i] = i;
	}

	transform_matrix_upload_buffers.push_back(Microsoft::WRL::ComPtr<ID3D12Resource>());
	transform_matrix_mapped_upload_buffers.push_back(nullptr);
	available_transform_matrix_buffers.push_back(0);
	transform_matrix_buffer_free_slots.push_back(vector<UINT>(free_slots_preallocated_array, free_slots_preallocated_array + TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES));

	// Create an accompanying upload heap 
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(c_aligned_transform_matrix_buffer),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&transform_matrix_upload_buffers[0])));
	NAME_D3D12_OBJECT(transform_matrix_upload_buffers[0]);

	// Create constant buffer views to access the upload buffer.
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = transform_matrix_upload_buffers[0]->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = c_aligned_transform_matrix_buffer;
	d3dDevice->CreateConstantBufferView(&cbvDesc, descriptor_cpu_handle_cbv_srv_uav_all_other_resources);
	descriptor_cpu_handle_cbv_srv_uav_all_other_resources.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
	descriptor_heap_cbv_srv_uav_all_other_resources_number_of_filled_descriptors++;


	// Map the constant buffer.
	// We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE readRangeModelTransformMatrixBuffer(0, 0);	// We do not intend to read from this resource on the CPU.
	ThrowIfFailed(transform_matrix_upload_buffers[0]->Map(0, &readRangeModelTransformMatrixBuffer, reinterpret_cast<void**>(&transform_matrix_mapped_upload_buffers[0])));
	#pragma endregion

	// Close the command list and execute it to begin the vertex/index buffer copy into the GPU's default heap.
	ThrowIfFailed(command_list_direct.Get()->Close());
	ID3D12CommandList* ppCommandLists[] = { command_list_direct.Get() };
	device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Wait for the command list to finish executing; the vertex/index buffers need to be uploaded to the GPU before the upload resources go out of scope.
	device_resources->WaitForGpu();

	// Release some of the upload buffers that we will never use again
	test_texture_upload.Reset();
	voxel_debug_cube.vertex_buffer_upload.Reset();
	voxel_debug_cube.index_buffer_upload.Reset();
	indirect_command_upload_buffer_voxel_debug.Reset();
	cone_direction_debug_line.vertex_buffer_upload.Reset();
	cone_direction_debug_line.index_buffer_upload.Reset();
	indirect_command_upload_buffer_cone_direction_debug.Reset();

	// Create the vertex and index buffer views for the voxel debug cube
	voxel_debug_cube.vertex_buffer_view.BufferLocation = voxel_debug_cube.vertex_buffer->GetGPUVirtualAddress();
	voxel_debug_cube.vertex_buffer_view.StrideInBytes = sizeof(ShaderStructureCPUVertexPositionNormalTextureColor);
	voxel_debug_cube.vertex_buffer_view.SizeInBytes = voxel_debug_cube.vertices.size() * sizeof(ShaderStructureCPUVertexPositionNormalTextureColor);

	voxel_debug_cube.index_buffer_view.BufferLocation = voxel_debug_cube.index_buffer->GetGPUVirtualAddress();
	voxel_debug_cube.index_buffer_view.SizeInBytes = voxel_debug_cube.indices.size() * sizeof(uint16_t);
	voxel_debug_cube.index_buffer_view.Format = DXGI_FORMAT_R16_UINT;

	// Create the vertex and index buffer views for the cone direction debug line
	cone_direction_debug_line.vertex_buffer_view.BufferLocation = cone_direction_debug_line.vertex_buffer->GetGPUVirtualAddress();
	cone_direction_debug_line.vertex_buffer_view.StrideInBytes = sizeof(ShaderStructureCPUVertexPositionNormalTextureColor);
	cone_direction_debug_line.vertex_buffer_view.SizeInBytes = cone_direction_debug_line.vertices.size() * sizeof(ShaderStructureCPUVertexPositionNormalTextureColor);

	cone_direction_debug_line.index_buffer_view.BufferLocation = cone_direction_debug_line.index_buffer->GetGPUVirtualAddress();
	cone_direction_debug_line.index_buffer_view.SizeInBytes = cone_direction_debug_line.indices.size() * sizeof(uint16_t);
	cone_direction_debug_line.index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
}

void SceneRenderer3D::UpdateBuffers(bool updateVoxelizerBuffers, bool updateVoxelGridDataBuffers)
{
	ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_unlit.Get()));
	if (updateVoxelizerBuffers == true)
	{
		UpdateVoxelizerBuffers();
	}

	if ((updateVoxelizerBuffers == false) && (updateVoxelGridDataBuffers == true))
	{
		UpdateVoxelGridDataBuffer();
	}

	// Close the command list and execute it to begin the vertex/index buffer copy into the GPU's default heap.
	ThrowIfFailed(command_list_direct.Get()->Close());
	ID3D12CommandList* ppCommandLists[] = { command_list_direct.Get() };
	device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Wait for the command list to finish executing; the vertex/index buffers need to be uploaded to the GPU before the upload resources go out of scope.
	device_resources->WaitForGpu();

	// Release the upload buffers
	if (updateVoxelizerBuffers == true)
	{
		//voxel_debug_constant_upload_buffer.Reset();
		voxel_data_structured_upload_buffer.Reset();
	}
}

void SceneRenderer3D::UpdateVoxelizerBuffers()
{
	// Setup the voxelizer scissor rect
	scissor_rect_voxelizer = { 0, 0, static_cast<LONG>(voxel_grid_data.res), static_cast<LONG>(voxel_grid_data.res) };
	// Setup the voxelizer viewport
	viewport_voxelizer.Width = voxel_grid_data.res;
	viewport_voxelizer.Height = voxel_grid_data.res;
	viewport_voxelizer.TopLeftX = 0;
	viewport_voxelizer.TopLeftY = 0;
	viewport_voxelizer.MinDepth = 0.0f;
	viewport_voxelizer.MaxDepth = 1.0f;

	// Create all the buffers used by the voxelizer
	auto d3dDevice = device_resources->GetD3DDevice();
	descriptor_cpu_handle_cbv_srv_uav_voxelizer.InitOffsetted(descriptor_heap_cbv_srv_uav_voxelizer->GetCPUDescriptorHandleForHeapStart(), c_frame_count * device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());

	number_of_voxels_in_grid = voxel_grid_data.res * voxel_grid_data.res * voxel_grid_data.res;
	
	#pragma region Indirect Draw Required Voxel Debug Data
	if (indirect_draw_required_voxel_debug_data_buffer.Get() != nullptr)
	{
		descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors--;
	}

	// Create the unordered access views (UAVs) that store the results of the compute work.
	counter_offset_indirect_draw_required_voxel_debug_data = AlignForUavCounter(sizeof(ShaderStructureCPUVoxelDebugData) * number_of_voxels_in_grid);
	// Allocate a buffer large enough to hold all of the indirect commands
	// for a single frame as well as a UAV counter.
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(counter_offset_indirect_draw_required_voxel_debug_data + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&indirect_draw_required_voxel_debug_data_buffer)));
	NAME_D3D12_OBJECT(indirect_draw_required_voxel_debug_data_buffer);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = number_of_voxels_in_grid;
	uavDesc.Buffer.StructureByteStride = sizeof(ShaderStructureCPUVoxelDebugData);
	uavDesc.Buffer.CounterOffsetInBytes = counter_offset_indirect_draw_required_voxel_debug_data;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	d3dDevice->CreateUnorderedAccessView(
		indirect_draw_required_voxel_debug_data_buffer.Get(),
		indirect_draw_required_voxel_debug_data_buffer.Get(),
		&uavDesc,
		descriptor_cpu_handle_cbv_srv_uav_voxelizer);

	descriptor_cpu_handle_cbv_srv_uav_voxelizer.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
	descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors++;
	#pragma endregion

	#pragma region Indirect Draw Required Cone Direction Debug Data
	if (indirect_draw_required_cone_direction_debug_data_buffer.Get() != nullptr)
	{
		descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors--;
	}

	// Create the unordered access views (UAVs) that store the results of the compute work.
	counter_offset_indirect_draw_required_cone_direction_debug_data = AlignForUavCounter(sizeof(ShaderStructureCPUConeDirectionDebugData) * voxel_grid_data.num_cones * 20);
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(counter_offset_indirect_draw_required_cone_direction_debug_data + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&indirect_draw_required_cone_direction_debug_data_buffer)));
	indirect_draw_required_cone_direction_debug_data_buffer->SetName(L"SceneRenderer3D indirect_draw_required_cone_direction_debug_data_buffer");

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescConeDirectionDebug = {};
	uavDescConeDirectionDebug.Format = DXGI_FORMAT_UNKNOWN;
	uavDescConeDirectionDebug.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDescConeDirectionDebug.Buffer.FirstElement = 0;
	uavDescConeDirectionDebug.Buffer.NumElements = voxel_grid_data.num_cones * 20;
	uavDescConeDirectionDebug.Buffer.StructureByteStride = sizeof(ShaderStructureCPUConeDirectionDebugData);
	uavDescConeDirectionDebug.Buffer.CounterOffsetInBytes = counter_offset_indirect_draw_required_cone_direction_debug_data;
	uavDescConeDirectionDebug.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	d3dDevice->CreateUnorderedAccessView(
		indirect_draw_required_cone_direction_debug_data_buffer.Get(),
		indirect_draw_required_cone_direction_debug_data_buffer.Get(),
		&uavDescConeDirectionDebug,
		descriptor_cpu_handle_cbv_srv_uav_voxelizer);

	descriptor_cpu_handle_cbv_srv_uav_voxelizer.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
	descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors++;
	#pragma endregion

	#pragma region Voxel Data RW Structured Buffer
	if (voxel_data_structured_buffer.Get() != nullptr)
	{
		descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors--;
	}

	UINT64 alignedVoxelDataStructuredBufferSize = ((number_of_voxels_in_grid * sizeof(UINT32) * 2) + 255) & ~255;

	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(alignedVoxelDataStructuredBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&voxel_data_structured_buffer)));
	voxel_data_structured_buffer->SetName(L"SceneRenderer3D voxel_data_structured_buffer");

	//voxelDataStructuredBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(alignedVoxelDataStructuredBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&voxel_data_structured_upload_buffer)));
	voxel_data_structured_upload_buffer->SetName(L"SceneRenderer3D voxel_data_structured_upload_buffer");

	// Zero out the resource
	UINT8* pVoxelDataStructuredBufferData = new UINT8[alignedVoxelDataStructuredBufferSize];
	std::ZeroMemory(pVoxelDataStructuredBufferData, alignedVoxelDataStructuredBufferSize);
	D3D12_SUBRESOURCE_DATA subresourceDataVoxelDataStructuredBuffer;
	subresourceDataVoxelDataStructuredBuffer.pData = pVoxelDataStructuredBufferData;
	subresourceDataVoxelDataStructuredBuffer.RowPitch = alignedVoxelDataStructuredBufferSize;
	subresourceDataVoxelDataStructuredBuffer.SlicePitch = subresourceDataVoxelDataStructuredBuffer.RowPitch;

	UpdateSubresources(command_list_direct.Get(), voxel_data_structured_buffer.Get(), voxel_data_structured_upload_buffer.Get(), 0, 0, 1, &subresourceDataVoxelDataStructuredBuffer);
	delete[] pVoxelDataStructuredBufferData;

	D3D12_RESOURCE_BARRIER voxelDataStructuredBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(voxel_data_structured_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	command_list_direct.Get()->ResourceBarrier(1, &voxelDataStructuredBufferResourceBarrier);

	// Create a UAV
	D3D12_UNORDERED_ACCESS_VIEW_DESC voxelDataStructuredBufferUAVDesc = {};
	voxelDataStructuredBufferUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	voxelDataStructuredBufferUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	voxelDataStructuredBufferUAVDesc.Buffer.FirstElement = 0;
	voxelDataStructuredBufferUAVDesc.Buffer.NumElements = number_of_voxels_in_grid;
	voxelDataStructuredBufferUAVDesc.Buffer.StructureByteStride = sizeof(UINT32) * 2;
	voxelDataStructuredBufferUAVDesc.Buffer.CounterOffsetInBytes = 0;
	d3dDevice->CreateUnorderedAccessView(voxel_data_structured_buffer.Get(), nullptr, &voxelDataStructuredBufferUAVDesc, descriptor_cpu_handle_cbv_srv_uav_voxelizer);
	descriptor_cpu_handle_cbv_srv_uav_voxelizer.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
	descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors++;
	#pragma endregion

	#pragma region Radiance 3D Texture
	if (radiance_texture_3D.Get() != nullptr)
	{
		descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors -= 2 * radiance_texture_3D_mip_count_copy;
	}
	
	radiance_texture_3D_mip_count_copy = voxel_grid_data.mip_count;
	descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors += 2 * voxel_grid_data.mip_count;
	D3D12_RESOURCE_DESC radiance3DTextureDesc = {};
	radiance3DTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	radiance3DTextureDesc.Width = voxel_grid_data.res;
	radiance3DTextureDesc.Height = voxel_grid_data.res;
	radiance3DTextureDesc.DepthOrArraySize = voxel_grid_data.res;
	radiance3DTextureDesc.MipLevels = voxel_grid_data.mip_count;
	radiance3DTextureDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
	radiance3DTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	radiance3DTextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	radiance3DTextureDesc.Alignment = 0;
	radiance3DTextureDesc.SampleDesc.Count = 1;
	radiance3DTextureDesc.SampleDesc.Quality = 0;
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&radiance3DTextureDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&radiance_texture_3D)));
	radiance_texture_3D->SetName(L"SceneRenderer3D radiacne_texture_3D");
	// Create an SRV and a UAV for each mip level
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescRadince3DTexture = {};
	srvDescRadince3DTexture.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescRadince3DTexture.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDescRadince3DTexture.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescRadiance3DTexture = {};
	uavDescRadiance3DTexture.Format = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
	uavDescRadiance3DTexture.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	for (int i = 0; i < voxel_grid_data.mip_count; i++)
	{
		srvDescRadince3DTexture.Texture3D.MostDetailedMip = i;
		srvDescRadince3DTexture.Texture3D.MipLevels = 1;
		// The first SRV for the mip level 0 needs to see all other mips, because this srv is the one used
		// when tracing the 3D texture, all the other SRVs for all the other mip levels can leave this setting at 1
		// since they will only be used when generating the mip chain, they won't be used to blend multiple mips together
		if (i == 0)
		{
			srvDescRadince3DTexture.Texture3D.MipLevels = voxel_grid_data.mip_count;
		}

		d3dDevice->CreateShaderResourceView(radiance_texture_3D.Get(), &srvDescRadince3DTexture, descriptor_cpu_handle_cbv_srv_uav_voxelizer);
		descriptor_cpu_handle_cbv_srv_uav_voxelizer.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
	}

	for (int i = 0; i < voxel_grid_data.mip_count; i++)
	{
		uavDescRadiance3DTexture.Texture3D.MipSlice = i;
		uavDescRadiance3DTexture.Texture3D.FirstWSlice = 0;
		uavDescRadiance3DTexture.Texture3D.WSize = -1;
		d3dDevice->CreateUnorderedAccessView(
			radiance_texture_3D.Get(),
			nullptr,
			&uavDescRadiance3DTexture,
			descriptor_cpu_handle_cbv_srv_uav_voxelizer);
		descriptor_cpu_handle_cbv_srv_uav_voxelizer.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
	}
	#pragma endregion

	UpdateVoxelGridDataBuffer();

	temp++;
}

void SceneRenderer3D::UpdateVoxelGridDataBuffer()
{
	voxel_grid_data_constant_buffer_frame_index_containing_most_updated_version = device_resources->GetCurrentFrameIndex();
	memcpy(voxel_grid_data_constant_mapped_buffer + (voxel_grid_data_constant_buffer_frame_index_containing_most_updated_version * c_aligned_shader_structure_cpu_voxel_grid_data), &voxel_grid_data, sizeof(ShaderStructureCPUVoxelGridData));
}

// Initializes view parameters when the window size changes.
void SceneRenderer3D::CreateWindowSizeDependentResources(Camera& camera)
{
	
}


// Saves the current state of the renderer.
void SceneRenderer3D::SaveState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;

	if (state->HasKey(AngleKey))
	{
		state->Remove(AngleKey);
	}

	//state->Insert(AngleKey, PropertyValue::CreateSingle(m_angle));
}

// Restores the previous state of the renderer.
void SceneRenderer3D::LoadState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;
	if (state->HasKey(AngleKey))
	{
		//m_angle = safe_cast<IPropertyValue^>(state->Lookup(AngleKey))->GetSingle();
		//state->Remove(AngleKey);
	}
}

void SceneRenderer3D::CopyDescriptorsIntoDescriptorHeap(CD3DX12_CPU_DESCRIPTOR_HANDLE& destinationDescriptorHandle)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorCpuHandleCbvSrvUavVoxelizer = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptor_heap_cbv_srv_uav_voxelizer->GetCPUDescriptorHandleForHeapStart(), 0);
	device_resources->GetD3DDevice()->CopyDescriptorsSimple(descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors, destinationDescriptorHandle, descriptorCpuHandleCbvSrvUavVoxelizer, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	destinationDescriptorHandle.Offset(descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors * device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());

	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorCpuHandleCbvSrvUavAllOtherResources = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptor_heap_cbv_srv_uav_all_other_resources->GetCPUDescriptorHandleForHeapStart(), 0);
	device_resources->GetD3DDevice()->CopyDescriptorsSimple(descriptor_heap_cbv_srv_uav_all_other_resources_number_of_filled_descriptors, destinationDescriptorHandle, descriptorCpuHandleCbvSrvUavAllOtherResources, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	destinationDescriptorHandle.Offset(descriptor_heap_cbv_srv_uav_all_other_resources_number_of_filled_descriptors * device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());
}

void SceneRenderer3D::AssignDescriptors(ID3D12GraphicsCommandList* _commandList, CD3DX12_GPU_DESCRIPTOR_HANDLE& descriptorHandle, UINT rootParameterIndex, bool assignCompute)
{
	// Bind the descriptor tables to their respective heap starts
	UINT currentFrameIndex = device_resources->GetCurrentFrameIndex();
	UINT cbvSrvUavDescriptorSize = device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav();
	if (assignCompute == false)
	{
		// Voxel Grid Data CBV
		descriptorHandle.Offset(voxel_grid_data_constant_buffer_frame_index_containing_most_updated_version * cbvSrvUavDescriptorSize);
		_commandList->SetGraphicsRootDescriptorTable(4, descriptorHandle);
		descriptorHandle.Offset((c_frame_count - voxel_grid_data_constant_buffer_frame_index_containing_most_updated_version) * cbvSrvUavDescriptorSize);
		// Required voxel debug data for frame draw UAV
		_commandList->SetGraphicsRootDescriptorTable(5, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Required cone debug data for frame draw UAV
		_commandList->SetGraphicsRootDescriptorTable(6, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Voxel Data RW Buffer UAV
		_commandList->SetGraphicsRootDescriptorTable(7, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Radiance Texture 3D SRV
		_commandList->SetGraphicsRootDescriptorTable(8, descriptorHandle);
		descriptorHandle.Offset(voxel_grid_data.mip_count * cbvSrvUavDescriptorSize);
		// Radiance Texture 3D UAV
		_commandList->SetGraphicsRootDescriptorTable(9, descriptorHandle);
		descriptorHandle.Offset(voxel_grid_data.mip_count * cbvSrvUavDescriptorSize);
		// SRV test texture
		_commandList->SetGraphicsRootDescriptorTable(11, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Indirect command Voxel Debug
		_commandList->SetGraphicsRootDescriptorTable(12, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Indirect command Cone Debug
		_commandList->SetGraphicsRootDescriptorTable(13, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Transform buffers CBV
		_commandList->SetGraphicsRootDescriptorTable(14, descriptorHandle);
	}
	else
	{
		// Voxel Grid Data CBV
		descriptorHandle.Offset(voxel_grid_data_constant_buffer_frame_index_containing_most_updated_version * cbvSrvUavDescriptorSize);
		_commandList->SetComputeRootDescriptorTable(4, descriptorHandle);
		descriptorHandle.Offset((c_frame_count - voxel_grid_data_constant_buffer_frame_index_containing_most_updated_version) * cbvSrvUavDescriptorSize);
		// Required voxel debug data for frame draw UAV
		_commandList->SetComputeRootDescriptorTable(5, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Required cone debug data for frame draw UAV
		_commandList->SetComputeRootDescriptorTable(6, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Voxel Data RW Buffer UAV
		_commandList->SetComputeRootDescriptorTable(7, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Radiance Texture 3D SRV
		_commandList->SetComputeRootDescriptorTable(8, descriptorHandle);
		descriptorHandle.Offset(voxel_grid_data.mip_count * cbvSrvUavDescriptorSize);
		// Radiance Texture 3D UAV
		_commandList->SetComputeRootDescriptorTable(9, descriptorHandle);
		descriptorHandle.Offset(voxel_grid_data.mip_count * cbvSrvUavDescriptorSize);
		// SRV test texture
		_commandList->SetComputeRootDescriptorTable(11, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Indirect command Voxel Debug
		_commandList->SetComputeRootDescriptorTable(12, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Indirect command Cone Debug
		_commandList->SetComputeRootDescriptorTable(13, descriptorHandle);
		descriptorHandle.Offset(cbvSrvUavDescriptorSize);
		// Transform buffers CBV
		_commandList->SetComputeRootDescriptorTable(14, descriptorHandle);
	}
}

// Renders one frame using the vertex and pixel shaders.
void SceneRenderer3D::Render(vector<Mesh>& scene, SpotLight& spotLight, Camera& camera, bool showVoxelDebugView, DrawMode drawMode)
{
	UINT currentFrameIndex = device_resources->GetCurrentFrameIndex();
	// Proccess each mesh
	bool commandListCopyNormalPriorityRequiresExecution = false;
	bool commandListCopyHighPriorityRequiresExecution = false;
	UINT64 fenceCommandListCopyNormalPriorityProgressValue = fence_command_list_copy_normal_priority_progress->GetCompletedValue();
	UINT64 fenceCommandListCopyHighPriorityProgressValue = fence_command_list_copy_high_priority_progress->GetCompletedValue();
	// Reset the copy command allocator if there are no command lists executing on it
	if ((fence_command_list_copy_normal_priority_progress_latest_unused_value - 1) == fenceCommandListCopyNormalPriorityProgressValue)
	{
		if (command_allocator_copy_normal_priority_already_reset == false)
		{
			command_allocator_copy_normal_priority_already_reset = true;
			ThrowIfFailed(device_resources->GetCommandAllocatorCopyNormalPriority()->Reset());
		}
	}

	if ((fence_command_list_copy_high_priority_progress_latest_unused_value - 1) == fenceCommandListCopyHighPriorityProgressValue)
	{
		if (command_allocator_copy_high_priority_already_reset == false)
		{
			command_allocator_copy_high_priority_already_reset = true;
			ThrowIfFailed(device_resources->GetCommandAllocatorCopyHighPriority()->Reset());
		}
	}
	
	// Iterate over each object inside of the scene, check if all the resources needed
	// for their rendering are present on the gpu, and if so render the object
	scene_object_indexes_that_require_rendering.clear();
	for (UINT i = 0; i < scene.size(); i++)
	{
		// Vertex and index buffer check
		if (scene[i].fence_value_signaling_vertex_and_index_bufferr_residency <= fenceCommandListCopyNormalPriorityProgressValue)
		{
			// If the data has been uploaded to the gpu, there is no more need for the upload buffers
			// Also since this is the first time this mesh is being renderer, we need to create 
			// the vertex and index buffer views
			if (scene[i].vertex_and_index_buffer_upload_started == true)
			{
				scene[i].vertex_and_index_buffer_upload_started = false;
				scene[i].vertex_buffer_upload.Reset();
				scene[i].index_buffer_upload.Reset();

				// Create vertex/index views
				scene[i].vertex_buffer_view.BufferLocation = scene[i].vertex_buffer->GetGPUVirtualAddress();
				scene[i].vertex_buffer_view.StrideInBytes = sizeof(ShaderStructureCPUVertexPositionNormalTextureColor);
				scene[i].vertex_buffer_view.SizeInBytes = scene[i].vertices.size() * sizeof(ShaderStructureCPUVertexPositionNormalTextureColor);

				scene[i].index_buffer_view.BufferLocation = scene[i].index_buffer->GetGPUVirtualAddress();
				scene[i].index_buffer_view.SizeInBytes = scene[i].indices.size() * sizeof(uint16_t);
				scene[i].index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
			}

			// Now we can render out this object.
			// But before that we need to start uploading it's model transform matrix to the gpu
			// If the object isn't static we will need to update its model transform matrix
			ShaderStructureCPUModelAndInverseTransposeModelView* _entry = nullptr;
			if (scene[i].is_static == false)
			{
				commandListCopyHighPriorityRequiresExecution = true;
				if (command_list_copy_high_priority_requires_reset == true)
				{
					command_list_copy_high_priority_requires_reset = false;
					ThrowIfFailed(command_list_copy_high_priority->Reset(device_resources->GetCommandAllocatorCopyHighPriority(), pipeline_state_unlit.Get()));
				}
				
				scene[i].current_frame_index_containing_most_updated_transform_matrix++;
				if (scene[i].current_frame_index_containing_most_updated_transform_matrix == c_frame_count)
				{
					scene[i].current_frame_index_containing_most_updated_transform_matrix = 0;
				}

				// Check if this object has a slot assigned to it, if not assign one
				if (scene[i].per_frame_transform_matrix_buffer_indexes_assigned[scene[i].current_frame_index_containing_most_updated_transform_matrix] == false)
				{
					scene[i].per_frame_transform_matrix_buffer_indexes_assigned[scene[i].current_frame_index_containing_most_updated_transform_matrix] = true;
					scene[i].per_frame_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_transform_matrix][0] = available_transform_matrix_buffers[0];
					auto& freeSlotsArray = transform_matrix_buffer_free_slots[available_transform_matrix_buffers[0]];
					scene[i].per_frame_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_transform_matrix][1] = freeSlotsArray[0];
					// Erase that free slot
					freeSlotsArray.erase(freeSlotsArray.begin());
					// Check if that was the last free slot, and if so update the per_frame_available_scene_object_model_transform_matrix_buffer array
					if (freeSlotsArray.size() == 0)
					{
						available_transform_matrix_buffers.erase(available_transform_matrix_buffers.begin());
						// Check if we ran out of buffers with free slots for this frame index
						if (available_transform_matrix_buffers.size() == 0)
						{
							// Create a new buffer
							transform_matrix_upload_buffers.push_back(Microsoft::WRL::ComPtr<ID3D12Resource>());
							transform_matrix_mapped_upload_buffers.push_back(nullptr);
							transform_matrix_buffer_free_slots.push_back(vector<UINT>(free_slots_preallocated_array, free_slots_preallocated_array + TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES));
							available_transform_matrix_buffers.push_back(transform_matrix_buffer_free_slots.size() - 1);

							// Create an accompanying upload heap 
							ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
								&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
								D3D12_HEAP_FLAG_NONE,
								&CD3DX12_RESOURCE_DESC::Buffer(sizeof(ShaderStructureCPUModelAndInverseTransposeModelView) * TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES),
								D3D12_RESOURCE_STATE_GENERIC_READ,
								nullptr,
								IID_PPV_ARGS(&transform_matrix_upload_buffers[transform_matrix_upload_buffers.size() - 1])));

							// Create constant buffer views to access the upload buffer.
							D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
							cbvDesc.BufferLocation = transform_matrix_upload_buffers[transform_matrix_mapped_upload_buffers.size() - 1]->GetGPUVirtualAddress();
							cbvDesc.SizeInBytes = sizeof(ShaderStructureCPUModelAndInverseTransposeModelView) * TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES;
							device_resources->GetD3DDevice()->CreateConstantBufferView(&cbvDesc, descriptor_cpu_handle_cbv_srv_uav_all_other_resources);
							descriptor_cpu_handle_cbv_srv_uav_all_other_resources.Offset(device_resources->GetDescriptorSizeDescriptorHeapCbvSrvUav());


							// Map the constant buffer.
							// We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.
							CD3DX12_RANGE readRangeModelTransformMatrixBuffer(0, 0);	// We do not intend to read from this resource on the CPU.
							ThrowIfFailed(transform_matrix_upload_buffers[transform_matrix_upload_buffers.size() - 1]->Map(0, &readRangeModelTransformMatrixBuffer, reinterpret_cast<void**>(&transform_matrix_mapped_upload_buffers[transform_matrix_mapped_upload_buffers.size() - 1])));
						}
					}
				}

				XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(scene[i].local_rotation.x),
					XMConvertToRadians(scene[i].local_rotation.y),
					XMConvertToRadians(scene[i].local_rotation.z));
				XMMATRIX translationMatrix = XMMatrixTranslation(scene[i].world_position.x,
					scene[i].world_position.y,
					scene[i].world_position.z);

				_entry = transform_matrix_mapped_upload_buffers[scene[i].per_frame_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_transform_matrix][0]] + scene[i].per_frame_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_transform_matrix][1];
				scene[i].transform_matrix = XMMatrixMultiply(rotationMatrix, translationMatrix);
				DirectX::XMStoreFloat4x4(&_entry->model, XMMatrixTranspose(scene[i].transform_matrix));

				// This way if the object was initialized as static
				// this would have been it's one and only model transform update.
				// After this it won't be updated anymore
				scene[i].is_static = scene[i].initialized_as_static;
				scene[i].initialized_as_static = false;
			}
			else
			{
				_entry = transform_matrix_mapped_upload_buffers[scene[i].per_frame_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_transform_matrix][0]] + scene[i].per_frame_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_transform_matrix][1];
			}
			
			DirectX::XMStoreFloat4x4(&_entry->inverse_transpose_model_view, /*XMMatrixTranspose(*/XMMatrixInverse(nullptr, XMMatrixMultiply(scene[i].transform_matrix, camera.GetViewMatrix())))/*)*/;

			scene_object_indexes_that_require_rendering.push_back(i);
		}
		else
		{
			if (scene[i].vertex_and_index_buffer_upload_started == false)
			{
				scene[i].vertex_and_index_buffer_upload_started = true;
				commandListCopyNormalPriorityRequiresExecution = true;
				if (command_list_copy_normal_priority_requires_reset == true)
				{
					command_list_copy_normal_priority_requires_reset = false;
					ThrowIfFailed(command_list_copy_normal_priority->Reset(device_resources->GetCommandAllocatorCopyNormalPriority(), pipeline_state_unlit.Get()));
				}
				
				// Upload the vertex buffer to the GPU.
				const UINT vertexBufferSize = scene[i].vertices.size() * sizeof(ShaderStructureCPUVertexPositionNormalTextureColor);
				CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
				CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
				ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
					&defaultHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&vertexBufferDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&scene[i].vertex_buffer)));

				CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
				ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
					&uploadHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&vertexBufferDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&scene[i].vertex_buffer_upload)));


				D3D12_SUBRESOURCE_DATA vertexData = {};
				vertexData.pData = reinterpret_cast<BYTE*>(&scene[i].vertices[0]);
				vertexData.RowPitch = vertexBufferSize;
				vertexData.SlicePitch = vertexData.RowPitch;

				UpdateSubresources(command_list_copy_normal_priority.Get(), scene[i].vertex_buffer.Get(), scene[i].vertex_buffer_upload.Get(), 0, 0, 1, &vertexData);


				// Upload the index buffer to the GPU.
				const UINT indexBufferSize = sizeof(uint16_t) * scene[i].indices.size();
				CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
				ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
					&defaultHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&indexBufferDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&scene[i].index_buffer)));

				ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
					&uploadHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&indexBufferDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&scene[i].index_buffer_upload)));

				D3D12_SUBRESOURCE_DATA indexData = {};
				indexData.pData = reinterpret_cast<BYTE*>(&scene[i].indices[0]);
				indexData.RowPitch = indexBufferSize;
				indexData.SlicePitch = indexData.RowPitch;

				UpdateSubresources(command_list_copy_normal_priority.Get(), scene[i].index_buffer.Get(), scene[i].index_buffer_upload.Get(), 0, 0, 1, &indexData);

				scene[i].fence_value_signaling_vertex_and_index_bufferr_residency = fence_command_list_copy_normal_priority_progress_latest_unused_value;
			}
		}
	}

	if (scene_object_indexes_that_require_rendering.size() != 0)
	{
		ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_spot_light_write_only_depth.Get()));
		ID3D12DescriptorHeap* _pHeaps[] = { device_resources->GetDescriptorHeapCbvSrvUav() };
		ID3D12CommandList* _pCommandListsDirect[] = { command_list_direct.Get() };

		// Populate the device_resource shader visible descriptor heap
		command_list_direct->SetGraphicsRootSignature(root_signature.Get());
		command_list_direct->SetDescriptorHeaps(_countof(_pHeaps), _pHeaps);

		CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHeapCPUHandleDeviceResources(device_resources->GetDescriptorHeapCbvSrvUav()->GetCPUDescriptorHandleForHeapStart());
		camera.CopyDescriptorsIntoDescriptorHeap(descriptorHeapCPUHandleDeviceResources);
		spotLight.CopyDescriptorsIntoDescriptorHeap(descriptorHeapCPUHandleDeviceResources, true, true);
		CopyDescriptorsIntoDescriptorHeap(descriptorHeapCPUHandleDeviceResources);

		CD3DX12_GPU_DESCRIPTOR_HANDLE descriptorHeapGPUHandleDeviceResources(device_resources->GetDescriptorHeapCbvSrvUav()->GetGPUDescriptorHandleForHeapStart());
		camera.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 1, false);
		spotLight.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 2, false);
		AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 4, false);

		#pragma region Spot Light Write Only Depth Pass
		command_list_direct->RSSetViewports(1, &spotLight.GetViewport());
		command_list_direct->RSSetScissorRects(1, &spotLight.GetScissorRect());
		// Bind the render target and depth buffer
		command_list_direct->ClearDepthStencilView(spotLight.GetShadowMapDepthStencilView(), D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
		command_list_direct->OMSetRenderTargets(0, nullptr, false, &spotLight.GetShadowMapDepthStencilView());
		command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		for (UINT i = 0; i < scene_object_indexes_that_require_rendering.size(); i++)
		{
			// Set the transform matrix buffer indexes
			command_list_direct->SetGraphicsRoot32BitConstants(0,
				2,
				scene[scene_object_indexes_that_require_rendering[i]].per_frame_transform_matrix_buffer_indexes[scene[scene_object_indexes_that_require_rendering[i]].current_frame_index_containing_most_updated_transform_matrix],
				0);

			// Bind the vertex and index buffer views
			command_list_direct->IASetVertexBuffers(0, 1, &scene[scene_object_indexes_that_require_rendering[i]].vertex_buffer_view);
			command_list_direct->IASetIndexBuffer(&scene[scene_object_indexes_that_require_rendering[i]].index_buffer_view);
			command_list_direct->DrawIndexedInstanced(scene[scene_object_indexes_that_require_rendering[i]].indices.size(), 1, 0, 0, 0);
		}
		#pragma endregion

		#pragma region Spot Light Shadow Map Pass
		command_list_direct->SetPipelineState(pipeline_state_spot_light_shadow_pass.Get());
		// Bind the render target and depth buffer
		float clearColor[1] = { 0.0f };
		command_list_direct->ClearRenderTargetView(spotLight.GetShadowMapRenderTargetView(), clearColor, 0, nullptr);
		command_list_direct->OMSetRenderTargets(1, &spotLight.GetShadowMapRenderTargetView(), false, &device_resources->GetDepthStencilView());
		CD3DX12_RESOURCE_BARRIER spotLightShadowMapPassBarriers[2];
		spotLightShadowMapPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
			spotLight.GetShadowMapDepthBuffer(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		command_list_direct->ResourceBarrier(1, spotLightShadowMapPassBarriers);
		for (UINT i = 0; i < scene_object_indexes_that_require_rendering.size(); i++)
		{
			// Set the transform matrix buffer indexes
			command_list_direct->SetGraphicsRoot32BitConstants(0,
				2,
				scene[scene_object_indexes_that_require_rendering[i]].per_frame_transform_matrix_buffer_indexes[scene[scene_object_indexes_that_require_rendering[i]].current_frame_index_containing_most_updated_transform_matrix],
				0);

			// Bind the vertex and index buffer views
			command_list_direct->IASetVertexBuffers(0, 1, &scene[scene_object_indexes_that_require_rendering[i]].vertex_buffer_view);
			command_list_direct->IASetIndexBuffer(&scene[scene_object_indexes_that_require_rendering[i]].index_buffer_view);
			command_list_direct->DrawIndexedInstanced(scene[scene_object_indexes_that_require_rendering[i]].indices.size(), 1, 0, 0, 0);
		}

		spotLightShadowMapPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
			spotLight.GetShadowMapDepthBuffer(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE);

		command_list_direct->ResourceBarrier(1, spotLightShadowMapPassBarriers);
		#pragma endregion

		#pragma region Full Screen Visualization
		/*
		command_list_direct->SetPipelineState(pipeline_state_full_screen_texture_visualization.Get());
		// Bind the render target and depth buffer
		command_list_direct->ClearDepthStencilView(device_resources->GetDepthStencilView(), D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
		command_list_direct->RSSetViewports(1, &device_resources->GetScreenViewport());
		command_list_direct->RSSetScissorRects(1, &device_resources->GetScissorRect());
		command_list_direct->OMSetRenderTargets(1, &device_resources->GetRenderTargetView(), false, &device_resources->GetDepthStencilView());
		spotLightShadowMapPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
			spotLight.GetShadowMapDepthBuffer(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
		spotLightShadowMapPassBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
			spotLight.GetShadowMapBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		command_list_direct->ResourceBarrier(2, spotLightShadowMapPassBarriers);
		command_list_direct->IASetVertexBuffers(0, 1, &scene[scene_object_indexes_that_require_rendering[0]].vertex_buffer_view);
		command_list_direct->IASetIndexBuffer(&scene[scene_object_indexes_that_require_rendering[0]].index_buffer_view);
		command_list_direct->DrawIndexedInstanced(scene[scene_object_indexes_that_require_rendering[0]].indices.size(), 1, 0, 0, 0);
		spotLightShadowMapPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
			spotLight.GetShadowMapBuffer(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		command_list_direct->ResourceBarrier(1, spotLightShadowMapPassBarriers);
		command_list_direct->Close();
		device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(_pCommandListsDirect), _pCommandListsDirect);
		//device_resources->GetCommandQueueDirect()->Signal(fence_command_list_direct_progress.Get(), fence_command_list_direct_progress_latest_unused_value);
		//fence_command_list_direct_progress_latest_unused_value++;
		*/
		#pragma endregion

		#pragma region Voxelizer Render Pass
		command_list_direct->SetPipelineState(pipeline_state_voxelizer.Get());
		command_list_direct->RSSetViewports(1, &viewport_voxelizer);
		command_list_direct->RSSetScissorRects(1, &scissor_rect_voxelizer);
		command_list_direct->OMSetRenderTargets(0, nullptr, false, nullptr);
		
		spotLightShadowMapPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
			spotLight.GetShadowMapBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		command_list_direct->ResourceBarrier(1, spotLightShadowMapPassBarriers);

		for (UINT i = 0; i < scene_object_indexes_that_require_rendering.size(); i++)
		{
			// Set the transform matrix buffer indexes
			command_list_direct->SetGraphicsRoot32BitConstants(0,
				2,
				scene[scene_object_indexes_that_require_rendering[i]].per_frame_transform_matrix_buffer_indexes[scene[scene_object_indexes_that_require_rendering[i]].current_frame_index_containing_most_updated_transform_matrix],
				0);

			// Bind the vertex and index buffer views
			command_list_direct->IASetVertexBuffers(0, 1, &scene[scene_object_indexes_that_require_rendering[i]].vertex_buffer_view);
			command_list_direct->IASetIndexBuffer(&scene[scene_object_indexes_that_require_rendering[i]].index_buffer_view);
			command_list_direct->DrawIndexedInstanced(scene[scene_object_indexes_that_require_rendering[i]].indices.size(), 1, 0, 0, 0);
		}

		spotLightShadowMapPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
			spotLight.GetShadowMapBuffer(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		command_list_direct->ResourceBarrier(1, spotLightShadowMapPassBarriers);

		command_list_direct->Close();
		device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(_pCommandListsDirect), _pCommandListsDirect);
		device_resources->GetCommandQueueDirect()->Signal(fence_command_list_direct_progress.Get(), fence_command_list_direct_progress_latest_unused_value);
		fence_command_list_direct_progress_latest_unused_value++;
		#pragma endregion

		#pragma region Radiance Temporal Copy Clear Render Pass
		ThrowIfFailed(device_resources->GetCommandAllocatorCompute()->Reset());
		ThrowIfFailed(command_list_compute->Reset(device_resources->GetCommandAllocatorCompute(), pipeline_state_radiance_temporal_clear.Get()));
		command_list_compute->SetComputeRootSignature(root_signature.Get());
		command_list_compute->SetDescriptorHeaps(_countof(_pHeaps), _pHeaps);

		descriptorHeapGPUHandleDeviceResources.InitOffsetted(device_resources->GetDescriptorHeapCbvSrvUav()->GetGPUDescriptorHandleForHeapStart(), 0);

		camera.AssignDescriptors(command_list_compute.Get(), descriptorHeapGPUHandleDeviceResources, 1, true);
		spotLight.AssignDescriptors(command_list_compute.Get(), descriptorHeapGPUHandleDeviceResources, 2, true);
		AssignDescriptors(command_list_compute.Get(), descriptorHeapGPUHandleDeviceResources, 4, true);

		command_list_compute->Dispatch(max(1u, (number_of_voxels_in_grid / 256)), 1, 1);
		#pragma endregion

		#pragma region Radiance Texture 3D Mip Chain Generation Pass
		command_list_compute->SetPipelineState(pipeline_state_radiance_mip_chain_generation.Get());
		radiance_texture_3D_generate_mip_chain_data.output_resolution = voxel_grid_data.res;
		for (int i = 0; i < voxel_grid_data.mip_count - 1; i++)
		{
			radiance_texture_3D_generate_mip_chain_data.input_mip_level = i;
			radiance_texture_3D_generate_mip_chain_data.output_mip_level = i + 1;
			
			 D3D12_RESOURCE_BARRIER radianceTexture3DSubresourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
				radiance_texture_3D.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				i);
			command_list_compute->ResourceBarrier(1, &radianceTexture3DSubresourceBarrier);
			
			
			radiance_texture_3D_generate_mip_chain_data.output_resolution = max(1u, radiance_texture_3D_generate_mip_chain_data.output_resolution / 2);
			radiance_texture_3D_generate_mip_chain_data.output_resolution_rcp = 1.0f / (float)radiance_texture_3D_generate_mip_chain_data.output_resolution;

			command_list_compute->SetComputeRoot32BitConstants(10, sizeof(ShaderStructureCPUGenerate3DMipChainData) / sizeof(UINT32), &radiance_texture_3D_generate_mip_chain_data, 0);
			UINT dispatchBlockSize = max(1u, (radiance_texture_3D_generate_mip_chain_data.output_resolution + GENERATE_3D_MIP_CHAIN_DISPATCH_BLOCK_SIZE - 1) / GENERATE_3D_MIP_CHAIN_DISPATCH_BLOCK_SIZE);
			command_list_compute->Dispatch(dispatchBlockSize, dispatchBlockSize, dispatchBlockSize);
			
			// If the secondary bounce is disabled, we won't be transitioning subresources back to a UAV state
			// because we need the entire resource to be in an SRV state for the final gather anyway
			if ((bool)voxel_grid_data.secondary_bounce_enabled == true)
			{
				radianceTexture3DSubresourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
					radiance_texture_3D.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					i);
				command_list_compute->ResourceBarrier(1, &radianceTexture3DSubresourceBarrier);
			}
		}

		// We need to transition the last mip as well 
		D3D12_RESOURCE_BARRIER radianceTexture3DSubresourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
			radiance_texture_3D.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			voxel_grid_data.mip_count - 1);
		command_list_compute->ResourceBarrier(1, &radianceTexture3DSubresourceBarrier);
		// If the secondary bounce is disabled, we won't be transitioning subresources back to a UAV state
		// because we need the entire resource to be in an SRV state for the final gather anyway
		if ((bool)voxel_grid_data.secondary_bounce_enabled == true)
		{
			radianceTexture3DSubresourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
				radiance_texture_3D.Get(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				voxel_grid_data.mip_count - 1);
			command_list_compute->ResourceBarrier(1, &radianceTexture3DSubresourceBarrier);
		}

		// Only the VoxelDebugView DrawMode requiers an additional compute pass,
		// So that's why if the drawMode is set to VoxelDebugView we dont close the command list here
		// But instead it will be closed in that subsequent compute pass
		if (drawMode != DrawMode::VoxelDebugView)
		{
			command_list_compute->Close();
			ID3D12CommandList* _pCommandListsCompute[] = { command_list_compute.Get() };
			device_resources->GetCommandQueueCompute()->Wait(fence_command_list_direct_progress.Get(), fence_command_list_direct_progress_latest_unused_value - 1);
			device_resources->GetCommandQueueCompute()->ExecuteCommandLists(_countof(_pCommandListsCompute), _pCommandListsCompute);
			device_resources->GetCommandQueueCompute()->Signal(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value);
			fence_command_list_compute_progress_latest_unused_value++;
		}
		#pragma endregion
		
		switch (drawMode)
		{
			case DrawMode::VoxelDebugView:
			{
				#pragma region Generate Debug Voxels Pass
				command_list_compute->SetPipelineState(pipeline_state_voxel_debug_draw_compute.Get());
				radiance_texture_3D_generate_mip_chain_data.output_resolution = voxel_grid_resolutions[((UINT32)log2(voxel_grid_data.res)) - radiance_texture_3D_voxel_debug_draw_mip_level];
				radiance_texture_3D_generate_mip_chain_data.output_resolution_rcp = 1.0f / (float)radiance_texture_3D_generate_mip_chain_data.output_resolution;
				radiance_texture_3D_generate_mip_chain_data.input_mip_level = radiance_texture_3D_voxel_debug_draw_mip_level;

				command_list_compute->CopyBufferRegion(indirect_draw_required_voxel_debug_data_buffer.Get(), counter_offset_indirect_draw_required_voxel_debug_data, indirect_draw_counter_reset_buffer.Get(), 0, sizeof(UINT));
				command_list_compute->CopyBufferRegion(indirect_command_buffer_voxel_debug.Get(), sizeof(UINT), indirect_draw_counter_reset_buffer.Get(), 0, sizeof(UINT));

				D3D12_RESOURCE_BARRIER perFrameIndirectProcesedCommandsBufferBarriers[2];
				perFrameIndirectProcesedCommandsBufferBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(indirect_draw_required_voxel_debug_data_buffer.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				perFrameIndirectProcesedCommandsBufferBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(indirect_command_buffer_voxel_debug.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

				command_list_compute->ResourceBarrier(2, perFrameIndirectProcesedCommandsBufferBarriers);


				command_list_compute->SetComputeRoot32BitConstants(10, sizeof(ShaderStructureCPUGenerate3DMipChainData) / sizeof(UINT32), &radiance_texture_3D_generate_mip_chain_data, 0);
				UINT dispatchBlockSize = max(1u, ((radiance_texture_3D_generate_mip_chain_data.output_resolution * radiance_texture_3D_generate_mip_chain_data.output_resolution * radiance_texture_3D_generate_mip_chain_data.output_resolution) / 256));
				command_list_compute->Dispatch(dispatchBlockSize, 1, 1);

				command_list_compute->Close();
				ID3D12CommandList* _pCommandListsCompute[] = { command_list_compute.Get() };
				device_resources->GetCommandQueueCompute()->Wait(fence_command_list_direct_progress.Get(), fence_command_list_direct_progress_latest_unused_value - 1);
				device_resources->GetCommandQueueCompute()->ExecuteCommandLists(_countof(_pCommandListsCompute), _pCommandListsCompute);
				device_resources->GetCommandQueueCompute()->Signal(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value);
				fence_command_list_compute_progress_latest_unused_value++;
				#pragma endregion
		
				#pragma region Voxel Debug Visualization Pass
				ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_voxel_debug_draw.Get()));
				command_list_direct->SetGraphicsRootSignature(root_signature.Get());
				command_list_direct->SetDescriptorHeaps(_countof(_pHeaps), _pHeaps);

				descriptorHeapGPUHandleDeviceResources.InitOffsetted(device_resources->GetDescriptorHeapCbvSrvUav()->GetGPUDescriptorHandleForHeapStart(), 0);
				camera.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 1, false);
				spotLight.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 2, false);
				AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 4, false);

				command_list_direct->RSSetViewports(1, &device_resources->GetScreenViewport());
				command_list_direct->RSSetScissorRects(1, &device_resources->GetScissorRect());
				// Bind the render target and depth buffer
				D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = device_resources->GetRenderTargetView();
				D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = device_resources->GetDepthStencilView();
				// We have to clear the device_resources depth buffer since we used it in the shadow map render pass
				command_list_direct->ClearDepthStencilView(device_resources->GetDepthStencilView(), D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
				command_list_direct->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);
				command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				command_list_direct->IASetVertexBuffers(0, 1, &voxel_debug_cube.vertex_buffer_view);
				command_list_direct->IASetIndexBuffer(&voxel_debug_cube.index_buffer_view);

				// Indicate that the command buffer will be used for indirect drawing
				// and that the back buffer will be used as a render target.
				D3D12_RESOURCE_BARRIER voxelDebugVisualizationPassBarriers[3];
				voxelDebugVisualizationPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
					indirect_command_buffer_voxel_debug.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

				command_list_direct->ResourceBarrier(1, voxelDebugVisualizationPassBarriers);

				command_list_direct->SetGraphicsRoot32BitConstants(10, sizeof(ShaderStructureCPUGenerate3DMipChainData) / sizeof(UINT32), &radiance_texture_3D_generate_mip_chain_data, 0);
				command_list_direct->ExecuteIndirect(
					indirect_draw_command_signature.Get(),
					1,
					indirect_command_buffer_voxel_debug.Get(),
					0,
					nullptr,
					0);

				voxelDebugVisualizationPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(indirect_draw_required_voxel_debug_data_buffer.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_COPY_DEST);
				voxelDebugVisualizationPassBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(indirect_command_buffer_voxel_debug.Get(),
					D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
					D3D12_RESOURCE_STATE_COPY_DEST);
				voxelDebugVisualizationPassBarriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(
					radiance_texture_3D.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				command_list_direct->ResourceBarrier(3, voxelDebugVisualizationPassBarriers);


				command_list_direct->Close();
				device_resources->GetCommandQueueDirect()->Wait(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value - 1);
				device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(_pCommandListsDirect), _pCommandListsDirect);
				#pragma endregion

				break;
			}
			case DrawMode::IndirectDiffuseView:
			{
				#pragma region Final Gather Copy Pass
				ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_final_gather_copy.Get()));
				command_list_direct->SetGraphicsRootSignature(root_signature.Get());
				command_list_direct->SetDescriptorHeaps(_countof(_pHeaps), _pHeaps);

				descriptorHeapGPUHandleDeviceResources.InitOffsetted(device_resources->GetDescriptorHeapCbvSrvUav()->GetGPUDescriptorHandleForHeapStart(), 0);
				camera.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 1, false);
				spotLight.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 2, false);
				AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 4, false);

				command_list_direct->RSSetViewports(1, &device_resources->GetScreenViewport());
				command_list_direct->RSSetScissorRects(1, &device_resources->GetScissorRect());
				command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = device_resources->GetRenderTargetView();
				D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = device_resources->GetDepthStencilView();
				command_list_direct->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
				command_list_direct->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);

				command_list_direct->CopyBufferRegion(indirect_draw_required_cone_direction_debug_data_buffer.Get(), counter_offset_indirect_draw_required_cone_direction_debug_data, indirect_draw_counter_reset_buffer.Get(), 0, sizeof(UINT));
				command_list_direct->CopyBufferRegion(indirect_command_buffer_cone_direction_debug.Get(), sizeof(UINT), indirect_draw_counter_reset_buffer.Get(), 0, sizeof(UINT));

				D3D12_RESOURCE_BARRIER finalGatherCopySubresourceBarriers[3];
				finalGatherCopySubresourceBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
					radiance_texture_3D.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				finalGatherCopySubresourceBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(indirect_draw_required_cone_direction_debug_data_buffer.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				finalGatherCopySubresourceBarriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(indirect_command_buffer_cone_direction_debug.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				command_list_direct->ResourceBarrier(3, finalGatherCopySubresourceBarriers);

				for (UINT i = 0; i < scene_object_indexes_that_require_rendering.size(); i++)
				{
					// Set the transform matrix buffer indexes
					command_list_direct->SetGraphicsRoot32BitConstants(0,
						2,
						scene[scene_object_indexes_that_require_rendering[i]].per_frame_transform_matrix_buffer_indexes[scene[scene_object_indexes_that_require_rendering[i]].current_frame_index_containing_most_updated_transform_matrix],
						0);

					// Bind the vertex and index buffer views
					command_list_direct->IASetVertexBuffers(0, 1, &scene[scene_object_indexes_that_require_rendering[i]].vertex_buffer_view);
					command_list_direct->IASetIndexBuffer(&scene[scene_object_indexes_that_require_rendering[i]].index_buffer_view);
					command_list_direct->DrawIndexedInstanced(scene[scene_object_indexes_that_require_rendering[i]].indices.size(), 1, 0, 0, 0);
				}

				finalGatherCopySubresourceBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
					radiance_texture_3D.Get(),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				command_list_direct->ResourceBarrier(1, finalGatherCopySubresourceBarriers);

				/*command_list_direct->Close();
				device_resources->GetCommandQueueDirect()->Wait(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value - 1);
				device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(_pCommandListsDirect), _pCommandListsDirect);*/
				#pragma endregion

				#pragma region Cone Direction Line Draw Pass
				command_list_direct->SetPipelineState(pipeline_state_cone_direction_debug_line_draw.Get());
				command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
				command_list_direct->IASetVertexBuffers(0, 1, &cone_direction_debug_line.vertex_buffer_view);
				command_list_direct->IASetIndexBuffer(&cone_direction_debug_line.index_buffer_view);

				D3D12_RESOURCE_BARRIER coneDirectionLineDrawSubresourceBarriers[2]; 
				coneDirectionLineDrawSubresourceBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
					indirect_command_buffer_cone_direction_debug.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
				command_list_direct->ResourceBarrier(1, coneDirectionLineDrawSubresourceBarriers);

				command_list_direct->ExecuteIndirect(
					indirect_draw_command_signature.Get(),
					1,
					indirect_command_buffer_cone_direction_debug.Get(),
					0,
					nullptr,
					0);

				coneDirectionLineDrawSubresourceBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
					indirect_draw_required_cone_direction_debug_data_buffer.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_COPY_DEST);
				coneDirectionLineDrawSubresourceBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
					indirect_command_buffer_cone_direction_debug.Get(),
					D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
					D3D12_RESOURCE_STATE_COPY_DEST);
				command_list_direct->ResourceBarrier(2, coneDirectionLineDrawSubresourceBarriers);

				command_list_direct->Close();
				device_resources->GetCommandQueueDirect()->Wait(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value - 1);
				device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(_pCommandListsDirect), _pCommandListsDirect);
				#pragma endregion

				break;
			}
			case DrawMode::FinalGatherView:
			{
				#pragma region Final Gather Pass
				ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_final_gather.Get()));
				command_list_direct->SetGraphicsRootSignature(root_signature.Get());
				command_list_direct->SetDescriptorHeaps(_countof(_pHeaps), _pHeaps);

				descriptorHeapGPUHandleDeviceResources.InitOffsetted(device_resources->GetDescriptorHeapCbvSrvUav()->GetGPUDescriptorHandleForHeapStart(), 0);
				camera.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 1, false);
				spotLight.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 2, false);
				AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 4, false);

				command_list_direct->RSSetViewports(1, &device_resources->GetScreenViewport());
				command_list_direct->RSSetScissorRects(1, &device_resources->GetScissorRect());
				command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = device_resources->GetRenderTargetView();
				D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = device_resources->GetDepthStencilView();
				command_list_direct->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
				command_list_direct->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);

				D3D12_RESOURCE_BARRIER radianceTexture3DSubresourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
					radiance_texture_3D.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				command_list_direct->ResourceBarrier(1, &radianceTexture3DSubresourceBarrier);

				for (UINT i = 0; i < scene_object_indexes_that_require_rendering.size(); i++)
				{
					// Set the transform matrix buffer indexes
					command_list_direct->SetGraphicsRoot32BitConstants(0,
						2,
						scene[scene_object_indexes_that_require_rendering[i]].per_frame_transform_matrix_buffer_indexes[scene[scene_object_indexes_that_require_rendering[i]].current_frame_index_containing_most_updated_transform_matrix],
						0);

					// Bind the vertex and index buffer views
					command_list_direct->IASetVertexBuffers(0, 1, &scene[scene_object_indexes_that_require_rendering[i]].vertex_buffer_view);
					command_list_direct->IASetIndexBuffer(&scene[scene_object_indexes_that_require_rendering[i]].index_buffer_view);
					command_list_direct->DrawIndexedInstanced(scene[scene_object_indexes_that_require_rendering[i]].indices.size(), 1, 0, 0, 0);
				}

				radianceTexture3DSubresourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
					radiance_texture_3D.Get(),
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				command_list_direct->ResourceBarrier(1, &radianceTexture3DSubresourceBarrier);

				command_list_direct->Close();
				device_resources->GetCommandQueueDirect()->Wait(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value - 1);
				device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(_pCommandListsDirect), _pCommandListsDirect);
				#pragma endregion
				break;
			}
		}

		//if (showVoxelDebugView == true)
		//{
			#pragma region Voxel Debug Visualization Pass
			/*
			ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_voxel_debug_visualization.Get()));
			command_list_direct->SetGraphicsRootSignature(root_signature.Get());
			command_list_direct->SetDescriptorHeaps(_countof(_pHeaps), _pHeaps);

			descriptorHeapGPUHandleDeviceResources.InitOffsetted(device_resources->GetDescriptorHeapCbvSrvUav()->GetGPUDescriptorHandleForHeapStart(), 0);
			camera.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 1, false);
			spotLight.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 2, false);
			AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 4, false);

			command_list_direct->RSSetViewports(1, &device_resources->GetScreenViewport());
			command_list_direct->RSSetScissorRects(1, &device_resources->GetScissorRect());
			// Bind the render target and depth buffer
			D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = device_resources->GetRenderTargetView();
			D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = device_resources->GetDepthStencilView();
			// We have to clear the device_resources depth buffer since we used it in the shadow map render pass
			command_list_direct->ClearDepthStencilView(device_resources->GetDepthStencilView(), D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
			command_list_direct->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);
			command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			command_list_direct->IASetVertexBuffers(0, 1, &voxel_debug_cube.vertex_buffer_view);
			command_list_direct->IASetIndexBuffer(&voxel_debug_cube.index_buffer_view);

			// Indicate that the command buffer will be used for indirect drawing
			// and that the back buffer will be used as a render target.
			D3D12_RESOURCE_BARRIER voxelDebugVisualizationPassBarriers[3];
			voxelDebugVisualizationPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
				indirect_command_buffer.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

			command_list_direct->ResourceBarrier(1, voxelDebugVisualizationPassBarriers);

			command_list_direct->ExecuteIndirect(
				voxel_debug_command_signature.Get(),
				1,
				indirect_command_buffer.Get(),
				0,
				nullptr,
				0);

			voxelDebugVisualizationPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(indirect_draw_required_voxel_debug_data_buffer.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_COPY_DEST);
			voxelDebugVisualizationPassBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(indirect_command_buffer.Get(),
				D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
				D3D12_RESOURCE_STATE_COPY_DEST);
			voxelDebugVisualizationPassBarriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(
				radiance_texture_3D.Get(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			command_list_direct->ResourceBarrier(3, voxelDebugVisualizationPassBarriers);
			



			command_list_direct->Close();
			device_resources->GetCommandQueueDirect()->Wait(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value - 1);
			device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(_pCommandListsDirect), _pCommandListsDirect);
			*/
			#pragma endregion

			//goto LABEL_END_OF_RENDER_PASSES;
		//}
		//else
		//{
			#pragma region Final Gather Pass
			/*
			ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_final_gather.Get()));
			command_list_direct->SetGraphicsRootSignature(root_signature.Get());
			command_list_direct->SetDescriptorHeaps(_countof(_pHeaps), _pHeaps);

			descriptorHeapGPUHandleDeviceResources.InitOffsetted(device_resources->GetDescriptorHeapCbvSrvUav()->GetGPUDescriptorHandleForHeapStart(), 0);
			camera.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 1, false);
			spotLight.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 2, false);
			AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 4, false);

			command_list_direct->RSSetViewports(1, &device_resources->GetScreenViewport());
			command_list_direct->RSSetScissorRects(1, &device_resources->GetScissorRect());
			command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = device_resources->GetRenderTargetView();
			D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = device_resources->GetDepthStencilView();
			command_list_direct->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
			command_list_direct->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);

			D3D12_RESOURCE_BARRIER radianceTexture3DSubresourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
				radiance_texture_3D.Get(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			command_list_direct->ResourceBarrier(1, &radianceTexture3DSubresourceBarrier);

			for (UINT i = 0; i < scene_object_indexes_that_require_rendering.size(); i++)
			{
				// Set the transform matrix buffer indexes
				command_list_direct->SetGraphicsRoot32BitConstants(0,
					2,
					scene[scene_object_indexes_that_require_rendering[i]].per_frame_transform_matrix_buffer_indexes[scene[scene_object_indexes_that_require_rendering[i]].current_frame_index_containing_most_updated_transform_matrix],
					0);

				// Bind the vertex and index buffer views
				command_list_direct->IASetVertexBuffers(0, 1, &scene[scene_object_indexes_that_require_rendering[i]].vertex_buffer_view);
				command_list_direct->IASetIndexBuffer(&scene[scene_object_indexes_that_require_rendering[i]].index_buffer_view);
				command_list_direct->DrawIndexedInstanced(scene[scene_object_indexes_that_require_rendering[i]].indices.size(), 1, 0, 0, 0);
			}

			radianceTexture3DSubresourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
				radiance_texture_3D.Get(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			command_list_direct->ResourceBarrier(1, &radianceTexture3DSubresourceBarrier);

			command_list_direct->Close();
			device_resources->GetCommandQueueDirect()->Wait(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value - 1);
			device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(_pCommandListsDirect), _pCommandListsDirect);
			*/
			#pragma endregion

			#pragma region Default Render Pass
			/*
			ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_unlit.Get()));
			command_list_direct->SetGraphicsRootSignature(root_signature.Get());
			command_list_direct->SetDescriptorHeaps(_countof(_pHeaps), _pHeaps);

			descriptorHeapGPUHandleDeviceResources.InitOffsetted(device_resources->GetDescriptorHeapCbvSrvUav()->GetGPUDescriptorHandleForHeapStart(), 0);
			camera.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 1, false);
			spotLight.AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 2, false);
			AssignDescriptors(command_list_direct.Get(), descriptorHeapGPUHandleDeviceResources, 4, false);

			command_list_direct->RSSetViewports(1, &device_resources->GetScreenViewport());
			command_list_direct->RSSetScissorRects(1, &device_resources->GetScissorRect());
			command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = device_resources->GetRenderTargetView();
			D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = device_resources->GetDepthStencilView();
			command_list_direct->ClearRenderTargetView(renderTargetView, DirectX::Colors::CornflowerBlue, 0, nullptr);
			command_list_direct->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAGS::D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
			command_list_direct->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);
			for (UINT i = 0; i < scene_object_indexes_that_require_rendering.size(); i++)
			{
				// Set the transform matrix buffer indexes
				command_list_direct->SetGraphicsRoot32BitConstants(0,
					2,
					scene[scene_object_indexes_that_require_rendering[i]].per_frame_transform_matrix_buffer_indexes[scene[scene_object_indexes_that_require_rendering[i]].current_frame_index_containing_most_updated_transform_matrix],
					0);

				// Bind the vertex and index buffer views
				command_list_direct->IASetVertexBuffers(0, 1, &scene[scene_object_indexes_that_require_rendering[i]].vertex_buffer_view);
				command_list_direct->IASetIndexBuffer(&scene[scene_object_indexes_that_require_rendering[i]].index_buffer_view);
				command_list_direct->DrawIndexedInstanced(scene[scene_object_indexes_that_require_rendering[i]].indices.size(), 1, 0, 0, 0);
			}

			D3D12_RESOURCE_BARRIER radianceTexture3DSubresourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
				radiance_texture_3D.Get(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			command_list_direct->ResourceBarrier(1, &radianceTexture3DSubresourceBarrier);

			command_list_direct->Close();
			device_resources->GetCommandQueueDirect()->Wait(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value - 1);
			device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(_pCommandListsDirect), _pCommandListsDirect);
			*/
			#pragma endregion
		//}
	}

	
	LABEL_END_OF_RENDER_PASSES:
	

	// Check if we need to execute normal priority copy command queue
	if (commandListCopyNormalPriorityRequiresExecution == true)
	{
		command_allocator_copy_normal_priority_already_reset = false;
		command_list_copy_normal_priority_requires_reset = true;
		ThrowIfFailed(command_list_copy_normal_priority->Close());
		ID3D12CommandList* ppCommandLists[] = { command_list_copy_normal_priority.Get() };
		device_resources->GetCommandQueueCopyNormalPriority()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		
		ThrowIfFailed(device_resources->GetCommandQueueCopyNormalPriority()->Signal(fence_command_list_copy_normal_priority_progress.Get(), fence_command_list_copy_normal_priority_progress_latest_unused_value));
		fence_command_list_copy_normal_priority_progress_latest_unused_value++;
	}

	// Wait for the previous frame's high priority copy command queue workload to finish
	if (fence_per_frame_command_list_copy_high_priority_values[previous_frame_index] > fenceCommandListCopyHighPriorityProgressValue)
	{
		ThrowIfFailed(fence_command_list_copy_high_priority_progress->SetEventOnCompletion(fence_per_frame_command_list_copy_high_priority_values[previous_frame_index], event_wait_for_gpu));
		WaitForSingleObjectEx(event_wait_for_gpu, INFINITE, FALSE);
	}

	previous_frame_index = currentFrameIndex;
}
