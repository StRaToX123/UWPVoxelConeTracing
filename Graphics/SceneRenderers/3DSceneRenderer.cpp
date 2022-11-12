﻿#include "Graphics/SceneRenderers/3DSceneRenderer.h"




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
#pragma region Default Pipeline State
	CD3DX12_DESCRIPTOR_RANGE rangeSRV;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVviewProjectionMatrix;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVmodelTransformMatrix;

	CD3DX12_ROOT_PARAMETER parameterConstants; // contains modelTransformMatrixBufferIndex, modelTransformMatrixBufferInternalIndex
	CD3DX12_ROOT_PARAMETER parameterPixelVisibleDescriptorTable;
	CD3DX12_ROOT_PARAMETER parameterVertexVisibleDescriptorTableViewProjectionMatrixBuffer;
	CD3DX12_ROOT_PARAMETER parameterVertexVisibleDescriptorTableModelTransformMatrixBuffers;


	rangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	rangeCBVviewProjectionMatrix.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	rangeCBVmodelTransformMatrix.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);

	parameterConstants.InitAsConstants(2, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	parameterPixelVisibleDescriptorTable.InitAsDescriptorTable(1, &rangeSRV, D3D12_SHADER_VISIBILITY_PIXEL);
	parameterVertexVisibleDescriptorTableViewProjectionMatrixBuffer.InitAsDescriptorTable(1, &rangeCBVviewProjectionMatrix, D3D12_SHADER_VISIBILITY_VERTEX);
	parameterVertexVisibleDescriptorTableModelTransformMatrixBuffers.InitAsDescriptorTable(1, &rangeCBVmodelTransformMatrix, D3D12_SHADER_VISIBILITY_VERTEX);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // Only the input assembler stage needs access to the constant buffer.
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
	CD3DX12_ROOT_PARAMETER parameterArray[4] = { parameterConstants,
		parameterPixelVisibleDescriptorTable,
		parameterVertexVisibleDescriptorTableViewProjectionMatrixBuffer,
		parameterVertexVisibleDescriptorTableModelTransformMatrixBuffers };
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

	descRootSignature.Init(4, parameterArray, 1, &staticSamplerDesc, rootSignatureFlags);

	ComPtr<ID3DBlob> pSignature;
	ComPtr<ID3DBlob> pError;
	D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf());
	if (pError.Get() != NULL)
	{
		DisplayDebugMessage("@@@@@@@@@@@@@@@@@@@@\n%s\n@@@@@@@@@@@@@@@@@@@@@\n", (char*)pError->GetBufferPointer());
		throw ref new FailureException();
	}

	ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&default_root_signature)));
	NAME_D3D12_OBJECT(default_root_signature);

	// Load shaders
	// Get the install folder full path
	auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;
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
	state.pRootSignature = default_root_signature.Get();
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

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&default_pipeline_state)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pVertexShadeByteCode;
	delete[] pPixelShadeByteCode;
#pragma endregion

