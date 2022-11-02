#include "Graphics/SceneRenderers/3DSceneRenderer.h"




// Indices into the application state map.
Platform::String^ AngleKey = "Angle";
Platform::String^ TrackingKey = "Tracking";

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
Sample3DSceneRenderer::Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, Camera& camera) :
	device_resources(deviceResources),
	camera_view_projection_constant_mapped_buffer(nullptr),
	fence_command_list_copy_normal_priority_progress_latest_unused_value(1),
	fence_command_list_copy_high_priority_progress_latest_unused_value(1),
	command_allocator_copy_normal_priority_already_reset(true),
	command_list_copy_normal_priority_requires_reset(false),
	command_allocator_copy_high_priority_already_reset(true),
	command_list_copy_high_priority_requires_reset(false),
	event_command_list_copy_high_priority_progress(NULL)
{
	LoadState();
	ZeroMemory(&camera_view_projection_constant_buffer_data, sizeof(camera_view_projection_constant_buffer_data));

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources(camera);
}

Sample3DSceneRenderer::~Sample3DSceneRenderer()
{
	camera_view_projection_constant_buffer->Unmap(0, nullptr);
	camera_view_projection_constant_mapped_buffer = nullptr;
	if (event_command_list_copy_high_priority_progress != NULL)
	{
		CloseHandle(event_command_list_copy_high_priority_progress);
	}
}

