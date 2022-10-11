#include "Graphics\SceneRenderers\3DSceneRenderer.h"




// Indices into the application state map.
Platform::String^ AngleKey = "Angle";
Platform::String^ TrackingKey = "Tracking";

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
Sample3DSceneRenderer::Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, Camera& camera) :
	device_resources(deviceResources),
	m_radiansPerSecond(XM_PIDIV4),	// rotate 45 degrees per second
	m_angle(0),
	m_mappedConstantBuffer(nullptr),
	fence_copy_command_list_progress_highest_value(0),
	copy_command_allocator_already_reset(true),
	copy_command_list_requires_reset(false)
{
	LoadState();
	ZeroMemory(&m_constantBufferData, sizeof(m_constantBufferData));

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources(camera);
}

Sample3DSceneRenderer::~Sample3DSceneRenderer()
{
	m_constantBuffer->Unmap(0, nullptr);
	m_mappedConstantBuffer = nullptr;
}

void Sample3DSceneRenderer::CreateDeviceDependentResources()
{
	auto d3dDevice = device_resources->GetD3DDevice();
	// Create a root signature with a single constant buffer slot.
	CD3DX12_DESCRIPTOR_RANGE rangeCBV;
	CD3DX12_DESCRIPTOR_RANGE rangeSRV;
	CD3DX12_DESCRIPTOR_RANGE rangeSamplers;
	CD3DX12_ROOT_PARAMETER parameterVertexVisibleDescriptorTable;
	CD3DX12_ROOT_PARAMETER parameterPixelVisibleDescriptorTable;
	//CD3DX12_ROOT_PARAMETER parameterPixelVisibleDescriptorTableSamplers;

	rangeCBV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	rangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	//rangeSamplers.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
	
	parameterVertexVisibleDescriptorTable.InitAsDescriptorTable(1, &rangeCBV, D3D12_SHADER_VISIBILITY_VERTEX);
	parameterPixelVisibleDescriptorTable.InitAsDescriptorTable(1, &rangeSRV, D3D12_SHADER_VISIBILITY_PIXEL);
	//parameterPixelVisibleDescriptorTableSamplers.InitAsDescriptorTable(1, &rangeSamplers, D3D12_SHADER_VISIBILITY_PIXEL);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // Only the input assembler stage needs access to the constant buffer.
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
	CD3DX12_ROOT_PARAMETER parameterArray[2] = { parameterVertexVisibleDescriptorTable, parameterPixelVisibleDescriptorTable/*, parameterPixelVisibleDescriptorTableSamplers*/ };
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

	descRootSignature.Init(2, parameterArray, 1, &staticSamplerDesc, rootSignatureFlags);

	ComPtr<ID3DBlob> pSignature;
	ComPtr<ID3DBlob> pError;
	DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
	DX::ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    NAME_D3D12_OBJECT(m_rootSignature);

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
	state.pRootSignature = m_rootSignature.Get();
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

	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&m_pipelineState)));

	// Shader data can be deleted once the pipeline state is created.
	delete[] pVertexShadeByteCode;
	delete[] pPixelShadeByteCode;

	// Upload cube geometry resources to the GPU.
	// Create a direct command list.
	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, device_resources->GetDirectCommandAllocator(), m_pipelineState.Get(), IID_PPV_ARGS(&command_list_direct)));
	NAME_D3D12_OBJECT(command_list_direct);
	// Create a copy command list.
	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, device_resources->GetCopyCommandAllocator(), m_pipelineState.Get(), IID_PPV_ARGS(&command_list_copy)));
	NAME_D3D12_OBJECT(command_list_copy);
	// Create a fence to go along with the copy command list.
	// This fence will be used to signal when a resource has been successfully uploaded to the gpu
	DX::ThrowIfFailed(device_resources->GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_copy_command_list_progress)));
	NAME_D3D12_OBJECT(fence_copy_command_list_progress);	

	// Create a descriptor heap for the constant buffers and the test texture.
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = DX::c_frameCount + 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptor_heap_cbv_srv)));
	NAME_D3D12_OBJECT(descriptor_heap_cbv_srv);

	// Create the constant buffer
	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(DX::c_frameCount * c_alignedConstantBufferSize);
	DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&constantBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_constantBuffer)));


	NAME_D3D12_OBJECT(m_constantBuffer);

	// Create constant buffer views to access the upload buffer.
	D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = m_constantBuffer->GetGPUVirtualAddress();
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvCpuHandle(descriptor_heap_cbv_srv->GetCPUDescriptorHandleForHeapStart());
	cbv_srv_descriptor_size = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int n = 0; n < DX::c_frameCount; n++)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.BufferLocation = cbvGpuAddress;
		desc.SizeInBytes = c_alignedConstantBufferSize;
		d3dDevice->CreateConstantBufferView(&desc, cbvSrvCpuHandle);

		cbvGpuAddress += desc.SizeInBytes;
		cbvSrvCpuHandle.Offset(cbv_srv_descriptor_size);
	}

	// Map the constant buffers.
	// We don't unmap this until the app closes. Keeping things mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE readRange(0, 0);	// We do not intend to read from this resource on the CPU.
	DX::ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedConstantBuffer)));
	ZeroMemory(m_mappedConstantBuffer, DX::c_frameCount * c_alignedConstantBufferSize);

	// Create and upload the test texture
	TexMetadata metadata;
	ScratchImage scratchImage;
	DX::ThrowIfFailed(LoadFromWICFile((wStringInstallPath + L"\\Assets\\woah.jpg").c_str(), WIC_FLAGS::WIC_FLAGS_NONE, &metadata, scratchImage));
	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
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
	device_resources->GetD3DDevice()->CreateShaderResourceView(test_texture.Get(), &srvDesc, cbvSrvCpuHandle);

	// Close the command list and execute it to begin the vertex/index buffer copy into the GPU's default heap.
	DX::ThrowIfFailed(command_list_direct->Close());
	ID3D12CommandList* ppCommandLists[] = { command_list_direct.Get() };
	device_resources->GetDirectCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Wait for the command list to finish executing; the vertex/index buffers need to be uploaded to the GPU before the upload resources go out of scope.
	device_resources->WaitForGpu();
}

