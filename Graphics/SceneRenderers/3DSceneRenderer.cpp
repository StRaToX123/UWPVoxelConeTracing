#include "Graphics/SceneRenderers/3DSceneRenderer.h"




// Indices into the application state map.
Platform::String^ AngleKey = "Angle";
Platform::String^ TrackingKey = "Tracking";

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
SceneRenderer3D::SceneRenderer3D(const std::shared_ptr<DeviceResources>& deviceResources, Camera& camera) :
	device_resources(deviceResources),
	camera_view_projection_constant_mapped_buffer(nullptr),
	fence_command_list_copy_normal_priority_progress_latest_unused_value(1),
	fence_command_list_copy_high_priority_progress_latest_unused_value(1),
	command_allocator_copy_normal_priority_already_reset(true),
	command_list_copy_normal_priority_requires_reset(false),
	command_allocator_copy_high_priority_already_reset(true),
	command_list_copy_high_priority_requires_reset(false),
	event_command_list_copy_high_priority_progress(NULL),
	previous_frame_index(c_frame_count - 1)
{
	voxel_grid_allowed_resolutions[0] = 32;
	voxel_grid_allowed_resolutions[1] = 64;
	voxel_grid_allowed_resolutions[2] = 128;
	voxel_grid_allowed_resolutions[3] = 256;

	voxel_debug_cube.InitializeAsCube();

	LoadState();
	ZeroMemory(&camera_view_projection_constant_buffer_data, sizeof(camera_view_projection_constant_buffer_data));

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
	CD3DX12_DESCRIPTOR_RANGE rangeSRVIndirectCommands;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVIndirectCommands;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVVoxelDataStructuredBuffer;
	CD3DX12_DESCRIPTOR_RANGE rangeUAVRadianceTexture3D;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVviewProjectionMatrix;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVTransformMatrix;
	
	rangeSRVTestTexture.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	rangeSRVIndirectCommands.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
	rangeUAVIndirectCommands.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
	rangeUAVVoxelDataStructuredBuffer.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
	rangeUAVRadianceTexture3D.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
	rangeCBVviewProjectionMatrix.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // register is 1 because register 0 is taken by the root constants
	rangeCBVTransformMatrix.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);

	CD3DX12_ROOT_PARAMETER parameterRootConstants; // contains transformMatrixBufferIndex, transformMatrixBufferInternalIndex
	CD3DX12_ROOT_PARAMETER parameterPixelVisibleDescriptorTableTestTexture;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableIndirectCommandsSRV;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableIndirectCommandsUAV;
	CD3DX12_ROOT_PARAMETER parameterAllVisibleDescriptorTableVoxelDataStructuredBufferUAV;
	CD3DX12_ROOT_PARAMETER parameterComputeVisibleDescriptorTableRadianceTexture3D;
	CD3DX12_ROOT_PARAMETER parameterGeometryVisibleDescriptorTableViewProjectionMatrixBuffer;
	CD3DX12_ROOT_PARAMETER parameterGeometryVisibleDescriptorTableTransformMatrixBuffers;

	parameterRootConstants.InitAsConstants(2, 0, 0, D3D12_SHADER_VISIBILITY_GEOMETRY);
	parameterPixelVisibleDescriptorTableTestTexture.InitAsDescriptorTable(1, &rangeSRVTestTexture, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableIndirectCommandsSRV.InitAsDescriptorTable(1, &rangeSRVIndirectCommands, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableIndirectCommandsUAV.InitAsDescriptorTable(1, &rangeUAVIndirectCommands, D3D12_SHADER_VISIBILITY_ALL);
	parameterAllVisibleDescriptorTableVoxelDataStructuredBufferUAV.InitAsDescriptorTable(1, &rangeUAVVoxelDataStructuredBuffer, D3D12_SHADER_VISIBILITY_ALL);
	parameterComputeVisibleDescriptorTableRadianceTexture3D.InitAsDescriptorTable(1, &rangeUAVRadianceTexture3D, D3D12_SHADER_VISIBILITY_ALL);
	parameterGeometryVisibleDescriptorTableViewProjectionMatrixBuffer.InitAsDescriptorTable(1, &rangeCBVviewProjectionMatrix, D3D12_SHADER_VISIBILITY_GEOMETRY);
	parameterGeometryVisibleDescriptorTableTransformMatrixBuffers.InitAsDescriptorTable(1, &rangeCBVTransformMatrix, D3D12_SHADER_VISIBILITY_GEOMETRY);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // Only the input assembler stage needs access to the constant buffer.
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
	CD3DX12_ROOT_PARAMETER parameterArray[8] = { parameterRootConstants,
		parameterPixelVisibleDescriptorTableTestTexture,
		parameterComputeVisibleDescriptorTableIndirectCommandsSRV,
		parameterComputeVisibleDescriptorTableIndirectCommandsUAV,
		parameterAllVisibleDescriptorTableVoxelDataStructuredBufferUAV,
		parameterComputeVisibleDescriptorTableRadianceTexture3D,
		parameterGeometryVisibleDescriptorTableViewProjectionMatrixBuffer,
		parameterGeometryVisibleDescriptorTableTransformMatrixBuffers };
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

	descRootSignature.Init(8, parameterArray, 1, &staticSamplerDesc, rootSignatureFlags);

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

#pragma region Default Pipeline State
	// Load shaders
	// Get the install folder full path
	wstring wStringInstallPath(Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data());
	string standardStringInstallPath(wStringInstallPath.begin(), wStringInstallPath.end());
	// Load the vertex shader
	char* pVertexShadeByteCode;
	int vertexShaderByteCodeLength;
	LoadShaderByteCode((standardStringInstallPath + "\\SampleVertexShader.cso").c_str(), pVertexShadeByteCode, vertexShaderByteCodeLength);
	// Load the pixel shader
	char* pPixelShadeByteCode;
	int pixelShaderByteCodeLength;
	LoadShaderByteCode((standardStringInstallPath + "\\SampleVertexShader.cso").c_str(), pPixelShadeByteCode, pixelShaderByteCodeLength);


	// Create the pipeline state once the shaders are loaded.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
	state.InputLayout = { ShaderStructureCPUVertexPositionNormalTexture::input_elements, ShaderStructureCPUVertexPositionNormalTexture::input_element_count };
	state.pRootSignature = root_signature.Get();
	state.VS = CD3DX12_SHADER_BYTECODE(pVertexShadeByteCode, vertexShaderByteCodeLength);
	state.PS = CD3DX12_SHADER_BYTECODE(pPixelShadeByteCode, pixelShaderByteCodeLength);
	state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	state.SampleMask = UINT_MAX;
	state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	state.NumRenderTargets = 1;
	state.RTVFormats[0] = device_resources->GetBackBufferFormat();
	state.DSVFormat = device_resources->GetDepthBufferFormat();
	state.SampleDesc.Count = 1;

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipeline_state_default)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pVertexShadeByteCode;
	delete[] pPixelShadeByteCode;
#pragma endregion

#pragma region Voxelizer Pipeline State
	// Load the geometry shader
	char* pGeometryShaderByteCode;
	int geometryShaderByteCodeLength;
	LoadShaderByteCode((standardStringInstallPath + "\\Voxelizer_GS.cso").c_str(), pGeometryShaderByteCode, geometryShaderByteCodeLength);
	//LoadShaderByteCode((standardStringInstallPath + "\\Voxelizer_VS.cso").c_str(), pVertexShadeByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\Voxelizer_PS.cso").c_str(), pPixelShadeByteCode, pixelShaderByteCodeLength);

	// Create the pipeline state once the shaders are loaded.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC voxelizerState = {};
	voxelizerState.InputLayout = { ShaderStructureCPUVertexPositionNormalColor::input_elements, ShaderStructureCPUVertexPositionNormalColor::input_element_count };
	voxelizerState.pRootSignature = root_signature.Get();
	voxelizerState.GS = CD3DX12_SHADER_BYTECODE(pGeometryShaderByteCode, geometryShaderByteCodeLength);
	//voxelizerState.VS = CD3DX12_SHADER_BYTECODE(pVertexShadeByteCode, vertexShaderByteCodeLength);
	voxelizerState.PS = CD3DX12_SHADER_BYTECODE(pPixelShadeByteCode, pixelShaderByteCodeLength);
	D3D12_RASTERIZER_DESC rasterizerDesc;
	rasterizerDesc.FillMode = D3D12_FILL_MODE::D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_NONE;
	rasterizerDesc.FrontCounterClockwise = true;
	rasterizerDesc.DepthBias = 0;
	rasterizerDesc.DepthBiasClamp = 0;
	rasterizerDesc.SlopeScaledDepthBias = 0;
	rasterizerDesc.DepthClipEnable = true;
	rasterizerDesc.MultisampleEnable = false;
	rasterizerDesc.AntialiasedLineEnable = false;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE::D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	rasterizerDesc.ForcedSampleCount = 8;
	voxelizerState.RasterizerState = rasterizerDesc;
	voxelizerState.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc;
	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.StencilEnable = false;
	voxelizerState.DepthStencilState = depthStencilDesc;
	voxelizerState.SampleMask = UINT_MAX;
	voxelizerState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	voxelizerState.NumRenderTargets = 1;
	//state.RTVFormats[0] = device_resources->GetBackBufferFormat();
	//state.DSVFormat = device_resources->GetDepthBufferFormat();
	voxelizerState.SampleDesc.Count = 1;

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipeline_state_voxelizer)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pGeometryShaderByteCode;
	delete[] pVertexShadeByteCode;
	delete[] pPixelShadeByteCode;