void Sample3DSceneRenderer::CreateDeviceDependentResources()
{
	auto d3dDevice = device_resources->GetD3DDevice();
	// Create a root signature with a single constant buffer slot.
	CD3DX12_DESCRIPTOR_RANGE rangeSRV;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVviewProjectionMatrix;
	CD3DX12_DESCRIPTOR_RANGE rangeCBVmodelTransformMatrix;
	
	CD3DX12_ROOT_PARAMETER parameterConstants; // contains frameIndex, modelTransformMatrixBufferIndex, modelTransformMatrixBufferInternalIndex
	CD3DX12_ROOT_PARAMETER parameterVertexVisibleRootConstantModelTransformMatrixBufferIndexes;
	CD3DX12_ROOT_PARAMETER parameterPixelVisibleDescriptorTable;
	CD3DX12_ROOT_PARAMETER parameterVertexVisibleDescriptorTableViewProjectionMatrixBuffer;
	CD3DX12_ROOT_PARAMETER parameterVertexVisibleDescriptorTableModelTransformMatrixBuffers;
	

	rangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0);
	rangeCBVviewProjectionMatrix.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	rangeCBVmodelTransformMatrix.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);
	
	parameterConstants.InitAsConstants(3, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
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
	DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
	DX::ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
    NAME_D3D12_OBJECT(root_signature);
	
	// Load shaders
	// Get the install folder full path
	auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;
	wstring wStringInstallPath(Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data());
	string standardStringInstallPath(wStringInstallPath.begin(), wStringInstallPath.end());
	// Load the vertex shader
	ifstream is;
	is.open((standardStringInstallPath + "\\SampleVertexShader.cso").c_str(), ios::binary);
	// get length of file:
	is.seekg(0, ios::end);
	int vertexShaderByteCodeLength = is.tellg();
	is.seekg(0, ios::beg);
	// allocate memory:
	char* pVertexShadeByteCode = new char[vertexShaderByteCodeLength];
	// read data as a block:
    is.read(pVertexShadeByteCode, vertexShaderByteCodeLength);
	is.close();

	// Load the pixel shader
	is.open((standardStringInstallPath + "\\SamplePixelShader.cso").c_str(), ios::binary);
	// get length of file:
	is.seekg(0, ios::end);
	int pixelShaderByteCodeLength = is.tellg();
	is.seekg(0, ios::beg);
	// allocate memory:
	char* pPixelShadeByteCode = new char[pixelShaderByteCodeLength];
	// read data as a block:
	is.read(pPixelShadeByteCode, pixelShaderByteCodeLength);
	is.close();

	
	// Create the pipeline state once the shaders are loaded.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
	state.InputLayout = { VertexPositionNormalTexture::input_elements, VertexPositionNormalTexture::input_element_count };
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

	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipeline_state)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pVertexShadeByteCode;
	delete[] pPixelShadeByteCode;

	// Upload cube geometry resources to the GPU.
	// Create a direct command list.
	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, device_resources->GetCommandAllocatorDirect(), pipeline_state.Get(), IID_PPV_ARGS(&command_list_direct)));
	NAME_D3D12_OBJECT(command_list_direct);
	// Create copy command lists.
	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, device_resources->GetCommandAllocatorCopyNormalPriority(), pipeline_state.Get(), IID_PPV_ARGS(&command_list_copy_normal_priority)));
	NAME_D3D12_OBJECT(command_list_copy_normal_priority);
	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, device_resources->GetCommandAllocatorCopyHighPriority(), pipeline_state.Get(), IID_PPV_ARGS(&command_list_copy_high_priority)));
	NAME_D3D12_OBJECT(command_list_copy_high_priority);
	// Create a fences to go along with the copy command lists.
	// This fence will be used to signal when a resource has been successfully uploaded to the gpu
	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_copy_normal_priority_progress)));
	NAME_D3D12_OBJECT(fence_command_list_copy_normal_priority_progress);
	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_command_list_copy_high_priority_progress)));
	NAME_D3D12_OBJECT(fence_command_list_copy_high_priority_progress);

	// Create a descriptor heap to house EVERYTHING!!!
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 100;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptor_heap_cbv_srv)));
	NAME_D3D12_OBJECT(descriptor_heap_cbv_srv);

	// Create the camera view projection constant buffer
	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(c_frame_count * c_aligned_view_projection_matrix_constant_buffer);
	DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
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
	DX::ThrowIfFailed(camera_view_projection_constant_buffer->Map(0, &readRange, reinterpret_cast<void**>(&camera_view_projection_constant_mapped_buffer)));
	ZeroMemory(camera_view_projection_constant_mapped_buffer, c_frame_count * c_aligned_view_projection_matrix_constant_buffer);
	// Set the camera_view_projection_constant_buffer_update_counter counter
	camera_view_projection_constant_buffer_update_counter = c_frame_count;
	

	// Create and upload the test texture
	TexMetadata metadata;
	ScratchImage scratchImage;
	DX::ThrowIfFailed(LoadFromWICFile((wStringInstallPath + L"\\Assets\\woah.jpg").c_str(), WIC_FLAGS::WIC_FLAGS_NONE, &metadata, scratchImage));
	D3D12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata.format,
		static_cast<UINT64>(metadata.width),
		static_cast<UINT>(metadata.height),
		static_cast<UINT16>(metadata.arraySize));

	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&test_texture)));

	std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
	const DirectX::Image* pImages = scratchImage.GetImages();
	for (int i = 0; i < scratchImage.GetImageCount(); ++i)
	{
		auto& subresource = subresources[i];
		subresource.RowPitch = pImages[i].rowPitch;
		subresource.SlicePitch = pImages[i].slicePitch;
		subresource.pData = pImages[i].pixels;
	}

	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
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
	default_model_transform_matrix_buffer_subresources.reserve(MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES);
	model_transform_matrix_buffer_subresources_number_of_rows.reserve(MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES);
	model_transform_matrix_buffer_subresources_row_size_in_bytes.reserve(MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES);
	DirectX::XMStoreFloat4x4(&default_model_transform_matrix.model, XMMatrixIdentity());
	for (int i = 0; i < (MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES); i++)
	{
		auto& subresource = default_model_transform_matrix_buffer_subresources[i * 4];
		subresource.RowPitch = c_aligned_model_transform_matrix;
		subresource.SlicePitch = c_aligned_model_transform_matrix;
		subresource.pData = &default_model_transform_matrix;

		model_transform_matrix_buffer_subresources_number_of_rows.emplace_back(1);
		model_transform_matrix_buffer_subresources_row_size_in_bytes.emplace_back(c_aligned_model_transform_matrix);
	}

	for (int i = 0; i < c_frame_count; i++)
	{
		fence_per_frame_command_list_copy_high_priority_values[i] = 0;
		per_frame_model_transform_matrix_buffers[i].push_back(Microsoft::WRL::ComPtr<ID3D12Resource>());
		per_frame_model_transform_matrix_upload_buffers[i].push_back(Microsoft::WRL::ComPtr<ID3D12Resource>());
		per_frame_available_model_transform_matrix_buffer[i].push_back(0);
		per_frame_model_transform_matrix_buffer_free_slots[i].push_back(vector<UINT>());
		per_frame_model_transform_matrix_buffer_free_slots[i][0].reserve(MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES);
		/*
		per_frame_updated_model_transform_matrix_buffer_subresources[i].push_back(vector<D3D12_SUBRESOURCE_DATA>());
		per_frame_updated_model_transform_matrix_buffer_subresources[i][0].reserve(MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES);
		per_frame_updated_model_transform_matrix_buffer_subresource_data[i].push_back(vector<XMFLOAT4X4>());
		per_frame_updated_model_transform_matrix_buffer_subresource_data[i][0].reserve(MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES);
		*/
		for (int j = 0; j < MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES; j++)
		{
			per_frame_model_transform_matrix_buffer_free_slots[i][0].emplace_back(j);
			/*
			per_frame_updated_model_transform_matrix_buffer_subresources[i][0].emplace_back(D3D12_SUBRESOURCE_DATA());
			auto& subresource = per_frame_updated_model_transform_matrix_buffer_subresources[i][0][j];
			subresource.RowPitch = c_aligned_model_transform_matrix;
			subresource.SlicePitch = c_aligned_model_transform_matrix;
			subresource.pData = &default_model_transform_matrix;

			per_frame_updated_model_transform_matrix_buffer_subresource_data[i][0].emplace_back(default_model_transform_matrix);
			*/
		}

		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(c_aligned_model_transform_matrix_constant_buffer),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&per_frame_model_transform_matrix_buffers[i][0])));

		// Create an accompanying upload heap 
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(c_aligned_model_transform_matrix_constant_buffer),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&per_frame_model_transform_matrix_upload_buffers[i][0])));

		UpdateSubresources(command_list_direct.Get(), per_frame_model_transform_matrix_buffers[i][0].Get(),
			per_frame_model_transform_matrix_upload_buffers[i][0].Get(),
			0,
			0,
			static_cast<uint32_t>(default_model_transform_matrix_buffer_subresources.size()),
			default_model_transform_matrix_buffer_subresources.data());

		CD3DX12_RESOURCE_BARRIER modelTransformMatrixBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(per_frame_model_transform_matrix_buffers[i][0].Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		command_list_direct->ResourceBarrier(1, &modelTransformMatrixBufferBarrier);

		// Create a CBV for the buffer
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.BufferLocation = per_frame_model_transform_matrix_upload_buffers[i][0]->GetGPUVirtualAddress();
		desc.SizeInBytes = c_aligned_model_transform_matrix_constant_buffer;
		d3dDevice->CreateConstantBufferView(&desc, cbv_srv_cpu_handle);
		cbv_srv_cpu_handle.Offset(cbv_srv_descriptor_size);
	}
		
	model_transform_matrix_placed_subresource_footprint.Footprint.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
	model_transform_matrix_placed_subresource_footprint.Footprint.Width = 1;
	model_transform_matrix_placed_subresource_footprint.Footprint.Height = 1;
	model_transform_matrix_placed_subresource_footprint.Footprint.Depth = 1;
	model_transform_matrix_placed_subresource_footprint.Footprint.RowPitch = c_aligned_model_transform_matrix;

	model_transform_matrix_resource_barrier_transitions_batch_array.reserve(100);
	model_transform_matrix_resource_barrier_returns_batch_array.reserve(100);

	event_command_list_copy_high_priority_progress = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (event_command_list_copy_high_priority_progress == nullptr)
	{
		DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}


	// Close the command list and execute it to begin the vertex/index buffer copy into the GPU's default heap.
	DX::ThrowIfFailed(command_list_direct->Close());
	ID3D12CommandList* ppCommandLists[] = { command_list_direct.Get() };
	device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Wait for the command list to finish executing; the vertex/index buffers need to be uploaded to the GPU before the upload resources go out of scope.
	device_resources->WaitForGpu();
}