#pragma region Voxelizer Pipeline State
	CD3DX12_DESCRIPTOR_RANGE voxelizerRangeCBVmodelTransformMatrix;
	CD3DX12_DESCRIPTOR_RANGE voxelizerRangeCBVvoxelData;

	CD3DX12_ROOT_PARAMETER voxelizerParameterConstants; // contains modelTransformMatrixBufferIndex, modelTransformMatrixBufferInternalIndex
	CD3DX12_ROOT_PARAMETER voxelizerParameterGeometryVisibleDescriptorTableModelTransformMatrixBuffers;
	CD3DX12_ROOT_PARAMETER voxelizerParameterGeometryPixelVisibleDescriptorTableVoxelDataBuffers;

	voxelizerRangeCBVmodelTransformMatrix.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	voxelizerRangeCBVvoxelData.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);
	
	voxelizerParameterConstants.InitAsConstants(2, 0, 0, D3D12_SHADER_VISIBILITY_GEOMETRY);
	voxelizerParameterGeometryVisibleDescriptorTableModelTransformMatrixBuffers.InitAsDescriptorTable(1, &voxelizerRangeCBVmodelTransformMatrix, D3D12_SHADER_VISIBILITY_GEOMETRY);
	voxelizerParameterGeometryPixelVisibleDescriptorTableVoxelDataBuffers.InitAsDescriptorTable(1, &voxelizerRangeCBVvoxelData, D3D12_SHADER_VISIBILITY_ALL);


	D3D12_ROOT_SIGNATURE_FLAGS voxelizerRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_SIGNATURE_DESC voxelizerDescRootSignature;
	CD3DX12_ROOT_PARAMETER voxelizerParameterArray[3] = { voxelizerParameterConstants,
		voxelizerParameterGeometryVisibleDescriptorTableModelTransformMatrixBuffers,
		voxelizerParameterGeometryPixelVisibleDescriptorTableVoxelDataBuffers };

	voxelizerDescRootSignature.Init(3, voxelizerParameterArray, 0, NULL, voxelizerRootSignatureFlags);

	ComPtr<ID3DBlob> pVoxelizerSignature;
	ComPtr<ID3DBlob> pVoxelizerError;
	D3D12SerializeRootSignature(&voxelizerDescRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pVoxelizerSignature.GetAddressOf(), pVoxelizerError.GetAddressOf());
	if (pError.Get() != NULL)
	{
		DisplayDebugMessage("@@@@@@@@@@@@@@@@@@@@\n%s\n@@@@@@@@@@@@@@@@@@@@@\n", (char*)pVoxelizerError->GetBufferPointer());
		throw ref new FailureException();
	}

	ThrowIfFailed(d3dDevice->CreateRootSignature(0, pVoxelizerSignature->GetBufferPointer(), pVoxelizerSignature->GetBufferSize(), IID_PPV_ARGS(&voxelizer_root_signature)));
	NAME_D3D12_OBJECT(voxelizer_root_signature);

	// Load the geometry shader
	char* pGeometryShaderByteCode;
	int geometryShaderByteCodeLength;
	LoadShaderByteCode((standardStringInstallPath + "\\Voxelizer_GS.cso").c_str(), pGeometryShaderByteCode, geometryShaderByteCodeLength);
	//LoadShaderByteCode((standardStringInstallPath + "\\Voxelizer_VS.cso").c_str(), pVertexShadeByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\Voxelizer_PS.cso").c_str(), pPixelShadeByteCode, pixelShaderByteCodeLength);

	// Create the pipeline state once the shaders are loaded.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC voxelizerState = {};
	voxelizerState.InputLayout = { ShaderStructureCPUVertexPositionNormalColor::input_elements, ShaderStructureCPUVertexPositionNormalColor::input_element_count };
	voxelizerState.pRootSignature = voxelizer_root_signature.Get();
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

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&voxelizer_pipeline_state)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pGeometryShaderByteCode;
	delete[] pVertexShadeByteCode;
	delete[] pPixelShadeByteCode;
