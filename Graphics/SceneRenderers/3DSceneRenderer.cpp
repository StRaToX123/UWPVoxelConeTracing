#include "Graphics/SceneRenderers/3DSceneRenderer.h"




// Indices into the application state map.
Platform::String^ AngleKey = "Angle";
Platform::String^ TrackingKey = "Tracking";

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
SceneRenderer3D::SceneRenderer3D(const std::shared_ptr<DeviceResources>& deviceResources, Camera& camera) :
	device_resources(deviceResources),
	camera_view_projection_constant_mapped_buffer(nullptr),
	fence_command_list_direct_progress_latest_unused_value(1),
	fence_command_list_compute_progress_latest_unused_value(1),
	fence_command_list_copy_normal_priority_progress_latest_unused_value(1),
	fence_command_list_copy_high_priority_progress_latest_unused_value(1),
	command_allocator_copy_normal_priority_already_reset(true),
	command_list_copy_normal_priority_requires_reset(false),
	command_allocator_copy_high_priority_already_reset(true),
	command_list_copy_high_priority_requires_reset(false),
	event_command_list_copy_high_priority_progress(NULL),
	previous_frame_index(c_frame_count - 1)
{
	voxel_grid_data.UpdateRes(ShaderStructureCPUVoxelGridData::AllowedResolution::RES_64);
	scissor_rect_voxelizer = { 0, 0, static_cast<LONG>(voxel_grid_data.res), static_cast<LONG>(voxel_grid_data.res) };

	LoadState();
	std::ZeroMemory(&camera_view_projection_constant_buffer_data, sizeof(camera_view_projection_constant_buffer_data));

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources(camera);
}

SceneRenderer3D::~SceneRenderer3D()
{
	camera_view_projection_constant_buffer->Unmap(0, nullptr);
	camera_view_projection_constant_mapped_buffer = nullptr;
	if (event_command_list_copy_high_priority_progress != NULL)
	{
		CloseHandle(event_command_list_copy_high_priority_progress);
	}
}

void SceneRenderer3D::LoadShaderByteCode(const char* shaderPath, _Out_ char*& shaderByteCode, _Out_ int& shaderByteCodeLength)
{
	ifstream is;
	is.open(shaderPath, ios::binary);
	// Get the length of file:
	is.seekg(0, ios::end);
	shaderByteCodeLength = is.tellg();
	is.seekg(0, ios::beg);
	// Allocate memory:
	shaderByteCode = new char[shaderByteCodeLength];
	// Read the data
	is.read(shaderByteCode, shaderByteCodeLength);
	is.close();
}