#pragma endregion

#pragma region Voxel Radiance Smooth Temporal Copy Clear Pipeline State
	// Load the geometry shader
	char* pComputeShaderByteCode;
	int computeShaderByteCodeLength;
	LoadShaderByteCode((standardStringInstallPath + "\\RadianceTemporalCopyClear_CS.cso").c_str(), pComputeShaderByteCode, computeShaderByteCodeLength);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc;
	computePipelineStateDesc.pRootSignature = root_signature.Get();
	computePipelineStateDesc.CS = CD3DX12_SHADER_BYTECODE(pComputeShaderByteCode, computeShaderByteCodeLength);
	computePipelineStateDesc.NodeMask = NULL;
	computePipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&pipeline_state_radiance_temporal_clear)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pComputeShaderByteCode;
#pragma endregion

#pragma region Voxel Debug View Pipeline
	// Load shaders
	// Get the install folder full path
	wstring wStringInstallPath(Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data());
	string standardStringInstallPath(wStringInstallPath.begin(), wStringInstallPath.end());
	// Load the vertex shader
	LoadShaderByteCode((standardStringInstallPath + "\\VoxelDebugDraw_VS.cso").c_str(), pVertexShadeByteCode, vertexShaderByteCodeLength);
	// Load the pixel shader
	LoadShaderByteCode((standardStringInstallPath + "\\VoxelDebugDraw_PS.cso").c_str(), pPixelShadeByteCode, pixelShaderByteCodeLength);


	// Create the pipeline state once the shaders are loaded.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
	state.InputLayout = { ShaderStructureCPUVertexPositionNormalTexture::input_elements, ShaderStructureCPUVertexPositionNormalTexture::input_element_count };
	state.pRootSignature = root_signature.Get();
	state.VS = CD3DX12_SHADER_BYTECODE(pVertexShadeByteCode, vertexShaderByteCodeLength);
	state.PS = CD3DX12_SHADER_BYTECODE(pPixelShadeByteCode, pixelShaderByteCodeLength);
	state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	state.SampleMask = UINT_MAX;
	state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	state.NumRenderTargets = 1;
	state.RTVFormats[0] = device_resources->GetBackBufferFormat();
	state.DSVFormat = device_resources->GetDepthBufferFormat();
	state.SampleDesc.Count = 1;

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipeline_state_default)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pVertexShadeByteCode;
	delete[] pPixelShadeByteCode;