#pragma endregion


	// Create a direct command list.
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, device_resources->GetCommandAllocatorDirect(), default_pipeline_state.Get(), IID_PPV_ARGS(&command_list_direct)));
	NAME_D3D12_OBJECT(command_list_direct);
	// Create copy command lists.
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, device_resources->GetCommandAllocatorCopyNormalPriority(), default_pipeline_state.Get(), IID_PPV_ARGS(&command_list_copy_normal_priority)));
	NAME_D3D12_OBJECT(command_list_copy_normal_priority);
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, device_resources->GetCommandAllocatorCopyHighPriority(), default_pipeline_state.Get(), IID_PPV_ARGS(&command_list_copy_high_priority)));
	NAME_D3D12_OBJECT(command_list_copy_high_priority);
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
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptor_heap_cbv_srv)));
	NAME_D3D12_OBJECT(descriptor_heap_cbv_srv);

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
	cbv_srv_cpu_handle.InitOffsetted(descriptor_heap_cbv_srv->GetCPUDescriptorHandleForHeapStart(), 0);
	cbv_srv_descriptor_size = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int n = 0; n < c_frame_count; n++)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.BufferLocation = cbvGpuAddress;
		desc.SizeInBytes = c_aligned_view_projection_matrix_constant_buffer;
		d3dDevice->CreateConstantBufferView(&desc, cbv_srv_cpu_handle);

		cbvGpuAddress += desc.SizeInBytes;
		cbv_srv_cpu_handle.Offset(cbv_srv_descriptor_size);
	}

	// Map the constant buffers.
	// We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE readRange(0, 0);	// We do not intend to read from this resource on the CPU.
	ThrowIfFailed(camera_view_projection_constant_buffer->Map(0, &readRange, reinterpret_cast<void**>(&camera_view_projection_constant_mapped_buffer)));
	ZeroMemory(camera_view_projection_constant_mapped_buffer, c_frame_count * c_aligned_view_projection_matrix_constant_buffer);
	

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
	device_resources->GetD3DDevice()->CreateShaderResourceView(test_texture.Get(), &srvDesc, cbv_srv_cpu_handle);
	cbv_srv_cpu_handle.Offset(cbv_srv_descriptor_size);

	// Create and setup a constant buffer that holds each scene object's world transform matrix
	for (int i = 0; i < (MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES); i++)
	{
		free_slots_preallocated_array[i] = i;
	}
	
	model_transform_matrix_upload_buffers.push_back(Microsoft::WRL::ComPtr<ID3D12Resource>());
	model_transform_matrix_mapped_upload_buffers.push_back(nullptr);
	available_model_transform_matrix_buffer.push_back(0);
	model_transform_matrix_buffer_free_slots.push_back(vector<UINT>(free_slots_preallocated_array, free_slots_preallocated_array + MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES));

	// Create an accompanying upload heap 
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(XMFLOAT4X4) * MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&model_transform_matrix_upload_buffers[0])));
	NAME_D3D12_OBJECT(model_transform_matrix_upload_buffers[0]);

	// Create constant buffer views to access the upload buffer.
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = model_transform_matrix_upload_buffers[0]->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = sizeof(XMFLOAT4X4) * MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES;
	d3dDevice->CreateConstantBufferView(&cbvDesc, cbv_srv_cpu_handle);
	cbv_srv_cpu_handle.Offset(cbv_srv_descriptor_size);
	

	// Map the constant buffer.
	// We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE readRangeModelTransformMatrixBuffer(0, 0);	// We do not intend to read from this resource on the CPU.
	ThrowIfFailed(model_transform_matrix_upload_buffers[0]->Map(0, &readRangeModelTransformMatrixBuffer, reinterpret_cast<void**>(&model_transform_matrix_mapped_upload_buffers[0])));

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
	ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), default_pipeline_state.Get()));
	// Update the view matrix
	DirectX::XMStoreFloat4x4(&camera_view_projection_constant_buffer_data.view, XMMatrixTranspose(camera.GetViewMatrix()));
	// Update the mapped viewProjection constant buffer
	UINT8* destination = camera_view_projection_constant_mapped_buffer + (currentFrameIndex * c_aligned_view_projection_matrix_constant_buffer);
	std::memcpy(destination, &camera_view_projection_constant_buffer_data, sizeof(camera_view_projection_constant_buffer_data));
	// Set the graphics root signature and descriptor heaps to be used by this frame.
	command_list_direct->SetGraphicsRootSignature(default_root_signature.Get());
	ID3D12DescriptorHeap* ppHeaps[] = { descriptor_heap_cbv_srv.Get() };
	command_list_direct->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	// Bind the descriptor tables to their respective heap starts
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(descriptor_heap_cbv_srv->GetGPUDescriptorHandleForHeapStart(), currentFrameIndex, cbv_srv_descriptor_size);
	command_list_direct->SetGraphicsRootDescriptorTable(2, gpuHandle);
	gpuHandle.Offset((c_frame_count - currentFrameIndex) * cbv_srv_descriptor_size);
	command_list_direct->SetGraphicsRootDescriptorTable(1, gpuHandle);
	gpuHandle.Offset(cbv_srv_descriptor_size);
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
			if (scene[i].is_static == false)
			{
				commandListCopyHighPriorityRequiresExecution = true;
				if (command_list_copy_high_priority_requires_reset == true)
				{
					command_list_copy_high_priority_requires_reset = false;
					ThrowIfFailed(command_list_copy_high_priority->Reset(device_resources->GetCommandAllocatorCopyHighPriority(), default_pipeline_state.Get()));
				}
				
				scene[i].current_frame_index_containing_most_updated_model_transform_matrix++;
				if (scene[i].current_frame_index_containing_most_updated_model_transform_matrix == c_frame_count)
				{
					scene[i].current_frame_index_containing_most_updated_model_transform_matrix = 0;
				}

				// Check if this object has a slot assigned to it, if not assign one
				if (scene[i].per_frame_model_transform_matrix_buffer_indexes_assigned[scene[i].current_frame_index_containing_most_updated_model_transform_matrix] == false)
				{
					scene[i].per_frame_model_transform_matrix_buffer_indexes_assigned[scene[i].current_frame_index_containing_most_updated_model_transform_matrix] = true;
					scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][0] = available_model_transform_matrix_buffer[0];
					auto& freeSlotsArray = model_transform_matrix_buffer_free_slots[available_model_transform_matrix_buffer[0]];
					scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][1] = freeSlotsArray[0];
					// Erase that free slot
					freeSlotsArray.erase(freeSlotsArray.begin());
					// Check if that was the last free slot, and if so update the per_frame_available_scene_object_model_transform_matrix_buffer array
					if (freeSlotsArray.size() == 0)
					{
						available_model_transform_matrix_buffer.erase(available_model_transform_matrix_buffer.begin());
						// Check if we ran out of buffers with free slots for this frame index
						if (available_model_transform_matrix_buffer.size() == 0)
						{
							// Create a new buffer
							model_transform_matrix_upload_buffers.push_back(Microsoft::WRL::ComPtr<ID3D12Resource>());
							model_transform_matrix_mapped_upload_buffers.push_back(nullptr);
							model_transform_matrix_buffer_free_slots.push_back(vector<UINT>(free_slots_preallocated_array, free_slots_preallocated_array + MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES));
							available_model_transform_matrix_buffer.push_back(model_transform_matrix_buffer_free_slots.size() - 1);

							// Create an accompanying upload heap 
							ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
								&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
								D3D12_HEAP_FLAG_NONE,
								&CD3DX12_RESOURCE_DESC::Buffer(sizeof(XMFLOAT4X4) * MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES),
								D3D12_RESOURCE_STATE_GENERIC_READ,
								nullptr,
								IID_PPV_ARGS(&model_transform_matrix_upload_buffers[model_transform_matrix_upload_buffers.size() - 1])));

							// Create constant buffer views to access the upload buffer.
							D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
							cbvDesc.BufferLocation = model_transform_matrix_upload_buffers[model_transform_matrix_mapped_upload_buffers.size() - 1]->GetGPUVirtualAddress();
							cbvDesc.SizeInBytes = sizeof(XMFLOAT4X4) * MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES;
							device_resources->GetD3DDevice()->CreateConstantBufferView(&cbvDesc, cbv_srv_cpu_handle);
							cbv_srv_cpu_handle.Offset(cbv_srv_descriptor_size);


							// Map the constant buffer.
							// We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.
							CD3DX12_RANGE readRangeModelTransformMatrixBuffer(0, 0);	// We do not intend to read from this resource on the CPU.
							ThrowIfFailed(model_transform_matrix_upload_buffers[model_transform_matrix_upload_buffers.size() - 1]->Map(0, &readRangeModelTransformMatrixBuffer, reinterpret_cast<void**>(&model_transform_matrix_mapped_upload_buffers[model_transform_matrix_mapped_upload_buffers.size() - 1])));
						}
					}
				}

				XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(scene[i].local_rotation.x),
					XMConvertToRadians(scene[i].local_rotation.y),
					XMConvertToRadians(scene[i].local_rotation.z));
				XMMATRIX translationMatrix = XMMatrixTranslation(scene[i].world_position.x,
					scene[i].world_position.y,
					scene[i].world_position.z);

				XMStoreFloat4x4(model_transform_matrix_mapped_upload_buffers[scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][0]] + scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][1], XMMatrixTranspose(XMMatrixMultiply(rotationMatrix, translationMatrix)));
				
				// This way if the object was initialized as static
				// this would have been it's one and only model transform update.
				// After this it won't be updated anymore
				scene[i].is_static = scene[i].initialized_as_static;
				scene[i].initialized_as_static = false;
			}
			
			// Set the model transform matrix buffer indexes
			command_list_direct->SetGraphicsRoot32BitConstants(0, 
				2, 
				scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix], 
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
					ThrowIfFailed(command_list_copy_normal_priority->Reset(device_resources->GetCommandAllocatorCopyNormalPriority(), default_pipeline_state.Get()));
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