void SceneRenderer3D::CreateDeviceDependentResources()
{
	auto d3dDevice = device_resources->GetD3DDevice();

	CD3DX12_DESCRIPTOR_RANGE rangeSRVTestTexture;
	CD3DX12_DESCRIPTOR_RANGE rangeSRVVoxelDebugData;
	//CD3DX12_DESCRIPTOR_RANGE rangeSRVIndirectCommands;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVRequiredVoxelDebugDataForDrawInstanced;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVIndirectCommand;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVVoxelDataStructuredBuffer;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVRadianceTexture3D;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVVoxelGridData;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVviewProjectionMatrix;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVTransformMatrix;
	
	rangeSRVTestTexture.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	rangeSRVVoxelDebugData.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
	//rangeSRVIndirectCommands.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
	rangeUAVRequiredVoxelDebugDataForDrawInstanced.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
	rangeUAVIndirectCommand.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
	rangeUAVVoxelDataStructuredBuffer.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
	rangeUAVRadianceTexture3D.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);
	rangeCBVVoxelGridData.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	rangeCBVviewProjectionMatrix.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2); // register is 1 because register 0 is taken by the root constants
	rangeCBVTransformMatrix.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 3);

	CD3DX12_ROOT_PARAMETER parameterRootConstants; // contains transformMatrixBufferIndex, transformMatrixBufferInternalIndex
	CD3DX12_ROOT_PARAMETER parameterPixelVisibleDescriptorTableTestTexture;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableVoxelDebugData;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableRequiredVoxelDebugDataForDrawInstanced;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableIndirectCommand;
	CD3DX12_ROOT_PARAMETER parameterAllVisibleDescriptorTableVoxelDataStructuredBufferUAV;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableRadianceTexture3D;
	CD3DX12_ROOT_PARAMETER parameterGeometryVisibleDescriptorTableVoxelGridDataBuffer;
	CD3DX12_ROOT_PARAMETER parameterGeometryVisibleDescriptorTableViewProjectionMatrixBuffer;
	CD3DX12_ROOT_PARAMETER parameterGeometryVisibleDescriptorTableTransformMatrixBuffers;

	parameterRootConstants.InitAsConstants(2, 0, 0, D3D12_SHADER_VISIBILITY_ALL);
	parameterPixelVisibleDescriptorTableTestTexture.InitAsDescriptorTable(1, &rangeSRVTestTexture, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableVoxelDebugData.InitAsDescriptorTable(1, &rangeSRVVoxelDebugData, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableRequiredVoxelDebugDataForDrawInstanced.InitAsDescriptorTable(1, &rangeUAVRequiredVoxelDebugDataForDrawInstanced, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableIndirectCommand.InitAsDescriptorTable(1, &rangeUAVIndirectCommand, D3D12_SHADER_VISIBILITY_ALL);
	parameterAllVisibleDescriptorTableVoxelDataStructuredBufferUAV.InitAsDescriptorTable(1, &rangeUAVVoxelDataStructuredBuffer, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableRadianceTexture3D.InitAsDescriptorTable(1, &rangeUAVRadianceTexture3D, D3D12_SHADER_VISIBILITY_ALL);
	parameterGeometryVisibleDescriptorTableVoxelGridDataBuffer.InitAsDescriptorTable(1, &rangeCBVVoxelGridData, D3D12_SHADER_VISIBILITY_ALL);
	parameterGeometryVisibleDescriptorTableViewProjectionMatrixBuffer.InitAsDescriptorTable(1, &rangeCBVviewProjectionMatrix, D3D12_SHADER_VISIBILITY_ALL);
	parameterGeometryVisibleDescriptorTableTransformMatrixBuffers.InitAsDescriptorTable(1, &rangeCBVTransformMatrix, D3D12_SHADER_VISIBILITY_ALL);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // Only the input assembler stage needs access to the constant buffer.
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
	CD3DX12_ROOT_PARAMETER parameterArray[10] = { parameterRootConstants,
		parameterPixelVisibleDescriptorTableTestTexture,
		parameterComputeVisibleDescriptorTableVoxelDebugData,
		parameterComputeVisibleDescriptorTableRequiredVoxelDebugDataForDrawInstanced,
		parameterComputeVisibleDescriptorTableIndirectCommand,
		parameterAllVisibleDescriptorTableVoxelDataStructuredBufferUAV,
		parameterComputeVisibleDescriptorTableRadianceTexture3D,
		parameterGeometryVisibleDescriptorTableVoxelGridDataBuffer,
		parameterGeometryVisibleDescriptorTableViewProjectionMatrixBuffer,
		parameterGeometryVisibleDescriptorTableTransformMatrixBuffers
	};
	// Create a texture sampler
	D3D12_STATIC_SAMPLER_DESC staticSamplerDesc = {};
	staticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	staticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplerDesc.MipLODBias = 0;
	staticSamplerDesc.MaxAnisotropy = 0;
	staticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	staticSamplerDesc.MinLOD = 0.0f;
	staticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplerDesc.ShaderRegister = 0;
	staticSamplerDesc.RegisterSpace = 0;
	staticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descRootSignature.Init(10, parameterArray, 1, &staticSamplerDesc, rootSignatureFlags);

	ComPtr<ID3DBlob> pSignature;
	ComPtr<ID3DBlob> pError;
	D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf());
	if (pError.Get() != NULL)
	{
		DisplayDebugMessage("@@@@@@@@@@@@@@@@@@@@\n%s\n@@@@@@@@@@@@@@@@@@@@@\n", (char*)pError->GetBufferPointer());
		throw ref new FailureException();
	}

	ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
	NAME_D3D12_OBJECT(root_signature);

	wstring wStringInstallPath(Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data());
	string standardStringInstallPath(wStringInstallPath.begin(), wStringInstallPath.end());
#pragma region Default Pipeline
	// Create a default pipeline state that anybody can use
	// Create the pipeline state 
	char* pVertexShaderByteCode;
	int vertexShaderByteCodeLength;
	char* pPixelShaderByteCode;
	int pixelShaderByteCodeLength;
	LoadShaderByteCode((standardStringInstallPath + "\\Sample_VS.cso").c_str(), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\Sample_PS.cso").c_str(), pPixelShaderByteCode, pixelShaderByteCodeLength);


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

	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipeline_state_default)));
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
	//voxelizerState.NumRenderTargets = 1;
	//state.RTVFormats[0] = device_resources->GetBackBufferFormat();
	//state.DSVFormat = device_resources->GetDepthBufferFormat();
	voxelizerState.SampleDesc.Count = 1;

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateGraphicsPipelineState(&voxelizerState, IID_PPV_ARGS(&pipeline_state_voxelizer)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pVertexShaderByteCode;
	delete[] pGeometryShaderByteCode;
	delete[] pPixelShaderByteCode;

	// Setup the voxelizer viewport
	viewport_voxelizer.Width = voxel_grid_data.res;
	viewport_voxelizer.Height = voxel_grid_data.res;
	viewport_voxelizer.TopLeftX = 0;
	viewport_voxelizer.TopLeftY = 0;
	viewport_voxelizer.MinDepth = 0.0f;
	viewport_voxelizer.MaxDepth = 1.0f;
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


	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&voxelDebugVisualizationPiepelineStateDesc, IID_PPV_ARGS(&pipeline_voxel_debug_visualization)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
#pragma endregion

	// Create direct command list
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, device_resources->GetCommandAllocatorDirect(), pipeline_state_default.Get(), IID_PPV_ARGS(&command_list_direct)));
	NAME_D3D12_OBJECT(command_list_direct);
	// Create copy command lists.
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, device_resources->GetCommandAllocatorCopyNormalPriority(), pipeline_state_default.Get(), IID_PPV_ARGS(&command_list_copy_normal_priority)));
	NAME_D3D12_OBJECT(command_list_copy_normal_priority);
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, device_resources->GetCommandAllocatorCopyHighPriority(), pipeline_state_default.Get(), IID_PPV_ARGS(&command_list_copy_high_priority)));
	NAME_D3D12_OBJECT(command_list_copy_high_priority);
	// Create compute command list.
	ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, device_resources->GetCommandAllocatorCompute(), pipeline_state_radiance_temporal_clear.Get(), IID_PPV_ARGS(&command_list_compute)));
	NAME_D3D12_OBJECT(command_list_compute);
	command_list_compute->Close();
	// Create a fences to go along with the copy command lists.
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_direct_progress)));
	NAME_D3D12_OBJECT(fence_command_list_direct_progress);
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_compute_progress)));
	NAME_D3D12_OBJECT(fence_command_list_compute_progress);
	// This fence will be used to signal when a resource has been successfully uploaded to the gpu
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_copy_normal_priority_progress)));
	NAME_D3D12_OBJECT(fence_command_list_copy_normal_priority_progress);
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_copy_high_priority_progress)));
	NAME_D3D12_OBJECT(fence_command_list_copy_high_priority_progress);
	for (int i = 0; i < c_frame_count; i++)
	{
		fence_per_frame_command_list_copy_high_priority_values[i] = 0;
	}

	// Create an event for the high priority queue
	event_command_list_copy_high_priority_progress = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (event_command_list_copy_high_priority_progress == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	// Create a descriptor heap to house EVERYTHING!!!
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 100;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptor_heap_cbv_srv_uav)));
	NAME_D3D12_OBJECT(descriptor_heap_cbv_srv_uav);

	cbv_srv_uav_descriptor_size = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	cbv_srv_uav_cpu_handle.InitOffsetted(descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart(), 0);

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
	device_resources->GetD3DDevice()->CreateShaderResourceView(test_texture.Get(), &srvDesc, cbv_srv_uav_cpu_handle);
	cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);