#pragma endregion


	// Create a direct command list.
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, device_resources->GetCommandAllocatorDirect(), pipeline_state_default.Get(), IID_PPV_ARGS(&command_list_direct)));
	NAME_D3D12_OBJECT(command_list_direct);
	// Create copy command lists.
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, device_resources->GetCommandAllocatorCopyNormalPriority(), pipeline_state_default.Get(), IID_PPV_ARGS(&command_list_copy_normal_priority)));
	NAME_D3D12_OBJECT(command_list_copy_normal_priority);
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, device_resources->GetCommandAllocatorCopyHighPriority(), pipeline_state_default.Get(), IID_PPV_ARGS(&command_list_copy_high_priority)));
	NAME_D3D12_OBJECT(command_list_copy_high_priority);
	// Create compute command list.
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, device_resources->GetCommandAllocatorCompute(), pipeline_state_radiance_temporal_clear.Get(), IID_PPV_ARGS(&command_list_compute)));
	NAME_D3D12_OBJECT(command_list_copy_normal_priority);
	// Create a fences to go along with the copy command lists.
	// This fence will be used to signal when a resource has been successfully uploaded to the gpu
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_copy_normal_priority_progress)));
	NAME_D3D12_OBJECT(fence_command_list_copy_normal_priority_progress);
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_copy_high_priority_progress)));
	NAME_D3D12_OBJECT(fence_command_list_copy_high_priority_progress);

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
	ThrowIfFailed(LoadFromWICFile((wStringInstallPath + L"\\Assets\\woah.jpg").c_str(), WIC_FLAGS::WIC_FLAGS_NONE, &metadata, scratchImage));
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
	command_list_direct->ResourceBarrier(1, &testTextureBarrier);

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