// Initializes view parameters when the window size changes.
void Sample3DSceneRenderer::CreateWindowSizeDependentResources(Camera& camera)
{
	D3D12_VIEWPORT viewport = device_resources->GetScreenViewport();
	m_scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height)};
	XMFLOAT4X4 orientation = device_resources->GetOrientationTransform3D();
	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);
	XMStoreFloat4x4(&m_constantBufferData.projection, XMMatrixTranspose(camera.GetProjectionMatrix() * orientationMatrix));
}


// Saves the current state of the renderer.
void Sample3DSceneRenderer::SaveState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;

	if (state->HasKey(AngleKey))
	{
		state->Remove(AngleKey);
	}

	state->Insert(AngleKey, PropertyValue::CreateSingle(m_angle));
}

// Restores the previous state of the renderer.
void Sample3DSceneRenderer::LoadState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;
	if (state->HasKey(AngleKey))
	{
		m_angle = safe_cast<IPropertyValue^>(state->Lookup(AngleKey))->GetSingle();
		state->Remove(AngleKey);
	}
}



// Renders one frame using the vertex and pixel shaders.
ID3D12GraphicsCommandList* Sample3DSceneRenderer::Render(vector<Mesh>& scene, Camera& camera, bool showImGui)
{
	DX::ThrowIfFailed(device_resources->GetDirectCommandAllocator()->Reset());
	// The command list can be reset anytime after ExecuteCommandList() is called.
	DX::ThrowIfFailed(command_list_direct->Reset(device_resources->GetDirectCommandAllocator(), m_pipelineState.Get()));
	// Update the view matrix
	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(camera.GetViewMatrix()));
	// Set the graphics root signature and descriptor heaps to be used by this frame.
	command_list_direct->SetGraphicsRootSignature(m_rootSignature.Get());
	ID3D12DescriptorHeap* ppHeaps[] = { descriptor_heap_cbv_srv.Get()/*, descriptor_heap_sampler.Get()*/};
	command_list_direct->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// Bind the descriptor tables to their respective heap starts
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(descriptor_heap_cbv_srv->GetGPUDescriptorHandleForHeapStart(), device_resources->GetCurrentFrameIndex(), cbv_srv_descriptor_size);
	command_list_direct->SetGraphicsRootDescriptorTable(0, gpuHandle);
	gpuHandle.Offset((DX::c_frameCount - device_resources->GetCurrentFrameIndex()) * cbv_srv_descriptor_size);
	command_list_direct->SetGraphicsRootDescriptorTable(1, gpuHandle);
	//command_list_direct->SetGraphicsRootDescriptorTable(2, descriptor_heap_sampler->GetGPUDescriptorHandleForHeapStart());

	// Set the viewport and scissor rectangle.
	D3D12_VIEWPORT viewport = device_resources->GetScreenViewport();
	command_list_direct->RSSetViewports(1, &viewport);
	command_list_direct->RSSetScissorRects(1, &m_scissorRect);

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
	bool copyCommandListRequiresExecution = false;
	UINT64 fenceCopyCommandListProgressValue = fence_copy_command_list_progress->GetCompletedValue();
	// Reset the copy command allocator if there are no command lists executing on it
	if ((fence_copy_command_list_progress_highest_value + 1) == fenceCopyCommandListProgressValue)
	{
		if (copy_command_allocator_already_reset == false)
		{
			copy_command_allocator_already_reset = true;
			DX::ThrowIfFailed(device_resources->GetCopyCommandAllocator()->Reset());
		}
	}
	
	for (int i = 0; i < 1; i++)
	{
		if (scene[i].fence_value_signaling_buffers_are_present_on_the_gpu < fenceCopyCommandListProgressValue)
		{
			// If the data has been uploaded to the gpu, there is no more need for the upload buffers
			// Also since this is the first time this mesh is being renderer, we need to create 
			// the vertex and index buffer views
			if (scene[i].buffer_upload_started == true)
			{
				scene[i].vertex_buffer_upload.Reset();
				scene[i].index_buffer_upload.Reset();
				scene[i].buffer_upload_started = false;

				// Create vertex/index views
				scene[i].vertex_buffer_view.BufferLocation = scene[i].vertex_buffer->GetGPUVirtualAddress();
				scene[i].vertex_buffer_view.StrideInBytes = sizeof(VertexPositionNormalTexture);
				scene[i].vertex_buffer_view.SizeInBytes = scene[i].vertices.size() * sizeof(VertexPositionNormalTexture);

				scene[i].index_buffer_view.BufferLocation = scene[i].index_buffer->GetGPUVirtualAddress();
				scene[i].index_buffer_view.SizeInBytes = scene[i].indices.size() * sizeof(uint16_t);
				scene[i].index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
			}

			// Update the model matrix
			// @@@@@@@@@@@@@@@@@@@@!!!!!!!!!!!!!!!!!!
			// @@@@@@@@@@@@@@@@@@@@!!!!!!!!!!!!!!!!!!
			// POSSIBLY WILL NEED TO MOVE THE PER OBJECT MODEL MATRIXES TO A BUFFER WHEN TESTING OUT WITH MULTIPLE OBJECTS
			// IT IS POSSIBLE THAT CHANGING THE CONSTANT BUFFER MAPPED DATA CAN BE DONE ONLY ONCE PER FRAME
			// BECAUSE OF THE SPEED AT WHICH THE DATA WILL BE UPLOADED TO THE GPU.
			// WE CAN FIX THIS IF WE UPLOAD EACH OBJECTS MODEL MATRIX TO A BUFFER AND THEN INDEX INTO IT FROM INSIDE A SHADER
			XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(scene[i].local_rotation.x),
																	XMConvertToRadians(scene[i].local_rotation.y),
																	 XMConvertToRadians(scene[i].local_rotation.z));
			XMMATRIX translationMatrix = XMMatrixTranslation(scene[i].world_position.x,
															  scene[i].world_position.y,
														       scene[i].world_position.z);
			XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(translationMatrix * rotationMatrix));
			UINT8* destination = m_mappedConstantBuffer + (device_resources->GetCurrentFrameIndex() * c_alignedConstantBufferSize);
			memcpy(destination, &m_constantBufferData, sizeof(m_constantBufferData));

			// Bind the vertex and index buffer views
			command_list_direct->IASetVertexBuffers(0, 1, &scene[i].vertex_buffer_view);
			command_list_direct->IASetIndexBuffer(&scene[i].index_buffer_view);
			command_list_direct->DrawIndexedInstanced(scene[i].indices.size(), 1, 0, 0, 0);
		}
		else
		{
			if (scene[i].buffer_upload_started == false)
			{
				scene[i].buffer_upload_started = true;
				copyCommandListRequiresExecution = true;
				if (copy_command_list_requires_reset == true)
				{
					copy_command_list_requires_reset = false;
					DX::ThrowIfFailed(command_list_copy->Reset(device_resources->GetCopyCommandAllocator(), m_pipelineState.Get()));
					fence_copy_command_list_progress_highest_value++;
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

				UpdateSubresources(command_list_copy.Get(), scene[i].vertex_buffer.Get(), scene[i].vertex_buffer_upload.Get(), 0, 0, 1, &vertexData);

				//CD3DX12_RESOURCE_BARRIER vertexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(scene[i].vertex_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
				//command_list_copy->ResourceBarrier(1, &vertexBufferResourceBarrier);
				

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

				UpdateSubresources(command_list_copy.Get(), scene[i].index_buffer.Get(), scene[i].index_buffer_upload.Get(), 0, 0, 1, &indexData);

				//CD3DX12_RESOURCE_BARRIER indexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(scene[i].index_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
				//command_list_copy->ResourceBarrier(1, &indexBufferResourceBarrier);
				
				scene[i].fence_value_signaling_buffers_are_present_on_the_gpu = fence_copy_command_list_progress_highest_value;
			}
		}
		
	}

	// Check if we need to execute the copy command queue
	if (copyCommandListRequiresExecution == true)
	{
		copy_command_allocator_already_reset = false;
		copy_command_list_requires_reset = true;
		DX::ThrowIfFailed(command_list_copy->Close());
		ID3D12CommandList* ppCommandLists[] = { command_list_copy.Get() };
		device_resources->GetCopyCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		DX::ThrowIfFailed(device_resources->GetCopyCommandQueue()->Signal(fence_copy_command_list_progress.Get(), fence_copy_command_list_progress_highest_value + 1));
	}

	return command_list_direct.Get();
}