#pragma endregion


#pragma region Voxel Debug Cube Resource Initialization
	voxel_debug_cube.InitializeAsCube();
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

#pragma region Indirect Execute
	// Create a constant buffer that holds all the model transform matrixes for each possible voxel position
	number_of_voxels_in_grid = voxel_grid_data.res * voxel_grid_data.res * voxel_grid_data.res;
	const UINT constantBufferDataSize = ((number_of_voxels_in_grid * sizeof(ShaderStructureCPUVoxelDebugData)) + 255) & ~255;
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(constantBufferDataSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&voxel_debug_constant_buffer)));

	NAME_D3D12_OBJECT(voxel_debug_constant_buffer);

	Microsoft::WRL::ComPtr<ID3D12Resource> voxelDebugConstantUploadBuffer;
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(constantBufferDataSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&voxelDebugConstantUploadBuffer)));

	NAME_D3D12_OBJECT(voxelDebugConstantUploadBuffer);
	// Map and initialize the constant buffer.
	ShaderStructureCPUVoxelDebugData* pVoxelDebugConstantBufferData = new ShaderStructureCPUVoxelDebugData[number_of_voxels_in_grid];
	float xCoordOriginal = voxel_grid_data.bottom_left_point_world_space_x + (voxel_grid_data.voxel_extent / 2.0f);
	float xCoord = xCoordOriginal;
	float yCoord = xCoordOriginal;
	float zCoord = xCoordOriginal;
	for (int i = 0; i < voxel_grid_data.res; i++)
	{
		for (int j = 0; j < voxel_grid_data.res; j++)
		{
			for (int k = 0; k < voxel_grid_data.res; k++)
			{
				UINT index = (i * voxel_grid_data.res * voxel_grid_data.res) + (j * voxel_grid_data.res) + k;
				//(pVoxelDebugConstantBufferData + index)->voxel_index = index;
				DirectX::XMStoreFloat4x4(&(pVoxelDebugConstantBufferData + index)->debug_cube_model_transform_matrix,
					XMMatrixTranspose(XMMatrixTranslation(xCoord, yCoord, zCoord)));
				xCoord += voxel_grid_data.voxel_extent;
			}

			xCoord = xCoordOriginal;
			yCoord += voxel_grid_data.voxel_extent;
		}

		xCoord = xCoordOriginal;
		yCoord = xCoordOriginal;
		zCoord += voxel_grid_data.voxel_extent;
	}

	D3D12_SUBRESOURCE_DATA voxelDebugConstantBufferSubresourceData;
	voxelDebugConstantBufferSubresourceData.pData = pVoxelDebugConstantBufferData;
	//voxelDebugConstantBufferSubresourceData.RowPitch = sizeof(ShaderStructureCPUVoxelDebugData) * number_of_voxels_in_grid;
	voxelDebugConstantBufferSubresourceData.RowPitch = ((number_of_voxels_in_grid * sizeof(ShaderStructureCPUVoxelDebugData)) + 255) & ~255;
	voxelDebugConstantBufferSubresourceData.SlicePitch = voxelDebugConstantBufferSubresourceData.RowPitch;
	UpdateSubresources(command_list_direct.Get(), voxel_debug_constant_buffer.Get(), voxelDebugConstantUploadBuffer.Get(), 0, 0, 1, &voxelDebugConstantBufferSubresourceData);
	command_list_direct.Get()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(voxel_debug_constant_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	delete[] pVoxelDebugConstantBufferData;
	// Create shader resource views (SRV) of the constant buffers for the
	// compute shader to read from.
	D3D12_SHADER_RESOURCE_VIEW_DESC indirectCommandSRVDesc = {};
	indirectCommandSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	indirectCommandSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	indirectCommandSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	indirectCommandSRVDesc.Buffer.NumElements = number_of_voxels_in_grid;
	indirectCommandSRVDesc.Buffer.StructureByteStride = sizeof(ShaderStructureCPUVoxelDebugData);
	indirectCommandSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	d3dDevice->CreateShaderResourceView(voxel_debug_constant_buffer.Get(), &indirectCommandSRVDesc, cbv_srv_uav_cpu_handle);
	cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);
	
	// Create the indirect command signature
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof(IndirectCommand);

	ThrowIfFailed(d3dDevice->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&voxel_debug_command_signature)));
	NAME_D3D12_OBJECT(voxel_debug_command_signature);

	// Create the unordered access views (UAVs) that store the results of the compute work.
	indirect_draw_required_voxel_debug_data_counter_offset = AlignForUavCounter(sizeof(ShaderStructureCPUVoxelDebugData) * number_of_voxels_in_grid);
	for (UINT frameIndex = 0; frameIndex < c_frame_count; frameIndex++)
	{
		// Allocate a buffer large enough to hold all of the indirect commands
		// for a single frame as well as a UAV counter.
		ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(indirect_draw_required_voxel_debug_data_counter_offset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&per_frame_indirect_draw_required_voxel_debug_data_buffer[frameIndex])));
		NAME_D3D12_OBJECT(per_frame_indirect_draw_required_voxel_debug_data_buffer[frameIndex]);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = number_of_voxels_in_grid;
		uavDesc.Buffer.StructureByteStride = sizeof(ShaderStructureCPUVoxelDebugData);
		uavDesc.Buffer.CounterOffsetInBytes = indirect_draw_required_voxel_debug_data_counter_offset;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		d3dDevice->CreateUnorderedAccessView(
			per_frame_indirect_draw_required_voxel_debug_data_buffer[frameIndex].Get(),
			per_frame_indirect_draw_required_voxel_debug_data_buffer[frameIndex].Get(),
			&uavDesc,
			cbv_srv_uav_cpu_handle);

		cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);
	}

	// Allocate a buffer that can be used to reset the UAV counters and initialize
	// it to 0.
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indirect_draw_required_voxel_debug_data_counter_reset_buffer)));

	UINT8* pMappedCounterReset = nullptr;
	ThrowIfFailed(indirect_draw_required_voxel_debug_data_counter_reset_buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pMappedCounterReset)));
	std::ZeroMemory(pMappedCounterReset, sizeof(UINT));
	indirect_draw_required_voxel_debug_data_counter_reset_buffer->Unmap(0, nullptr);

	// Create UAVs for each frame IndirectCommand
	IndirectCommand indirectCommand;
	indirectCommand.draw_indexed_arguments.IndexCountPerInstance = voxel_debug_cube.indices.size();
	indirectCommand.draw_indexed_arguments.StartIndexLocation = 0;
	indirectCommand.draw_indexed_arguments.StartInstanceLocation = 0;
	indirectCommand.draw_indexed_arguments.BaseVertexLocation = 0;
	for (UINT frameIndex = 0; frameIndex < c_frame_count; frameIndex++)
	{
		ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(AlignForUavCounter(sizeof(IndirectCommand)), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&per_frame_indirect_command_buffer[frameIndex])));
		NAME_D3D12_OBJECT(per_frame_indirect_command_buffer[frameIndex]);

		ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(AlignForUavCounter(sizeof(IndirectCommand))),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&per_frame_indirect_command_upload_buffer[frameIndex])));
		NAME_D3D12_OBJECT(per_frame_indirect_command_upload_buffer[frameIndex]);

		D3D12_SUBRESOURCE_DATA perFrameIndirectCommandSubresourceData;
		perFrameIndirectCommandSubresourceData.pData = &indirectCommand;
		perFrameIndirectCommandSubresourceData.RowPitch = sizeof(IndirectCommand);
		perFrameIndirectCommandSubresourceData.SlicePitch = perFrameIndirectCommandSubresourceData.RowPitch;
		UpdateSubresources(command_list_direct.Get(),
			per_frame_indirect_command_buffer[frameIndex].Get(),
			per_frame_indirect_command_upload_buffer[frameIndex].Get(),
			0,
			0,
			1,
			&perFrameIndirectCommandSubresourceData);

		command_list_direct->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(per_frame_indirect_command_buffer[frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1;
	uavDesc.Buffer.StructureByteStride = sizeof(IndirectCommand);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	for (UINT frameIndex = 0; frameIndex < c_frame_count; frameIndex++)
	{
		d3dDevice->CreateUnorderedAccessView(
			per_frame_indirect_command_buffer[frameIndex].Get(),
			nullptr,
			&uavDesc,
			cbv_srv_uav_cpu_handle);

		cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);
	}
#pragma endregion

#pragma region Voxel Data RW Structured Buffer
	D3D12_RESOURCE_DESC voxelDataStructuredBufferDesc = {};
	voxelDataStructuredBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	voxelDataStructuredBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	voxelDataStructuredBufferDesc.Width = number_of_voxels_in_grid * sizeof(UINT32) * 2;
	voxelDataStructuredBufferDesc.Height = 1;
	voxelDataStructuredBufferDesc.MipLevels = 1;
	voxelDataStructuredBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	voxelDataStructuredBufferDesc.DepthOrArraySize = 1;
	voxelDataStructuredBufferDesc.Alignment = 0;
	voxelDataStructuredBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	voxelDataStructuredBufferDesc.SampleDesc.Count = 1;
	voxelDataStructuredBufferDesc.SampleDesc.Quality = 0;
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&voxelDataStructuredBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&voxel_data_structured_buffer)));
	NAME_D3D12_OBJECT(voxel_data_structured_buffer);

	Microsoft::WRL::ComPtr<ID3D12Resource> voxelDataStructuredUploadBuffer;
	voxelDataStructuredBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&voxelDataStructuredBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&voxelDataStructuredUploadBuffer)));

	// Zero out the resource
	UINT32* pVoxelDataStructuredBufferData = new UINT32[number_of_voxels_in_grid * 2];
	std::ZeroMemory(pVoxelDataStructuredBufferData, number_of_voxels_in_grid * sizeof(UINT32) * 2);
	D3D12_SUBRESOURCE_DATA subresourceDataVoxelDataStructuredBuffer;
	subresourceDataVoxelDataStructuredBuffer.pData = pVoxelDataStructuredBufferData;
	subresourceDataVoxelDataStructuredBuffer.RowPitch = number_of_voxels_in_grid * sizeof(UINT32) * 2;
	subresourceDataVoxelDataStructuredBuffer.SlicePitch = subresourceDataVoxelDataStructuredBuffer.RowPitch;

	UpdateSubresources(command_list_direct.Get(), voxel_data_structured_buffer.Get(), voxelDataStructuredUploadBuffer.Get(), 0, 0, 1, &subresourceDataVoxelDataStructuredBuffer);
	delete[] pVoxelDataStructuredBufferData;

	D3D12_RESOURCE_BARRIER voxelDataStructuredBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(voxel_data_structured_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
	command_list_direct.Get()->ResourceBarrier(1, &voxelDataStructuredBufferResourceBarrier);

	// Create a UAV
	D3D12_UNORDERED_ACCESS_VIEW_DESC voxelDataStructuredBufferUAVDesc = {};
	voxelDataStructuredBufferUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	voxelDataStructuredBufferUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	voxelDataStructuredBufferUAVDesc.Buffer.FirstElement = 0;
	voxelDataStructuredBufferUAVDesc.Buffer.NumElements = number_of_voxels_in_grid;
	voxelDataStructuredBufferUAVDesc.Buffer.StructureByteStride = sizeof(UINT32) * 2;
	voxelDataStructuredBufferUAVDesc.Buffer.CounterOffsetInBytes = 0;
	d3dDevice->CreateUnorderedAccessView(voxel_data_structured_buffer.Get(), nullptr, &voxelDataStructuredBufferUAVDesc, cbv_srv_uav_cpu_handle);
	cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);