#pragma region Indirect Execute
	// Create a constant buffer that holds all the model transform matrixes for each possible voxel position
	UINT maxPossibleVoxelGridResolution = voxel_grid_allowed_resolutions[_countof(voxel_grid_allowed_resolutions) - 1];
	UINT64 maxNumberOfVoxels = maxPossibleVoxelGridResolution * maxPossibleVoxelGridResolution * maxPossibleVoxelGridResolution;
	const UINT constantBufferDataSize = maxNumberOfVoxels * sizeof(ShaderStructureCPUVoxelDebugData);

	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(constantBufferDataSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&voxel_debug_constant_buffer)));

	NAME_D3D12_OBJECT(voxel_debug_constant_buffer);

	// Map and initialize the constant buffer.
	ShaderStructureCPUVoxelDebugData* pVoxelDebugConstantMappedBuffer;
	ThrowIfFailed(voxel_debug_constant_buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pVoxelDebugConstantMappedBuffer)));
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	float offsetZ = 0.0f;
	for (int i = 0; i < maxNumberOfVoxels; i++)
	{
		(pVoxelDebugConstantMappedBuffer + i)->voxel_index = i;
		XMStoreFloat4x4(&(pVoxelDebugConstantMappedBuffer + i)->debug_cube_model_transform_matrix, XMMatrixTranslation()
	}

	memcpy(m_pCbvDataBegin, &m_constantBufferData[0], TriangleCount * sizeof(SceneConstantBuffer));

	// Create shader resource views (SRV) of the constant buffers for the
	// compute shader to read from.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = TriangleCount;
	srvDesc.Buffer.StructureByteStride = sizeof(SceneConstantBuffer);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), CbvSrvOffset, m_cbvSrvUavDescriptorSize);
	for (UINT frame = 0; frame < FrameCount; frame++)
	{
		srvDesc.Buffer.FirstElement = frame * TriangleCount;
		m_device->CreateShaderResourceView(m_constantBuffer.Get(), &srvDesc, cbvSrvHandle);
		cbvSrvHandle.Offset(CbvSrvUavDescriptorCountPerFrame, m_cbvSrvUavDescriptorSize);
	}
	// Create the indirect command signature
	// Each command consists of a CBV update and a DrawInstanced call.
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[2] = {};
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
	argumentDescs[0].ConstantBufferView.RootParameterIndex = 7;
	argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;


	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof(IndirectCommand);

	ThrowIfFailed(d3dDevice->CreateCommandSignature(&commandSignatureDesc, root_signature.Get(), IID_PPV_ARGS(&voxel_debug_command_signature)));
	NAME_D3D12_OBJECT(voxel_debug_command_signature);

	// Create the command buffers and UAVs to store the results of the compute work.
	std::vector<IndirectCommand> commands;
	commands.resize(maxNumberOfVoxels);
	const UINT commandBufferSize = sizeof(IndirectCommand) * maxNumberOfVoxels;
	D3D12_RESOURCE_DESC commandBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(commandBufferSize);
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&commandBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&indirect_command_buffer)));

	NAME_D3D12_OBJECT(indirect_command_buffer);

	Microsoft::WRL::ComPtr<ID3D12Resource> indirectCommandUploadBuffer;
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(commandBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indirectCommandUploadBuffer)));

	NAME_D3D12_OBJECT(indirectCommandUploadBuffer);

	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = indirect_command_buffer->GetGPUVirtualAddress();
	UINT commandIndex = 0;
	for (UINT n = 0; n < maxNumberOfVoxels; n++)
	{
		commands[commandIndex].cbv = gpuAddress;
		commands[commandIndex].drawArguments.VertexCountPerInstance = voxel_debug_cube.vertices.size();
		commands[commandIndex].drawArguments.InstanceCount = 1;
		commands[commandIndex].drawArguments.StartVertexLocation = 0;
		commands[commandIndex].drawArguments.StartInstanceLocation = 0;

		commandIndex++;
		gpuAddress += sizeof(SceneConstantBuffer);
	}
	

	// Copy data to the intermediate upload heap and then schedule a copy
	// from the upload heap to the command buffer.
	D3D12_SUBRESOURCE_DATA commandData = {};
	commandData.pData = reinterpret_cast<UINT8*>(&commands[0]);
	commandData.RowPitch = commandBufferSize;
	commandData.SlicePitch = commandData.RowPitch;

	UpdateSubresources<1>(m_commandList.Get(), m_commandBuffer.Get(), commandBufferUpload.Get(), 0, 0, 1, &commandData);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_commandBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	// Create SRVs for the command buffers.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = TriangleCount;
	srvDesc.Buffer.StructureByteStride = sizeof(IndirectCommand);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	CD3DX12_CPU_DESCRIPTOR_HANDLE commandsHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), CommandsOffset, m_cbvSrvUavDescriptorSize);
	for (UINT frame = 0; frame < FrameCount; frame++)
	{
		srvDesc.Buffer.FirstElement = frame * TriangleCount;
		m_device->CreateShaderResourceView(m_commandBuffer.Get(), &srvDesc, commandsHandle);
		commandsHandle.Offset(CbvSrvUavDescriptorCountPerFrame, m_cbvSrvUavDescriptorSize);
	}

	// Create the unordered access views (UAVs) that store the results of the compute work.
	CD3DX12_CPU_DESCRIPTOR_HANDLE processedCommandsHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), ProcessedCommandsOffset, m_cbvSrvUavDescriptorSize);
	for (UINT frame = 0; frame < FrameCount; frame++)
	{
		// Allocate a buffer large enough to hold all of the indirect commands
		// for a single frame as well as a UAV counter.
		commandBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(CommandBufferCounterOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&commandBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_processedCommandBuffers[frame])));

		NAME_D3D12_OBJECT_INDEXED(m_processedCommandBuffers, frame);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = TriangleCount;
		uavDesc.Buffer.StructureByteStride = sizeof(IndirectCommand);
		uavDesc.Buffer.CounterOffsetInBytes = CommandBufferCounterOffset;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		m_device->CreateUnorderedAccessView(
			m_processedCommandBuffers[frame].Get(),
			m_processedCommandBuffers[frame].Get(),
			&uavDesc,
			processedCommandsHandle);

		processedCommandsHandle.Offset(CbvSrvUavDescriptorCountPerFrame, m_cbvSrvUavDescriptorSize);
	}

	// Allocate a buffer that can be used to reset the UAV counters and initialize
	// it to 0.
	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_processedCommandBufferCounterReset)));

	UINT8* pMappedCounterReset = nullptr;
	CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
	ThrowIfFailed(m_processedCommandBufferCounterReset->Map(0, &readRange, reinterpret_cast<void**>(&pMappedCounterReset)));
	ZeroMemory(pMappedCounterReset, sizeof(UINT));
	m_processedCommandBufferCounterReset->Unmap(0, nullptr);