// Initializes view parameters when the window size changes.
void Sample3DSceneRenderer::CreateWindowSizeDependentResources(Camera& camera)
{
	D3D12_VIEWPORT viewport = device_resources->GetScreenViewport();
	scissor_rect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height)};
	XMFLOAT4X4 orientation = device_resources->GetOrientationTransform3D();
	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);
	DirectX::XMStoreFloat4x4(&camera_view_projection_constant_buffer_data.projection, XMMatrixTranspose(camera.GetProjectionMatrix() * orientationMatrix));
}


// Saves the current state of the renderer.
void Sample3DSceneRenderer::SaveState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;

	if (state->HasKey(AngleKey))
	{
		state->Remove(AngleKey);
	}

	//state->Insert(AngleKey, PropertyValue::CreateSingle(m_angle));
}

// Restores the previous state of the renderer.
void Sample3DSceneRenderer::LoadState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;
	if (state->HasKey(AngleKey))
	{
		//m_angle = safe_cast<IPropertyValue^>(state->Lookup(AngleKey))->GetSingle();
		//state->Remove(AngleKey);
	}
}



// Renders one frame using the vertex and pixel shaders.
ID3D12GraphicsCommandList* Sample3DSceneRenderer::Render(vector<Mesh>& scene, Camera& camera)
{
	UINT currentFrameIndex = device_resources->GetCurrentFrameIndex();
	UINT previousFrameIndex;
	if (currentFrameIndex == 0)
	{
		previousFrameIndex = c_frame_count - 1;
	}
	else
	{
		previousFrameIndex = currentFrameIndex - 1;
	}

	DX::ThrowIfFailed(device_resources->GetCommandAllocatorDirect()->Reset());
	// The command list can be reset anytime after ExecuteCommandList() is called.
	DX::ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state.Get()));
	// Set the frame index
	command_list_direct->SetGraphicsRoot32BitConstant(0, currentFrameIndex, 0);
	// Update the view matrix
	DirectX::XMStoreFloat4x4(&camera_view_projection_constant_buffer_data.view, XMMatrixTranspose(camera.GetViewMatrix()));
	// Update the mapped viewProjection constant buffer
	UINT8* destination = camera_view_projection_constant_mapped_buffer + (currentFrameIndex * c_aligned_view_projection_matrix_constant_buffer);
	std::memcpy(destination, &camera_view_projection_constant_buffer_data, sizeof(camera_view_projection_constant_buffer_data));
	// Set the graphics root signature and descriptor heaps to be used by this frame.
	command_list_direct->SetGraphicsRootSignature(root_signature.Get());
	ID3D12DescriptorHeap* ppHeaps[] = { descriptor_heap_cbv_srv.Get() };
	command_list_direct->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	// Bind the descriptor tables to their respective heap starts
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(descriptor_heap_cbv_srv->GetGPUDescriptorHandleForHeapStart(), 0, cbv_srv_descriptor_size);
	command_list_direct->SetGraphicsRootDescriptorTable(2, gpuHandle);
	gpuHandle.Offset(c_frame_count * cbv_srv_descriptor_size);
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
	UINT64 fenceCommandListCopyNormalPriorityProgressValue = fence_command_list_copy_normal_priority_progress->GetCompletedValue();
	UINT64 fenceCommandListCopyHighPriorityProgressValue = fence_command_list_copy_high_priority_progress->GetCompletedValue();
	// Reset the copy command allocator if there are no command lists executing on it
	if ((fence_command_list_copy_normal_priority_progress_latest_unused_value - 1) == fenceCommandListCopyNormalPriorityProgressValue)
	{
		if (command_allocator_copy_normal_priority_already_reset == false)
		{
			command_allocator_copy_normal_priority_already_reset = true;
			DX::ThrowIfFailed(device_resources->GetCommandAllocatorCopyNormalPriority()->Reset());
		}
	}

	if ((fence_command_list_copy_high_priority_progress_latest_unused_value - 1) == fenceCommandListCopyHighPriorityProgressValue)
	{
		if (command_allocator_copy_high_priority_already_reset == false)
		{
			command_allocator_copy_high_priority_already_reset = true;
			DX::ThrowIfFailed(device_resources->GetCommandAllocatorCopyHighPriority()->Reset());
		}
	}
	
	// Iterate over each object inside of the scene, check if all the resources needed
	// for their rendering are present on the gpu, and if so render them
	for (int i = 0; i < scene.size(); i++)
	{
		// Vertex and index buffer check
		if (scene[i].fence_value_signaling_required_resource_residency < fenceCommandListCopyNormalPriorityProgressValue)
		{
			// If the data has been uploaded to the gpu, there is no more need for the upload buffers
			// Also since this is the first time this mesh is being renderer, we need to create 
			// the vertex and index buffer views
			if (scene[i].vertex_and_index_buffer_upload_started == true)
			{
				scene[i].vertex_buffer_upload.Reset();
				scene[i].index_buffer_upload.Reset();
				scene[i].vertex_and_index_buffer_upload_started = false;

				// Create vertex/index views
				scene[i].vertex_buffer_view.BufferLocation = scene[i].vertex_buffer->GetGPUVirtualAddress();
				scene[i].vertex_buffer_view.StrideInBytes = sizeof(VertexPositionNormalTexture);
				scene[i].vertex_buffer_view.SizeInBytes = scene[i].vertices.size() * sizeof(VertexPositionNormalTexture);

				scene[i].index_buffer_view.BufferLocation = scene[i].index_buffer->GetGPUVirtualAddress();
				scene[i].index_buffer_view.SizeInBytes = scene[i].indices.size() * sizeof(uint16_t);
				scene[i].index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
			}

			if (scene[i].update_previous_frame_index_containing_most_updated_model_transform_matrix == true)
			{
				scene[i].update_previous_frame_index_containing_most_updated_model_transform_matrix = false;
				scene[i].previous_frame_index_containing_most_updated_model_transform_matrix = scene[i].current_frame_index_containing_most_updated_model_transform_matrix;
			}

			// Now we can render out this object.
			// But before that we need to start uploading it's model transform matrix to the gpu
			// If the object isn't static we will need to update its model transform matrix
			if (scene[i].is_static == false)
			{
				if (command_list_copy_high_priority_requires_reset == true)
				{
					command_list_copy_high_priority_requires_reset = false;
					DX::ThrowIfFailed(command_list_copy_high_priority->Reset(device_resources->GetCommandAllocatorCopyHighPriority(), pipeline_state.Get()));
					per_model_transform_matrix_buffer_update_data.clear();
					model_transform_matrix_resource_barrier_transitions_batch_array.clear();
					model_transform_matrix_resource_barrier_returns_batch_array.clear();
				}
				
				// Check if this object has a slot assigned to it, if not assign one
				if (scene[i].per_frame_model_transform_matrix_buffer_indexes_assigned[scene[i].current_frame_index_containing_most_updated_model_transform_matrix] == false)
				{
					scene[i].per_frame_model_transform_matrix_buffer_indexes_assigned[scene[i].current_frame_index_containing_most_updated_model_transform_matrix] = true;
					scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][0] = per_frame_available_model_transform_matrix_buffer[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][0];
					auto& freeSlotsArray = per_frame_model_transform_matrix_buffer_free_slots[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][per_frame_available_model_transform_matrix_buffer[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][0]];
					scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][1] = freeSlotsArray[0];
					// Erase that free slot
					freeSlotsArray.erase(freeSlotsArray.begin());
					// Check if that was the last free slot, and if so update the per_frame_available_scene_object_model_transform_matrix_buffer array
					if (freeSlotsArray.size() == 0)
					{
						per_frame_available_model_transform_matrix_buffer[scene[i].current_frame_index_containing_most_updated_model_transform_matrix].erase(per_frame_available_model_transform_matrix_buffer[scene[i].current_frame_index_containing_most_updated_model_transform_matrix].begin());
						// Check if we ran out of buffers with free slots for this frame index
						if (per_frame_available_model_transform_matrix_buffer[scene[i].current_frame_index_containing_most_updated_model_transform_matrix].size() == 0)
						{
							// Create a new buffer
							per_frame_model_transform_matrix_buffers[scene[i].current_frame_index_containing_most_updated_model_transform_matrix].push_back(Microsoft::WRL::ComPtr<ID3D12Resource>());
							per_frame_model_transform_matrix_upload_buffers[scene[i].current_frame_index_containing_most_updated_model_transform_matrix].push_back(Microsoft::WRL::ComPtr<ID3D12Resource>());
							per_frame_model_transform_matrix_buffer_free_slots[scene[i].current_frame_index_containing_most_updated_model_transform_matrix].push_back(vector<UINT>());
							per_frame_available_model_transform_matrix_buffer[scene[i].current_frame_index_containing_most_updated_model_transform_matrix].push_back(per_frame_model_transform_matrix_buffer_free_slots[scene[i].current_frame_index_containing_most_updated_model_transform_matrix].size() - 1);
							auto& bufferFreeSlots = per_frame_model_transform_matrix_buffer_free_slots[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][per_frame_model_transform_matrix_buffer_free_slots[scene[i].current_frame_index_containing_most_updated_model_transform_matrix].size() - 1];
							bufferFreeSlots.reserve(MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES);
							for (int j = 0; j < MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES; j++)
							{
								bufferFreeSlots.emplace_back(j);
							}

							auto& newlyCreatedBuffer = per_frame_model_transform_matrix_buffers[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][per_frame_model_transform_matrix_buffers[scene[i].current_frame_index_containing_most_updated_model_transform_matrix].size() - 1];
							auto& newlyCreatedUploadBuffer = per_frame_model_transform_matrix_upload_buffers[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][per_frame_model_transform_matrix_upload_buffers[scene[i].current_frame_index_containing_most_updated_model_transform_matrix].size() - 1];
							DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
								&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
								D3D12_HEAP_FLAG_NONE,
								&CD3DX12_RESOURCE_DESC::Buffer(c_aligned_model_transform_matrix_constant_buffer),
								D3D12_RESOURCE_STATE_COPY_DEST,
								nullptr,
								IID_PPV_ARGS(&newlyCreatedBuffer)));

							// Create an accompanying upload heap 
							DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
								&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
								D3D12_HEAP_FLAG_NONE,
								&CD3DX12_RESOURCE_DESC::Buffer(c_aligned_model_transform_matrix_constant_buffer),
								D3D12_RESOURCE_STATE_GENERIC_READ,
								nullptr,
								IID_PPV_ARGS(&newlyCreatedUploadBuffer)));

							UpdateSubresources(command_list_copy_high_priority.Get(),
								newlyCreatedBuffer.Get(),
								newlyCreatedUploadBuffer.Get(),
								0,
								0,
								static_cast<uint32_t>(default_model_transform_matrix_buffer_subresources.size()),
								default_model_transform_matrix_buffer_subresources.data());

							CD3DX12_RESOURCE_BARRIER modelTransformMatrixBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(newlyCreatedBuffer.Get(),
								D3D12_RESOURCE_STATE_COPY_DEST,
								D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
							command_list_copy_high_priority->ResourceBarrier(1, &modelTransformMatrixBufferBarrier);

							// Create a CBV for the buffer
							D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
							desc.BufferLocation = newlyCreatedBuffer->GetGPUVirtualAddress();
							desc.SizeInBytes = c_aligned_model_transform_matrix_constant_buffer;
							device_resources->GetD3DDevice()->CreateConstantBufferView(&desc, cbv_srv_cpu_handle);
							cbv_srv_cpu_handle.Offset(cbv_srv_descriptor_size);
						}
					}
				}

				XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(scene[i].local_rotation.x),
					XMConvertToRadians(scene[i].local_rotation.y),
					XMConvertToRadians(scene[i].local_rotation.z));
				XMMATRIX translationMatrix = XMMatrixTranslation(scene[i].world_position.x,
					scene[i].world_position.y,
					scene[i].world_position.z);

				// Next assign the footprint to the array describing all the subresources being updated for the buffer this
				// scene object was assigned to
				auto unorderedMapLookUpResult = per_model_transform_matrix_buffer_update_data.find(scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][0]);
				ModelTransformMatrixBufferUpdateData* pModelTransformMatrixBufferUpdateData;
				// Check if this is the first subresource update being applied to this buffer on this frame
				if (unorderedMapLookUpResult == per_model_transform_matrix_buffer_update_data.end())
				{
					auto newMapElement = make_pair(scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][0], ModelTransformMatrixBufferUpdateData());
					pModelTransformMatrixBufferUpdateData = &newMapElement.second;
					per_model_transform_matrix_buffer_update_data.insert(newMapElement);
				}
				else
				{
					pModelTransformMatrixBufferUpdateData = &unorderedMapLookUpResult->second;
				}

				// Create a new subresource footprint
				// Update the model_transform_matrix_placed_subresource_footprint's offset so that it targets this scene object's
				// model assigned slot
				model_transform_matrix_placed_subresource_footprint.Offset = c_aligned_model_transform_matrix * scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][1];
				pModelTransformMatrixBufferUpdateData->footprints.push_back(model_transform_matrix_placed_subresource_footprint);
				pModelTransformMatrixBufferUpdateData->subresources_data.push_back(ShaderStructureModelTransformMatrix());
				DirectX::XMStoreFloat4x4(&pModelTransformMatrixBufferUpdateData->subresources_data[pModelTransformMatrixBufferUpdateData->subresources_data.size() - 1].model, XMMatrixTranspose(XMMatrixMultiply(translationMatrix, rotationMatrix)));
				D3D12_SUBRESOURCE_DATA subresource;
				subresource.RowPitch = c_aligned_model_transform_matrix;
				subresource.SlicePitch = c_aligned_model_transform_matrix;
				subresource.pData = &pModelTransformMatrixBufferUpdateData->subresources_data[pModelTransformMatrixBufferUpdateData->subresources_data.size() - 1];
				pModelTransformMatrixBufferUpdateData->subresources.push_back(subresource);

				// Create transition barriers to go along with the footprints
				CD3DX12_RESOURCE_BARRIER subresourceTransitionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(per_frame_model_transform_matrix_buffers[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][0]].Get(),
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
					D3D12_RESOURCE_STATE_COPY_DEST, 
					scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][1],
					D3D12_RESOURCE_BARRIER_FLAG_NONE);
				CD3DX12_RESOURCE_BARRIER subresourceReturnBarrier = CD3DX12_RESOURCE_BARRIER::Transition(per_frame_model_transform_matrix_buffers[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][0]].Get(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
					scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].current_frame_index_containing_most_updated_model_transform_matrix][1],
					D3D12_RESOURCE_BARRIER_FLAG_NONE);
				model_transform_matrix_resource_barrier_transitions_batch_array.emplace_back(subresourceTransitionBarrier);
				model_transform_matrix_resource_barrier_returns_batch_array.emplace_back(subresourceReturnBarrier);

				scene[i].update_previous_frame_index_containing_most_updated_model_transform_matrix == true;
				scene[i].current_frame_index_containing_most_updated_model_transform_matrix++;
				if (scene[i].current_frame_index_containing_most_updated_model_transform_matrix == c_frame_count)
				{
					scene[i].current_frame_index_containing_most_updated_model_transform_matrix = 0;
				}
				
				// This way if the object was initialized as static
				// this would have been it's one and only model transform update.
				// After this it won't be updated anymore
				scene[i].is_static = scene[i].initialized_as_static;
			}
			
			// Draw the object only if it has a model transform matrix resident on the gpu
			if (scene[i].model_transform_matrix_residency == true)
			{
				// Set the model transform matrix buffer indexes
				command_list_direct->SetGraphicsRoot32BitConstants(0, 2, scene[i].per_frame_model_transform_matrix_buffer_indexes[scene[i].previous_frame_index_containing_most_updated_model_transform_matrix].data(), 1);
				// Bind the vertex and index buffer views
				command_list_direct->IASetVertexBuffers(0, 1, &scene[i].vertex_buffer_view);
				command_list_direct->IASetIndexBuffer(&scene[i].index_buffer_view);
				command_list_direct->DrawIndexedInstanced(scene[i].indices.size(), 1, 0, 0, 0);
			}
			else
			{
				// Because we will wait for the data to be resident on the next frame, we can set the residency here to true
				scene[i].model_transform_matrix_residency = true;
			}
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
					DX::ThrowIfFailed(command_list_copy_normal_priority->Reset(device_resources->GetCommandAllocatorCopyNormalPriority(), pipeline_state.Get()));
				}
				
				// Upload the vertex buffer to the GPU.
				const UINT vertexBufferSize = scene[i].vertices.size() * sizeof(VertexPositionNormalTexture);
				CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
				CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
				DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
					&defaultHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&vertexBufferDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&scene[i].vertex_buffer)));

				CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
				DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
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
				DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
					&defaultHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&indexBufferDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&scene[i].index_buffer)));

				DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommittedResource(
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

				scene[i].fence_value_signaling_required_resource_residency = fence_command_list_copy_normal_priority_progress_latest_unused_value;
			}
		}
	}

	// Check if we need to execute high priority copy command queue
	if (per_model_transform_matrix_buffer_update_data.empty() == false)
	{
		// Populate the command list
		for (auto const& element : per_model_transform_matrix_buffer_update_data)
		{
			// Transition all the subresources that will be updated
			command_list_copy_high_priority->ResourceBarrier(model_transform_matrix_resource_barrier_transitions_batch_array.size(), model_transform_matrix_resource_barrier_transitions_batch_array.data());
			UpdateSubresources(command_list_copy_high_priority.Get(),
				per_frame_model_transform_matrix_buffers[currentFrameIndex][element.first].Get(),
				per_frame_model_transform_matrix_upload_buffers[currentFrameIndex][element.first].Get(),
				0,
				element.second.footprints.size(),
				element.second.footprints.size() * c_aligned_model_transform_matrix,
				element.second.footprints.data(),
				model_transform_matrix_buffer_subresources_number_of_rows.data(),
				model_transform_matrix_buffer_subresources_row_size_in_bytes.data(),
				element.second.subresources.data());

			// Return all the subresources to their previous states, so that they can be read by the vertex shader
			command_list_copy_high_priority->ResourceBarrier(model_transform_matrix_resource_barrier_returns_batch_array.size(), model_transform_matrix_resource_barrier_returns_batch_array.data());

		}


		command_allocator_copy_high_priority_already_reset = false;
		command_list_copy_high_priority_requires_reset = true;
		DX::ThrowIfFailed(command_list_copy_high_priority->Close());
		ID3D12CommandList* ppCommandLists[] = { command_list_copy_high_priority.Get() };
		device_resources->GetCommandQueueCopyNormalPriority()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		DX::ThrowIfFailed(device_resources->GetCommandQueueCopyHighPriority()->Signal(fence_command_list_copy_high_priority_progress.Get(), fence_command_list_copy_high_priority_progress_latest_unused_value));
		fence_per_frame_command_list_copy_high_priority_values[currentFrameIndex] = fence_command_list_copy_high_priority_progress_latest_unused_value;
		fence_command_list_copy_high_priority_progress_latest_unused_value++;
	}

	// Check if we need to execute normal priority copy command queue
	if (commandListCopyNormalPriorityRequiresExecution == true)
	{
		command_allocator_copy_normal_priority_already_reset = false;
		command_list_copy_normal_priority_requires_reset = true;
		DX::ThrowIfFailed(command_list_copy_normal_priority->Close());
		ID3D12CommandList* ppCommandLists[] = { command_list_copy_normal_priority.Get() };
		device_resources->GetCommandQueueCopyNormalPriority()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		
		DX::ThrowIfFailed(device_resources->GetCommandQueueCopyNormalPriority()->Signal(fence_command_list_copy_normal_priority_progress.Get(), fence_command_list_copy_normal_priority_progress_latest_unused_value));
		fence_command_list_copy_normal_priority_progress_latest_unused_value++;
	}

	// Wait for the previous frames model transform matrixes to be uploaded to the gpu
	if (fenceCommandListCopyHighPriorityProgressValue < fence_per_frame_command_list_copy_high_priority_values[previousFrameIndex])
	{
		DX::ThrowIfFailed(fence_command_list_copy_high_priority_progress->SetEventOnCompletion(fence_per_frame_command_list_copy_high_priority_values[previousFrameIndex], event_command_list_copy_high_priority_progress));
		WaitForSingleObjectEx(event_command_list_copy_high_priority_progress, INFINITE, FALSE);
	}

	return command_list_direct.Get();
}