#pragma endregion

#pragma region Radiance 3D Texture
	/*
	D3D12_RESOURCE_DESC radiance3DTextureDesc = {};
	radiance3DTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	radiance3DTextureDesc.Width = voxel_grid_data.res;
	radiance3DTextureDesc.Height = voxel_grid_data.res;
	radiance3DTextureDesc.DepthOrArraySize = voxel_grid_data.res;
	radiance3DTextureDesc.MipLevels = (UINT32)log2(voxel_grid_data.res) + 1;
	radiance3DTextureDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT;
	radiance3DTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	radiance3DTextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	radiance3DTextureDesc.Alignment = 0;
	radiance3DTextureDesc.SampleDesc.Count = 1;
	radiance3DTextureDesc.SampleDesc.Quality = 0;

	for (int frameIndex = 0; frameIndex < c_frame_count; frameIndex++)
	{
		ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&radiance3DTextureDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&per_frame_radiance_texture_3d[frameIndex])));



		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.MipSlice = 0;
		uavDesc.Texture3D.FirstWSlice = 0;
		uavDesc.Texture3D.WSize = -1;

		d3dDevice->CreateUnorderedAccessView(
			per_frame_radiance_texture_3d[frameIndex].Get(),
			nullptr,
			&uavDesc,
			cbv_srv_uav_cpu_handle);

		cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);
	}
	*/