#pragma endregion




#pragma region Voxel Data Structured Buffer
	D3D12_RESOURCE_DESC voxelDataStructuredBufferDesc;
	voxelDataStructuredBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	voxelDataStructuredBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	voxelDataStructuredBufferDesc.Width = maxNumberOfVoxels * sizeof(UINT32) * 2;
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

	// Zero out the resource
	UINT8* pVoxelDataStructuredMappedBuffer;
	ThrowIfFailed(voxel_data_structured_buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pVoxelDataStructuredMappedBuffer)));
	ZeroMemory(pVoxelDataStructuredMappedBuffer, maxNumberOfVoxels * sizeof(UINT32) * 2);
	voxel_data_structured_buffer->Unmap(0, nullptr);
	
	// Create a UAV
	D3D12_UNORDERED_ACCESS_VIEW_DESC voxelDataStructuredBufferUAVDesc = {};
	voxelDataStructuredBufferUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	voxelDataStructuredBufferUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	voxelDataStructuredBufferUAVDesc.Buffer.FirstElement = 0;
	voxelDataStructuredBufferUAVDesc.Buffer.NumElements = maxNumberOfVoxels;
	voxelDataStructuredBufferUAVDesc.Buffer.StructureByteStride = sizeof(UINT32) * 2;
	voxelDataStructuredBufferUAVDesc.Buffer.CounterOffsetInBytes = 0;
	d3dDevice->CreateUnorderedAccessView(voxel_data_structured_buffer.Get(), nullptr, &voxelDataStructuredBufferUAVDesc, cbv_srv_uav_cpu_handle);
	cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);