#pragma endregion

#pragma region Voxel Grid Data Constant Buffer
	// Create the camera view projection constant buffer
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(c_frame_count * c_aligned_shader_structure_cpu_voxel_grid_data),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&voxel_grid_data_constant_buffer)));


	NAME_D3D12_OBJECT(voxel_grid_data_constant_buffer);

	// Create constant buffer views to access the upload buffer.
	D3D12_GPU_VIRTUAL_ADDRESS voxelGridDataConstantBufferGPUAddress = voxel_grid_data_constant_buffer->GetGPUVirtualAddress();
	for (int i = 0; i < c_frame_count; i++)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.BufferLocation = voxelGridDataConstantBufferGPUAddress;
		desc.SizeInBytes = c_aligned_shader_structure_cpu_voxel_grid_data;
		d3dDevice->CreateConstantBufferView(&desc, cbv_srv_uav_cpu_handle);

		voxelGridDataConstantBufferGPUAddress += desc.SizeInBytes;
		cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);
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

#pragma region Camera View Projection Buffer
	// Create the camera view projection constant buffer
	;
	CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(c_frame_count * c_aligned_view_projection_matrix_constant_buffer);
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&constantBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&camera_view_projection_constant_buffer)));


	NAME_D3D12_OBJECT(camera_view_projection_constant_buffer);



	// Create constant buffer views to access the upload buffer.
	D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = camera_view_projection_constant_buffer->GetGPUVirtualAddress();



	for (int n = 0; n < c_frame_count; n++)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.BufferLocation = cbvGpuAddress;
		desc.SizeInBytes = c_aligned_view_projection_matrix_constant_buffer;
		d3dDevice->CreateConstantBufferView(&desc, cbv_srv_uav_cpu_handle);

		cbvGpuAddress += desc.SizeInBytes;
		cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);
	}

	// Map the constant buffers.
	// We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.
	ThrowIfFailed(camera_view_projection_constant_buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&camera_view_projection_constant_mapped_buffer)));
	std::ZeroMemory(camera_view_projection_constant_mapped_buffer, c_frame_count * c_aligned_view_projection_matrix_constant_buffer);
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
	d3dDevice->CreateConstantBufferView(&cbvDesc, cbv_srv_uav_cpu_handle);
	cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);


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
	for (int i = 0; i < c_frame_count; i++)
	{
		per_frame_indirect_draw_required_voxel_debug_data_upload_buffer[i].Reset();
		per_frame_indirect_command_upload_buffer[i].Reset();
	}

	// Create the vertex and index buffer views for the voxel debug cube
	// Create vertex/index views
	voxel_debug_cube.vertex_buffer_view.BufferLocation = voxel_debug_cube.vertex_buffer->GetGPUVirtualAddress();
	voxel_debug_cube.vertex_buffer_view.StrideInBytes = sizeof(ShaderStructureCPUVertexPositionNormalTextureColor);
	voxel_debug_cube.vertex_buffer_view.SizeInBytes = voxel_debug_cube.vertices.size() * sizeof(ShaderStructureCPUVertexPositionNormalTextureColor);

	voxel_debug_cube.index_buffer_view.BufferLocation = voxel_debug_cube.index_buffer->GetGPUVirtualAddress();
	voxel_debug_cube.index_buffer_view.SizeInBytes = voxel_debug_cube.indices.size() * sizeof(uint16_t);
	voxel_debug_cube.index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
}

// Initializes view parameters when the window size changes.
void SceneRenderer3D::CreateWindowSizeDependentResources(Camera& camera)
{
	D3D12_VIEWPORT viewport = device_resources->GetScreenViewport();
	scissor_rect_default = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height)};
	XMFLOAT4X4 orientation = device_resources->GetOrientationTransform3D();
	XMMATRIX orientationMatrix = DirectX::XMLoadFloat4x4(&orientation);
	DirectX::XMStoreFloat4x4(&camera_view_projection_constant_buffer_data.projection, XMMatrixTranspose(camera.GetProjectionMatrix() * orientationMatrix));
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

void SceneRenderer3D::AssignDescriptors(ID3D12GraphicsCommandList* _commandList, bool isComputeCommandList, UINT currentFrameIndex)
{
	// Bind the descriptor tables to their respective heap starts
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(descriptor_heap_cbv_srv_uav->GetGPUDescriptorHandleForHeapStart(), 0, cbv_srv_uav_descriptor_size);
	if (isComputeCommandList == false)
	{
		// SRV test texture
		_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);
		gpuHandle.Offset(cbv_srv_uav_descriptor_size);
		// Voxel debug data SRV
		_commandList->SetGraphicsRootDescriptorTable(2, gpuHandle);
		gpuHandle.Offset(cbv_srv_uav_descriptor_size + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		//gpuHandle.Offset(cbv_srv_uav_descriptor_size);
		// Required voxel debug data for frame draw UAV
		_commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);
		gpuHandle.Offset(((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size) + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		// Indirect command
		_commandList->SetGraphicsRootDescriptorTable(4, gpuHandle);
		gpuHandle.Offset((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size);
		// Voxel Data RW Buffer UAV
		_commandList->SetGraphicsRootDescriptorTable(5, gpuHandle);
		gpuHandle.Offset(cbv_srv_uav_descriptor_size + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		// Radiance Texture 3D UAV
		//_commandList->SetGraphicsRootDescriptorTable(6, gpuHandle);
		//gpuHandle.Offset(((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size) + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		// Voxel Grid Data CBV
		_commandList->SetGraphicsRootDescriptorTable(7, gpuHandle);
		gpuHandle.Offset(((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size) + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		// View Projection CBV
		_commandList->SetGraphicsRootDescriptorTable(8, gpuHandle);
		//gpuHandle.Offset(((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size) + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		gpuHandle.Offset((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size);
		// Transform buffers CBV
		_commandList->SetGraphicsRootDescriptorTable(9, gpuHandle);
	}
	else
	{
		// SRV test texture
		_commandList->SetComputeRootDescriptorTable(1, gpuHandle);
		gpuHandle.Offset(cbv_srv_uav_descriptor_size);
		// Voxel debug data SRV
		_commandList->SetComputeRootDescriptorTable(2, gpuHandle);
		gpuHandle.Offset(cbv_srv_uav_descriptor_size + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		//gpuHandle.Offset(cbv_srv_uav_descriptor_size);
		// Required voxel debug data for frame draw UAV
		_commandList->SetComputeRootDescriptorTable(3, gpuHandle);
		gpuHandle.Offset(((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size) + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		// Indirect command
		_commandList->SetComputeRootDescriptorTable(4, gpuHandle);
		gpuHandle.Offset((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size);
		// Voxel Data RW Buffer UAV
		_commandList->SetComputeRootDescriptorTable(5, gpuHandle);
		gpuHandle.Offset(cbv_srv_uav_descriptor_size + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		// Radiance Texture 3D UAV
		//_commandList->SetComputeRootDescriptorTable(6, gpuHandle);
		//gpuHandle.Offset(((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size) + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		// Voxel Grid Data CBV
		_commandList->SetComputeRootDescriptorTable(7, gpuHandle);
		gpuHandle.Offset(((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size) + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		// View Projection CBV
		_commandList->SetComputeRootDescriptorTable(8, gpuHandle);
		//gpuHandle.Offset(((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size) + (currentFrameIndex * cbv_srv_uav_descriptor_size));
		gpuHandle.Offset((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size);
		// Transform buffers CBV
		_commandList->SetComputeRootDescriptorTable(9, gpuHandle);
	}
}

// Renders one frame using the vertex and pixel shaders.
ID3D12GraphicsCommandList* SceneRenderer3D::Render(vector<Mesh>& scene, Camera& camera, bool voxelDebugVisualization)
{
	UINT currentFrameIndex = device_resources->GetCurrentFrameIndex();
	// Update the view matrix
	DirectX::XMStoreFloat4x4(&camera_view_projection_constant_buffer_data.view, XMMatrixTranspose(camera.GetViewMatrix()));
	// Update the mapped viewProjection constant buffer
	UINT8* destination = camera_view_projection_constant_mapped_buffer + (currentFrameIndex * c_aligned_view_projection_matrix_constant_buffer);
	std::memcpy(destination, &camera_view_projection_constant_buffer_data, sizeof(camera_view_projection_constant_buffer_data));

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
					ThrowIfFailed(command_list_copy_high_priority->Reset(device_resources->GetCommandAllocatorCopyHighPriority(), pipeline_state_default.Get()));
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
							device_resources->GetD3DDevice()->CreateConstantBufferView(&cbvDesc, cbv_srv_uav_cpu_handle);
							cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);


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
					ThrowIfFailed(command_list_copy_normal_priority->Reset(device_resources->GetCommandAllocatorCopyNormalPriority(), pipeline_state_default.Get()));
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


#pragma region Voxelizer Render Pass
	ThrowIfFailed(device_resources->GetCommandAllocatorDirect()->Reset());
	ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_voxelizer.Get()));
	command_list_direct->SetGraphicsRootSignature(root_signature.Get());
	ID3D12DescriptorHeap* ppHeaps[] = { descriptor_heap_cbv_srv_uav.Get() };
	command_list_direct->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	AssignDescriptors(command_list_direct.Get(), false, currentFrameIndex);
	command_list_direct->RSSetViewports(1, &viewport_voxelizer);
	command_list_direct->RSSetScissorRects(1, &scissor_rect_voxelizer);
	command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// Indicate this resource will be in use as a render target.

	CD3DX12_RESOURCE_BARRIER voxelizaerRenderPassBarriers[2] = {
		CD3DX12_RESOURCE_BARRIER::Transition(device_resources->GetRenderTarget(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(voxel_data_structured_buffer.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	};


	//CD3DX12_RESOURCE_BARRIER voxelizaerRenderPassBarriers[2];
	//voxelizaerRenderPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(voxel_data_structured_buffer.Get(),
											//D3D12_RESOURCE_STATE_COMMON,
												//D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	command_list_direct->ResourceBarrier(2, voxelizaerRenderPassBarriers);
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

	voxelizaerRenderPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(voxel_data_structured_buffer.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON);
	command_list_direct->ResourceBarrier(1, voxelizaerRenderPassBarriers);
	command_list_direct->Close();
	ID3D12CommandList* ppCommandLists[] = { command_list_direct.Get() };
	//device_resources->GetCommandQueueDirect()->Wait(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value - 1);
	device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	device_resources->GetCommandQueueDirect()->Signal(fence_command_list_direct_progress.Get(), fence_command_list_direct_progress_latest_unused_value);
	fence_command_list_direct_progress_latest_unused_value++;
#pragma endregion

#pragma region Radiance Temporal Copy Clear Render Pass
	ThrowIfFailed(device_resources->GetCommandAllocatorCompute()->Reset());
	ThrowIfFailed(command_list_compute->Reset(device_resources->GetCommandAllocatorCompute(), pipeline_state_radiance_temporal_clear.Get()));
	command_list_compute->SetComputeRootSignature(root_signature.Get());
	ID3D12DescriptorHeap* ppHeapsRadianceTemporalCopyClearPass[] = { descriptor_heap_cbv_srv_uav.Get() };
	command_list_compute->SetDescriptorHeaps(_countof(ppHeapsRadianceTemporalCopyClearPass), ppHeapsRadianceTemporalCopyClearPass);
	AssignDescriptors(command_list_compute.Get(), true, currentFrameIndex);

	D3D12_RESOURCE_BARRIER perFrameIndirectProcesedCommandsBufferBarriers[3] = {
		CD3DX12_RESOURCE_BARRIER::Transition(per_frame_indirect_draw_required_voxel_debug_data_buffer[currentFrameIndex].Get(),
												D3D12_RESOURCE_STATE_COMMON,
													D3D12_RESOURCE_STATE_COPY_DEST),
		CD3DX12_RESOURCE_BARRIER::Transition(per_frame_indirect_command_buffer[currentFrameIndex].Get(),
												D3D12_RESOURCE_STATE_COMMON,
													D3D12_RESOURCE_STATE_COPY_DEST),
		CD3DX12_RESOURCE_BARRIER::Transition(voxel_data_structured_buffer.Get(),
												D3D12_RESOURCE_STATE_COMMON,
													D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	};

	command_list_compute->ResourceBarrier(_countof(perFrameIndirectProcesedCommandsBufferBarriers), perFrameIndirectProcesedCommandsBufferBarriers);

	command_list_compute->CopyBufferRegion(per_frame_indirect_draw_required_voxel_debug_data_buffer[currentFrameIndex].Get(), indirect_draw_required_voxel_debug_data_counter_offset, indirect_draw_required_voxel_debug_data_counter_reset_buffer.Get(), 0, sizeof(UINT));
	command_list_compute->CopyBufferRegion(per_frame_indirect_command_buffer[currentFrameIndex].Get(), sizeof(UINT), indirect_draw_required_voxel_debug_data_counter_reset_buffer.Get(), 0, sizeof(UINT));

	perFrameIndirectProcesedCommandsBufferBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(per_frame_indirect_draw_required_voxel_debug_data_buffer[currentFrameIndex].Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	perFrameIndirectProcesedCommandsBufferBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(per_frame_indirect_command_buffer[currentFrameIndex].Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	command_list_compute->ResourceBarrier(2, perFrameIndirectProcesedCommandsBufferBarriers);

	command_list_compute->Dispatch((number_of_voxels_in_grid / 256), 1, 1);

	perFrameIndirectProcesedCommandsBufferBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(per_frame_indirect_draw_required_voxel_debug_data_buffer[currentFrameIndex].Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON);
	perFrameIndirectProcesedCommandsBufferBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(per_frame_indirect_command_buffer[currentFrameIndex].Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON);

	perFrameIndirectProcesedCommandsBufferBarriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(voxel_data_structured_buffer.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON);

	command_list_compute->ResourceBarrier(_countof(perFrameIndirectProcesedCommandsBufferBarriers), perFrameIndirectProcesedCommandsBufferBarriers);
	command_list_compute->Close();

	ID3D12CommandList* ppCommandListsCompute[] = { command_list_compute.Get() };
	device_resources->GetCommandQueueCompute()->Wait(fence_command_list_direct_progress.Get(), fence_command_list_direct_progress_latest_unused_value - 1);
	device_resources->GetCommandQueueCompute()->ExecuteCommandLists(_countof(ppCommandListsCompute), ppCommandListsCompute);
	device_resources->GetCommandQueueCompute()->Signal(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value);
	fence_command_list_compute_progress_latest_unused_value++;
#pragma endregion

#pragma region Voxel Debug Visualization Pass
	if (voxelDebugVisualization == true)
	{
		ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_voxel_debug_visualization.Get()));
		command_list_direct->SetGraphicsRootSignature(root_signature.Get());
		ID3D12DescriptorHeap* ppHeapsVoxelDebugViewPass[] = { descriptor_heap_cbv_srv_uav.Get() };
		command_list_direct->SetDescriptorHeaps(_countof(ppHeapsVoxelDebugViewPass), ppHeapsVoxelDebugViewPass);
		AssignDescriptors(command_list_direct.Get(), false, currentFrameIndex);
		command_list_direct->RSSetViewports(1, &device_resources->GetScreenViewport());
		command_list_direct->RSSetScissorRects(1, &scissor_rect_default);
		// Bind the render target and depth buffer
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = device_resources->GetRenderTargetView();
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = device_resources->GetDepthStencilView();
		command_list_direct->ClearRenderTargetView(renderTargetView, DirectX::Colors::CornflowerBlue, 0, nullptr);
		command_list_direct->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		command_list_direct->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);
		command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		command_list_direct->IASetVertexBuffers(0, 1, &voxel_debug_cube.vertex_buffer_view);
		command_list_direct->IASetIndexBuffer(&voxel_debug_cube.index_buffer_view);


		// Indicate that the command buffer will be used for indirect drawing
		// and that the back buffer will be used as a render target.
		D3D12_RESOURCE_BARRIER voxelDebugVisualizationPassBarriers[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				per_frame_indirect_draw_required_voxel_debug_data_buffer[currentFrameIndex].Get(),
				D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(
				per_frame_indirect_command_buffer[currentFrameIndex].Get(),
				D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
			//CD3DX12_RESOURCE_BARRIER::Transition(device_resources->GetRenderTarget(), 
			//D3D12_RESOURCE_STATE_PRESENT, 
			//D3D12_RESOURCE_STATE_RENDER_TARGET)
		};

		command_list_direct->ResourceBarrier(_countof(voxelDebugVisualizationPassBarriers), voxelDebugVisualizationPassBarriers);

		command_list_direct->ExecuteIndirect(
			voxel_debug_command_signature.Get(),
			1,
			per_frame_indirect_command_buffer[currentFrameIndex].Get(),
			0,
			nullptr,
			0);

		voxelDebugVisualizationPassBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(per_frame_indirect_draw_required_voxel_debug_data_buffer[currentFrameIndex].Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_COMMON);
		voxelDebugVisualizationPassBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(per_frame_indirect_command_buffer[currentFrameIndex].Get(),
			D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
			D3D12_RESOURCE_STATE_COMMON);


		command_list_direct->ResourceBarrier(2, voxelDebugVisualizationPassBarriers);
		command_list_direct->Close();
		ID3D12CommandList* ppCommandListsIndirectExecute[] = { command_list_direct.Get() };
		device_resources->GetCommandQueueDirect()->Wait(fence_command_list_compute_progress.Get(), fence_command_list_compute_progress_latest_unused_value - 1);
		device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(ppCommandListsIndirectExecute), ppCommandListsIndirectExecute);
		device_resources->GetCommandQueueDirect()->Signal(fence_command_list_direct_progress.Get(), fence_command_list_direct_progress_latest_unused_value);
		fence_command_list_direct_progress_latest_unused_value++;

		goto LABEL_SKIP_ALL_RENDER_PASSES;
	}
#pragma endregion
	


	LABEL_SKIP_ALL_RENDER_PASSES:

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
		ThrowIfFailed(fence_command_list_copy_high_priority_progress->SetEventOnCompletion(fence_per_frame_command_list_copy_high_priority_values[previous_frame_index], event_command_list_copy_high_priority_progress));
		WaitForSingleObjectEx(event_command_list_copy_high_priority_progress, INFINITE, FALSE);
	}

	previous_frame_index = currentFrameIndex;


	ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_default.Get()));
	command_list_direct->SetGraphicsRootSignature(root_signature.Get());
	command_list_direct->RSSetViewports(1, &device_resources->GetScreenViewport());
	command_list_direct->RSSetScissorRects(1, &scissor_rect_default);
	// Bind the render target and depth buffer
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = device_resources->GetRenderTargetView();
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = device_resources->GetDepthStencilView();
	command_list_direct->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);
	command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	return command_list_direct.Get();
}