#pragma endregion





	// Create the camera view projection constant buffer
	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(c_frame_count * c_aligned_view_projection_matrix_constant_buffer);
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&uploadHeapProperties,
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
	CD3DX12_RANGE readRange(0, 0);	// We do not intend to read from this resource on the CPU.
	ThrowIfFailed(camera_view_projection_constant_buffer->Map(0, &readRange, reinterpret_cast<void**>(&camera_view_projection_constant_mapped_buffer)));
	ZeroMemory(camera_view_projection_constant_mapped_buffer, c_frame_count * c_aligned_view_projection_matrix_constant_buffer);
	


	

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
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(ShaderStructureCPUModelAndInverseTransposeModelView) * TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&transform_matrix_upload_buffers[0])));
	NAME_D3D12_OBJECT(transform_matrix_upload_buffers[0]);

	// Create constant buffer views to access the upload buffer.
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = transform_matrix_upload_buffers[0]->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = sizeof(ShaderStructureCPUModelAndInverseTransposeModelView) * TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES;
	d3dDevice->CreateConstantBufferView(&cbvDesc, cbv_srv_uav_cpu_handle);
	cbv_srv_uav_cpu_handle.Offset(cbv_srv_uav_descriptor_size);
	

	// Map the constant buffer.
	// We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE readRangeModelTransformMatrixBuffer(0, 0);	// We do not intend to read from this resource on the CPU.
	ThrowIfFailed(transform_matrix_upload_buffers[0]->Map(0, &readRangeModelTransformMatrixBuffer, reinterpret_cast<void**>(&transform_matrix_mapped_upload_buffers[0])));

	// Create an event for the high priority queue
	event_command_list_copy_high_priority_progress = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (event_command_list_copy_high_priority_progress == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	for (int i = 0; i < c_frame_count; i++)
	{
		fence_per_frame_command_list_copy_high_priority_values[i] = 0;
	}


	// Close the command list and execute it to begin the vertex/index buffer copy into the GPU's default heap.
	ThrowIfFailed(command_list_direct->Close());
	ID3D12CommandList* ppCommandLists[] = { command_list_direct.Get() };
	device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Wait for the command list to finish executing; the vertex/index buffers need to be uploaded to the GPU before the upload resources go out of scope.
	device_resources->WaitForGpu();
}

// Initializes view parameters when the window size changes.
void SceneRenderer3D::CreateWindowSizeDependentResources(Camera& camera)
{
	D3D12_VIEWPORT viewport = device_resources->GetScreenViewport();
	scissor_rect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height)};
	XMFLOAT4X4 orientation = device_resources->GetOrientationTransform3D();
	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);
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



// Renders one frame using the vertex and pixel shaders.
ID3D12GraphicsCommandList* SceneRenderer3D::Render(vector<Mesh>& scene, Camera& camera, bool voxelDebugVisualization)
{
	UINT currentFrameIndex = device_resources->GetCurrentFrameIndex();
	ThrowIfFailed(device_resources->GetCommandAllocatorDirect()->Reset());
	// The command list can be reset anytime after ExecuteCommandList() is called.
	ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_default.Get()));
	// Update the view matrix
	DirectX::XMStoreFloat4x4(&camera_view_projection_constant_buffer_data.view, XMMatrixTranspose(camera.GetViewMatrix()));
	// Update the mapped viewProjection constant buffer
	UINT8* destination = camera_view_projection_constant_mapped_buffer + (currentFrameIndex * c_aligned_view_projection_matrix_constant_buffer);
	std::memcpy(destination, &camera_view_projection_constant_buffer_data, sizeof(camera_view_projection_constant_buffer_data));
	// Set the graphics root signature and descriptor heaps to be used by this frame.
	command_list_direct->SetGraphicsRootSignature(root_signature.Get());
	ID3D12DescriptorHeap* ppHeaps[] = { descriptor_heap_cbv_srv_uav.Get() };
	command_list_direct->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	// Bind the descriptor tables to their respective heap starts
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(descriptor_heap_cbv_srv_uav->GetGPUDescriptorHandleForHeapStart(), currentFrameIndex, cbv_srv_uav_descriptor_size);
	command_list_direct->SetGraphicsRootDescriptorTable(2, gpuHandle);
	gpuHandle.Offset((c_frame_count - currentFrameIndex) * cbv_srv_uav_descriptor_size);
	command_list_direct->SetGraphicsRootDescriptorTable(1, gpuHandle);
	gpuHandle.Offset(cbv_srv_uav_descriptor_size);
	command_list_direct->SetGraphicsRootDescriptorTable(3, gpuHandle);


	// Set the viewport and scissor rectangle.
	D3D12_VIEWPORT viewport = device_resources->GetScreenViewport();
	command_list_direct->RSSetViewports(1, &viewport);
	command_list_direct->RSSetScissorRects(1, &scissor_rect);

	// Indicate this resource will be in use as a render target.
	CD3DX12_RESOURCE_BARRIER renderTargetResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(device_resources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	command_list_direct->ResourceBarrier(1, &renderTargetResourceBarrier);

	// Bind the render target and depth buffer
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = device_resources->GetRenderTargetView();
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = device_resources->GetDepthStencilView();
	command_list_direct->ClearRenderTargetView(renderTargetView, DirectX::Colors::CornflowerBlue, 0, nullptr);
	command_list_direct->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	command_list_direct->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);
	command_list_direct->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	

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
	for (int i = 0; i < scene.size(); i++)
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
				scene[i].vertex_buffer_view.StrideInBytes = sizeof(ShaderStructureCPUVertexPositionNormalTexture);
				scene[i].vertex_buffer_view.SizeInBytes = scene[i].vertices.size() * sizeof(ShaderStructureCPUVertexPositionNormalTexture);

				scene[i].index_buffer_view.BufferLocation = scene[i].index_buffer->GetGPUVirtualAddress();
				scene[i].index_buffer_view.SizeInBytes = scene[i].indices.size() * sizeof(uint16_t);
				scene[i].index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
			}

			// Now we can render out this object.
			// But before that we need to start uploading it's model transform matrix to the gpu
			// If the object isn't static we will need to update its model transform matrix
			ShaderStructureCPUModelAndInverseTransposeModelView* pEntry = nullptr;
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

				ShaderStructureCPUModelAndInverseTransposeModelView* pEntry = transform_matrix_mapped_upload_buffers[scene[i].per_frame_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_transform_matrix][0]] + scene[i].per_frame_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_transform_matrix][1];
				scene[i].transform_matrix = XMMatrixMultiply(rotationMatrix, translationMatrix);
				XMStoreFloat4x4(&pEntry->model, XMMatrixTranspose(scene[i].transform_matrix));

				// This way if the object was initialized as static
				// this would have been it's one and only model transform update.
				// After this it won't be updated anymore
				scene[i].is_static = scene[i].initialized_as_static;
				scene[i].initialized_as_static = false;
			}
			else
			{
				ShaderStructureCPUModelAndInverseTransposeModelView* pEntry = transform_matrix_mapped_upload_buffers[scene[i].per_frame_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_transform_matrix][0]] + scene[i].per_frame_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_transform_matrix][1];
			}
			
			XMStoreFloat4x4(&pEntry->inverse_transpose_model_view, /*XMMatrixTranspose(*/XMMatrixInverse(nullptr, XMMatrixMultiply(scene[i].transform_matrix, camera.GetViewMatrix())))/*)*/;
			// Set the transform matrix buffer indexes
			command_list_direct->SetGraphicsRoot32BitConstants(0, 
				2, 
				scene[i].per_frame_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_transform_matrix], 
				0);
			// Bind the vertex and index buffer views
			command_list_direct->IASetVertexBuffers(0, 1, &scene[i].vertex_buffer_view);
			command_list_direct->IASetIndexBuffer(&scene[i].index_buffer_view);
			command_list_direct->DrawIndexedInstanced(scene[i].indices.size(), 1, 0, 0, 0);
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
				const UINT vertexBufferSize = scene[i].vertices.size() * sizeof(ShaderStructureCPUVertexPositionNormalTexture);
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
	return command_list_direct.Get();
}
