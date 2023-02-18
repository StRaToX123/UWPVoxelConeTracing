#include "Graphics\SceneRenderers\SceneRendererDirectLightingVoxelGIandAO.h"


static const float gs_clear_color_black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
static const float gs_clear_color_white[] = { 1.0f, 1.0f, 1.0f, 1.0f };

SceneRendererDirectLightingVoxelGIandAO::SceneRendererDirectLightingVoxelGIandAO()
{
	p_constant_buffer_voxelization = nullptr;
	p_constant_buffer_vct_main = nullptr;
	p_render_target_vct_voxelization = nullptr;

}

SceneRendererDirectLightingVoxelGIandAO::~SceneRendererDirectLightingVoxelGIandAO()
{
	if (p_constant_buffer_voxelization != nullptr)
	{
		delete p_constant_buffer_voxelization;
	}

	if (p_constant_buffer_vct_main != nullptr)
	{
		delete p_constant_buffer_vct_main;
	}

	if (p_render_target_vct_voxelization != nullptr)
	{
		delete p_render_target_vct_voxelization;
	}

	if (P_constant_buffers_vct_anisotropic_mip_generation.size() > 0)
	{
		for (UINT i = 0; i < P_constant_buffers_vct_anisotropic_mip_generation.size(); i++)
		{
			delete P_constant_buffers_vct_anisotropic_mip_generation[i];
		}
	}

	if (P_render_targets_gbuffer.size() > 0)
	{
		for (UINT i = 0; i < P_render_targets_gbuffer.size(); i++)
		{
			delete P_render_targets_gbuffer[i];
		}
	}

	delete mLightingCB;
	delete mLightingRT;
}

void SceneRendererDirectLightingVoxelGIandAO::Initialize(Windows::UI::Core::CoreWindow^ coreWindow)
{
	DX12DeviceResourcesSingleton* _deviceResources = DX12DeviceResourcesSingleton::GetDX12DeviceResources();
	descriptor_heap_manager.Initialize();

	ID3D12Device* _d3dDevice = _deviceResources->GetD3DDevice();

	fence_unused_value_direct_queue = 1;
	ThrowIfFailed(_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_direct_queue)));
	fence_unused_value_compute_queue = 1;
	ThrowIfFailed(_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_compute_queue)));
	fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
	
	auto size = _deviceResources->GetOutputSize();
	float aspectRatio = float(size.right) / float(size.bottom);

	#pragma region States
	mDepthStateRW.DepthEnable = TRUE;
	mDepthStateRW.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	mDepthStateRW.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	mDepthStateRW.StencilEnable = FALSE;
	mDepthStateRW.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	mDepthStateRW.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	mDepthStateRW.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	mDepthStateRW.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	mDepthStateRW.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mDepthStateRW.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mDepthStateRW.BackFace = mDepthStateRW.FrontFace;

	mDepthStateDisabled.DepthEnable = FALSE;
	mDepthStateDisabled.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	mDepthStateDisabled.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	mDepthStateDisabled.StencilEnable = FALSE;
	mDepthStateDisabled.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	mDepthStateDisabled.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	mDepthStateDisabled.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	mDepthStateDisabled.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	mDepthStateDisabled.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mDepthStateDisabled.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mDepthStateDisabled.BackFace = mDepthStateRW.FrontFace;

	mBlendState.IndependentBlendEnable = FALSE;
	mBlendState.RenderTarget[0].BlendEnable = FALSE;
	mBlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	mBlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	mBlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	mBlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	mBlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	mBlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	mBlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	mRasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	mRasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	mRasterizerState.FrontCounterClockwise = FALSE;
	mRasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	mRasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	mRasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	mRasterizerState.DepthClipEnable = TRUE;
	mRasterizerState.MultisampleEnable = FALSE;
	mRasterizerState.AntialiasedLineEnable = FALSE;
	mRasterizerState.ForcedSampleCount = 0;
	mRasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	mRasterizerStateNoCullNoDepth.FillMode = D3D12_FILL_MODE_SOLID;
	mRasterizerStateNoCullNoDepth.CullMode = D3D12_CULL_MODE_NONE;
	mRasterizerStateNoCullNoDepth.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	mRasterizerStateNoCullNoDepth.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	mRasterizerStateNoCullNoDepth.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	mRasterizerStateNoCullNoDepth.DepthClipEnable = FALSE;
	mRasterizerStateNoCullNoDepth.MultisampleEnable = FALSE;
	mRasterizerStateNoCullNoDepth.AntialiasedLineEnable = FALSE;
	mRasterizerStateNoCullNoDepth.ForcedSampleCount = 0;
	mRasterizerStateNoCullNoDepth.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	mRasterizerStateShadow.FillMode = D3D12_FILL_MODE_SOLID;
	mRasterizerStateShadow.CullMode = D3D12_CULL_MODE_BACK;
	mRasterizerStateShadow.FrontCounterClockwise = FALSE;
	mRasterizerStateShadow.SlopeScaledDepthBias = 10.0f;
	mRasterizerStateShadow.DepthBias = 0.05f;
	mRasterizerStateShadow.DepthClipEnable = FALSE;
	mRasterizerStateShadow.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	mRasterizerStateShadow.MultisampleEnable = FALSE;
	mRasterizerStateShadow.AntialiasedLineEnable = FALSE;
	mRasterizerStateShadow.ForcedSampleCount = 0;
	mRasterizerStateShadow.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	mBilinearSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	mBilinearSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	mBilinearSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	mBilinearSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	mBilinearSampler.MipLODBias = 0;
	mBilinearSampler.MaxAnisotropy = 16;
	mBilinearSampler.MinLOD = 0.0f;
	mBilinearSampler.MaxLOD = D3D12_FLOAT32_MAX;
	mBilinearSampler.BorderColor[0] = 0.0f;
	mBilinearSampler.BorderColor[1] = 0.0f;
	mBilinearSampler.BorderColor[2] = 0.0f;
	mBilinearSampler.BorderColor[3] = 0.0f;
	#pragma endregion

	// Set the most updated indexes for the constant buffers
	shader_structure_cpu_vct_main_data_most_updated_index = _deviceResources->GetBackBufferCount() - 1;
	shader_structure_cpu_illumination_flags_most_updated_index = shader_structure_cpu_vct_main_data_most_updated_index;
	shader_structure_cpu_voxelization_data_most_updated_index = shader_structure_cpu_vct_main_data_most_updated_index;
	shader_structure_cpu_lighting_data_most_updated_index = shader_structure_cpu_vct_main_data_most_updated_index;
	shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index = shader_structure_cpu_vct_main_data_most_updated_index;

	InitGbuffer(_d3dDevice, &descriptor_heap_manager);
	InitShadowMapping(_d3dDevice, &descriptor_heap_manager);
	InitVoxelConeTracing(_deviceResources, _d3dDevice, &descriptor_heap_manager);
	InitLighting(_deviceResources, _d3dDevice, &descriptor_heap_manager);
	InitComposite(_deviceResources, _d3dDevice, &descriptor_heap_manager);
	UpdateBuffers(UpdatableBuffers::ILLUMINATION_FLAGS_DATA_BUFFER);
	UpdateBuffers(UpdatableBuffers::VCT_MAIN_DATA_BUFFER);
	UpdateBuffers(UpdatableBuffers::VOXELIZATION_DATA_BUFFER);
	UpdateBuffers(UpdatableBuffers::LIGHTING_DATA_BUFFER);
}

void SceneRendererDirectLightingVoxelGIandAO::UpdateBuffers(UpdatableBuffers whichBufferToUpdate)
{
	DX12DeviceResourcesSingleton* _deviceResources = DX12DeviceResourcesSingleton::GetDX12DeviceResources();
	switch (whichBufferToUpdate)
	{
		case UpdatableBuffers::ILLUMINATION_FLAGS_DATA_BUFFER:
		{
			shader_structure_cpu_illumination_flags_most_updated_index++;
			if (shader_structure_cpu_illumination_flags_most_updated_index == _deviceResources->GetBackBufferCount())
			{
				shader_structure_cpu_illumination_flags_most_updated_index = 0;
			}

			memcpy(mIlluminationFlagsCB->GetMappedData(shader_structure_cpu_illumination_flags_most_updated_index), &shader_structure_cpu_illumination_flags_data, sizeof(ShaderStructureCPUIlluminationFlagsData));
			break;
		}

		case UpdatableBuffers::VCT_MAIN_DATA_BUFFER:
		{
			shader_structure_cpu_vct_main_data_most_updated_index++;
			if (shader_structure_cpu_vct_main_data_most_updated_index == _deviceResources->GetBackBufferCount())
			{
				shader_structure_cpu_vct_main_data_most_updated_index = 0;
			}

			shader_structure_cpu_vct_main_data.upsample_ratio = DirectX::XMFLOAT2(P_render_targets_gbuffer[0]->GetWidth() / mVCTMainRT->GetWidth(), P_render_targets_gbuffer[0]->GetHeight() / mVCTMainRT->GetHeight());
			memcpy(p_constant_buffer_vct_main->GetMappedData(shader_structure_cpu_vct_main_data_most_updated_index), &shader_structure_cpu_vct_main_data, sizeof(ShaderStructureCPUVCTMainData));
			break;
		}

		case UpdatableBuffers::VOXELIZATION_DATA_BUFFER:
		{
			shader_structure_cpu_voxelization_data_most_updated_index++;
			if (shader_structure_cpu_voxelization_data_most_updated_index == _deviceResources->GetBackBufferCount())
			{
				shader_structure_cpu_voxelization_data_most_updated_index = 0;
			}

			shader_structure_cpu_voxelization_data.voxel_grid_top_left_back_point_world_space.x = -(shader_structure_cpu_voxelization_data.voxel_grid_extent_world_space * 0.5f);
			shader_structure_cpu_voxelization_data.voxel_grid_top_left_back_point_world_space.y = -shader_structure_cpu_voxelization_data.voxel_grid_top_left_back_point_world_space.x;
			shader_structure_cpu_voxelization_data.voxel_grid_top_left_back_point_world_space.z = shader_structure_cpu_voxelization_data.voxel_grid_top_left_back_point_world_space.x;
			shader_structure_cpu_voxelization_data.voxel_scale = shader_structure_cpu_voxelization_data.voxel_grid_res * 0.5f;
			shader_structure_cpu_voxelization_data.voxel_extent_rcp = 1.0f / (shader_structure_cpu_voxelization_data.voxel_grid_extent_world_space / shader_structure_cpu_voxelization_data.voxel_grid_res);
			shader_structure_cpu_voxelization_data.voxel_grid_half_extent_world_space_rcp = 1.0f / (shader_structure_cpu_voxelization_data.voxel_grid_extent_world_space * 0.5f);
			// For the mip count we don't want to count the 1x1x1 mip since the anisotropic mip chain generation 
			// requires at least a 2x2x2 texture in order to perform calculations
			// We also don't want to count the mip level 0 (full res mip) since the anisotropic mip chain starts from what would
			// be the original 3D textures mip level 1
			shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count = (UINT32)log2(shader_structure_cpu_voxelization_data.voxel_grid_res) - 1;
			memcpy(p_constant_buffer_voxelization->GetMappedData(shader_structure_cpu_voxelization_data_most_updated_index), &shader_structure_cpu_voxelization_data, sizeof(ShaderStructureCPUVoxelizationData));
			break;
		}

		case UpdatableBuffers::LIGHTING_DATA_BUFFER:
		{
			shader_structure_cpu_lighting_data_most_updated_index++;
			if (shader_structure_cpu_lighting_data_most_updated_index == _deviceResources->GetBackBufferCount())
			{
				shader_structure_cpu_lighting_data_most_updated_index = 0;
			}

			shader_structure_cpu_lighting_data.shadow_texel_size = XMFLOAT2(1.0f / mShadowDepth->GetWidth(), 1.0f / mShadowDepth->GetHeight());
			memcpy(mLightingCB->GetMappedData(shader_structure_cpu_lighting_data_most_updated_index), &shader_structure_cpu_lighting_data, sizeof(ShaderStructureCPUVoxelizationData));
			break;
		}
	}
}

// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
// This function expects to be given an already reset direct command list and a an already reset compute command list.
// When returning the functions leaves the direct command list in a reset state and the compute command list in an unreset state
// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
void SceneRendererDirectLightingVoxelGIandAO::Render(std::vector<Model*>& scene,
	DirectionalLight& directionalLight,
	DXRSTimer& mTimer,
	Camera& camera,
	D3D12_VERTEX_BUFFER_VIEW& fullScreenQuadVertexBufferView,
	ID3D12GraphicsCommandList* _commandListDirect,
	ID3D12GraphicsCommandList* _commandListCompute,
	ID3D12CommandAllocator* _commandAllocatorDirect)
{	
	// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
	// This function expects to be given an already reset direct command list and a an already reset compute command list.
	// When returning the functions leaves the direct command list in a reset state and the compute command list in an unreset state
	// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!

	DX12DeviceResourcesSingleton* _deviceResources = DX12DeviceResourcesSingleton::GetDX12DeviceResources();
	ID3D12CommandList* __commandListsDirect[] = { _commandListDirect };
	ID3D12CommandList* __commandListsCompute[] = { _commandListCompute };
	auto _d3dDevice = _deviceResources->GetD3DDevice();
	DX12DescriptorHeapGPU* gpuDescriptorHeap = descriptor_heap_manager.GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gpuDescriptorHeap->Reset();
	ID3D12DescriptorHeap* ppHeaps[] = { gpuDescriptorHeap->GetHeap() };
	
	DX12DescriptorHandleBlock modelDataDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock(scene.size());
	for (int i = 0; i < scene.size(); i++)
	{
		modelDataDescriptorHandleBlock.Add(scene[i]->GetCBV());
	}

	DX12DescriptorHandleBlock cameraDataDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
	cameraDataDescriptorHandleBlock.Add(camera.GetCBV());

	DX12DescriptorHandleBlock directionalLightDataDescriptorHeapBlock = gpuDescriptorHeap->GetHandleBlock();
	directionalLightDataDescriptorHeapBlock.Add(directionalLight.GetCBV());

	DX12DescriptorHandleBlock shadowDepthDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
	shadowDepthDescriptorHandleBlock.Add(mShadowDepth->GetSRV());

	// Graphics command list
	{
		_commandListDirect->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		_commandListDirect->ClearDepthStencilView(_deviceResources->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		RenderGbuffer(_deviceResources, _d3dDevice, _commandListDirect, gpuDescriptorHeap, scene, modelDataDescriptorHandleBlock, cameraDataDescriptorHandleBlock);
		RenderShadowMapping(_deviceResources, _d3dDevice, _commandListDirect, gpuDescriptorHeap, scene, modelDataDescriptorHandleBlock, directionalLightDataDescriptorHeapBlock);
		RenderVoxelConeTracing(_deviceResources, 
			_d3dDevice, 
			_commandListDirect, 
			gpuDescriptorHeap, 
			scene, 
			modelDataDescriptorHandleBlock, 
			directionalLightDataDescriptorHeapBlock,
			cameraDataDescriptorHandleBlock,
			shadowDepthDescriptorHandleBlock,
			GRAPHICS_QUEUE); // Only voxelization there which can't go to compute

		// We have to transition the GBuffers here inside of the graphics command list to a non pixel shader resource,
		// because the compute command list cannot transition resources from a state of a render target
		_deviceResources->ResourceBarriersBegin(barriers);
		P_render_targets_gbuffer[0]->TransitionTo(barriers, _commandListDirect, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		P_render_targets_gbuffer[1]->TransitionTo(barriers, _commandListDirect, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		P_render_targets_gbuffer[2]->TransitionTo(barriers, _commandListDirect, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		_deviceResources->ResourceBarriersEnd(barriers, _commandListDirect);

		_commandListDirect->Close();
		_deviceResources->GetCommandQueueDirect()->ExecuteCommandLists(1, __commandListsDirect);
		_deviceResources->GetCommandQueueDirect()->Signal(fence_direct_queue.Get(), fence_unused_value_direct_queue);
		fence_unused_value_direct_queue++;
	}

	// Compute command list
	{
		_commandListCompute->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		RenderVoxelConeTracing(_deviceResources,
			_d3dDevice,
			_commandListCompute,
			gpuDescriptorHeap,
			scene,
			modelDataDescriptorHandleBlock,
			directionalLightDataDescriptorHeapBlock,
			cameraDataDescriptorHandleBlock,
			shadowDepthDescriptorHandleBlock,
			COMPUTE_QUEUE);

		_deviceResources->GetCommandQueueCompute()->Wait(fence_direct_queue.Get(), fence_unused_value_direct_queue - 1);
		_commandListCompute->Close();
		_deviceResources->GetCommandQueueCompute()->ExecuteCommandLists(1, __commandListsCompute);
		_deviceResources->GetCommandQueueCompute()->Signal(fence_compute_queue.Get(), fence_unused_value_compute_queue);
		fence_unused_value_compute_queue++;
	}

	// Submit the rest of the frame to graphics command list
	{
		_commandListDirect->Reset(_commandAllocatorDirect, nullptr);
		_commandListDirect->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		RenderLighting(_deviceResources,
			_d3dDevice,
			_commandListDirect,
			gpuDescriptorHeap,
			directionalLightDataDescriptorHeapBlock,
			cameraDataDescriptorHandleBlock,
			shadowDepthDescriptorHandleBlock,
			fullScreenQuadVertexBufferView);
		RenderComposite(_deviceResources, 
			_d3dDevice, 
			_commandListDirect, 
			gpuDescriptorHeap, 
			fullScreenQuadVertexBufferView);

		// We need to transition these resources here inside of the graphics command list,
		// since the compute command list will not allow us to transition a resource 
		// from a pixel shader resource state.
		_deviceResources->ResourceBarriersBegin(barriers);
		mVCTMainRT->TransitionTo(barriers, _commandListDirect, D3D12_RESOURCE_STATE_COMMON);
		mVCTMainUpsampleAndBlurRT->TransitionTo(barriers, _commandListDirect, D3D12_RESOURCE_STATE_COMMON);
		_deviceResources->ResourceBarriersEnd(barriers, _commandListDirect);

		_deviceResources->GetCommandQueueDirect()->Wait(fence_compute_queue.Get(), fence_unused_value_compute_queue - 1);
	}
}

void SceneRendererDirectLightingVoxelGIandAO::ThrowFailedErrorBlob(ID3DBlob* blob)
{
	std::string message = "";
	if (blob)
		message.append((char*)blob->GetBufferPointer());

	throw std::runtime_error(message.c_str());
}

void SceneRendererDirectLightingVoxelGIandAO::InitGbuffer(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager)
{
	D3D12_SAMPLER_DESC sampler;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;

	DXGI_FORMAT rtFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	P_render_targets_gbuffer.push_back(new DX12RenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, rtFormat, flags, L"Albedo"));

	rtFormat = DXGI_FORMAT_R16G16B16A16_SNORM;
	P_render_targets_gbuffer.push_back(new DX12RenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, rtFormat, flags, L"Normals"));

	rtFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	P_render_targets_gbuffer.push_back(new DX12RenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, rtFormat, flags, L"World Positions"));

	// Root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	root_signature_gbuffer.Reset(2, 1);
	root_signature_gbuffer.InitStaticSampler(0, sampler, D3D12_SHADER_VISIBILITY_PIXEL);
	root_signature_gbuffer[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
	root_signature_gbuffer[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, D3D12_SHADER_VISIBILITY_ALL);
	root_signature_gbuffer.Finalize(device, L"GPrepassRS", rootSignatureFlags);

	char* pVertexShaderByteCode;
	int vertexShaderByteCodeLength;
	char* pPixelShaderByteCode;
	int pixelShaderByteCodeLength;
	LoadShaderByteCode(GetFilePath("GBuffer_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode(GetFilePath("GBuffer_PS.cso"), pPixelShaderByteCode, pixelShaderByteCodeLength);

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Describe and create the graphics pipeline state object (PSO).
	DXGI_FORMAT formats[3];
	formats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	formats[1] = DXGI_FORMAT_R16G16B16A16_SNORM;
	formats[2] = DXGI_FORMAT_R32G32B32A32_FLOAT;

	pipeline_state_gbuffer.SetRootSignature(root_signature_gbuffer);
	pipeline_state_gbuffer.SetRasterizerState(mRasterizerState);
	pipeline_state_gbuffer.SetBlendState(mBlendState);
	pipeline_state_gbuffer.SetDepthStencilState(mDepthStateRW);
	pipeline_state_gbuffer.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	pipeline_state_gbuffer.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	pipeline_state_gbuffer.SetRenderTargetFormats(_countof(formats), formats, DXGI_FORMAT_D32_FLOAT);
	pipeline_state_gbuffer.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
	pipeline_state_gbuffer.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
	pipeline_state_gbuffer.Finalize(device);

	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
}
void SceneRendererDirectLightingVoxelGIandAO::RenderGbuffer(DX12DeviceResourcesSingleton* _deviceResources,
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* commandList, 
	DX12DescriptorHeapGPU* gpuDescriptorHeap, 
	std::vector<Model*>& scene,
	DX12DescriptorHandleBlock& modelDataDescriptorHandleBlock,
	DX12DescriptorHandleBlock& cameraDataDescriptorHandleBlock)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);

	{
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &rect);

		commandList->SetPipelineState(pipeline_state_gbuffer.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(root_signature_gbuffer.GetSignature());

		// Transition buffers to rendertarget outputs
		_deviceResources->ResourceBarriersBegin(barriers);
		P_render_targets_gbuffer[0]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		P_render_targets_gbuffer[1]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		P_render_targets_gbuffer[2]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		_deviceResources->ResourceBarriersEnd(barriers, commandList);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] =
		{
			P_render_targets_gbuffer[0]->GetRTV(),
			P_render_targets_gbuffer[1]->GetRTV(),
			P_render_targets_gbuffer[2]->GetRTV(),
		};

		commandList->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, FALSE, &_deviceResources->GetDepthStencilView());
		commandList->ClearRenderTargetView(rtvHandles[0], gs_clear_color_black, 0, nullptr);
		commandList->ClearRenderTargetView(rtvHandles[1], gs_clear_color_black, 0, nullptr);
		commandList->ClearRenderTargetView(rtvHandles[2], gs_clear_color_black, 0, nullptr);
		
		commandList->SetGraphicsRootDescriptorTable(0, cameraDataDescriptorHandleBlock.GetGPUHandle());
		CD3DX12_GPU_DESCRIPTOR_HANDLE modelDataGPUdescriptorHandle = modelDataDescriptorHandleBlock.GetGPUHandle();
		for (UINT i = 0; i < scene.size(); i++) 
		{				
			commandList->SetGraphicsRootDescriptorTable(1, modelDataGPUdescriptorHandle);
			modelDataGPUdescriptorHandle.Offset(gpuDescriptorHeap->GetDescriptorSize());
			scene[i]->Render(commandList);
		}
	}
}

void SceneRendererDirectLightingVoxelGIandAO::InitShadowMapping(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager)
// Create resources for shadow mapping
{
	mShadowDepth = new DXRSDepthBuffer(device, descriptorManager, SHADOWMAP_SIZE, SHADOWMAP_SIZE, DXGI_FORMAT_D32_FLOAT);

	// Create root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	mShadowMappingRS.Reset(2, 0);
	mShadowMappingRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
	mShadowMappingRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, D3D12_SHADER_VISIBILITY_VERTEX);
	mShadowMappingRS.Finalize(device, L"Shadow Mapping pass RS", rootSignatureFlags);

	char* pVertexShaderByteCode;
	int vertexShaderByteCodeLength;
	LoadShaderByteCode(GetFilePath("ShadowMapping_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mShadowMappingPSO.SetRootSignature(mShadowMappingRS);
	mShadowMappingPSO.SetRasterizerState(mRasterizerStateShadow);
	mShadowMappingPSO.SetRenderTargetFormats(0, nullptr, mShadowDepth->GetFormat());
	mShadowMappingPSO.SetBlendState(mBlendState);
	mShadowMappingPSO.SetDepthStencilState(mDepthStateRW);
	mShadowMappingPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	mShadowMappingPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	mShadowMappingPSO.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
	mShadowMappingPSO.Finalize(device);

	delete[] pVertexShaderByteCode;
}
void SceneRendererDirectLightingVoxelGIandAO::RenderShadowMapping(DX12DeviceResourcesSingleton* _deviceResources, 
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* commandList, 
	DX12DescriptorHeapGPU* gpuDescriptorHeap, 
	std::vector<Model*>& scene,
	DX12DescriptorHandleBlock& modelDataDescriptorHandleBlock, 
	DX12DescriptorHandleBlock& directionalLightDataDescriptorHeapBlock)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);

	{
		CD3DX12_VIEWPORT shadowMapViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, mShadowDepth->GetWidth(), mShadowDepth->GetHeight());
		CD3DX12_RECT shadowRect = CD3DX12_RECT(0.0f, 0.0f, mShadowDepth->GetWidth(), mShadowDepth->GetHeight());
		commandList->RSSetViewports(1, &shadowMapViewport);
		commandList->RSSetScissorRects(1, &shadowRect);

		commandList->SetPipelineState(mShadowMappingPSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mShadowMappingRS.GetSignature());

		_deviceResources->ResourceBarriersBegin(barriers);
		mShadowDepth->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		_deviceResources->ResourceBarriersEnd(barriers, commandList);

		commandList->OMSetRenderTargets(0, nullptr, FALSE, &mShadowDepth->GetDSV().GetCPUHandle());
		commandList->ClearDepthStencilView(mShadowDepth->GetDSV().GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		commandList->SetGraphicsRootDescriptorTable(0, directionalLightDataDescriptorHeapBlock.GetGPUHandle());
		CD3DX12_GPU_DESCRIPTOR_HANDLE modelDataGPUdescriptorHandle = modelDataDescriptorHandleBlock.GetGPUHandle();
		for (auto& model : scene)
		{
			commandList->SetGraphicsRootDescriptorTable(1, modelDataGPUdescriptorHandle);
			modelDataGPUdescriptorHandle.Offset(gpuDescriptorHeap->GetDescriptorSize());
			model->Render(commandList);
		}

		// Reset back
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &rect);
	}
}

void SceneRendererDirectLightingVoxelGIandAO::InitVoxelConeTracing(DX12DeviceResourcesSingleton* _deviceResources, 
	ID3D12Device* device, 
	DX12DescriptorHeapManager* descriptorManager)
{
	// Voxelization
	{
		viewport_voxel_cone_tracing_voxelization = CD3DX12_VIEWPORT(0.0f, 0.0f, shader_structure_cpu_voxelization_data.voxel_grid_res, shader_structure_cpu_voxelization_data.voxel_grid_res);
		scissor_rect_voxel_cone_tracing_voxelization = CD3DX12_RECT(0.0f, 0.0f, shader_structure_cpu_voxelization_data.voxel_grid_res, shader_structure_cpu_voxelization_data.voxel_grid_res);
		D3D12_SAMPLER_DESC shadowSampler;
		shadowSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		shadowSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		shadowSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		shadowSampler.MipLODBias = 0;
		shadowSampler.MaxAnisotropy = 16;
		shadowSampler.MinLOD = 0.0f;
		shadowSampler.MaxLOD = D3D12_FLOAT32_MAX;
		shadowSampler.BorderColor[0] = 1.0f;
		shadowSampler.BorderColor[1] = 1.0f;
		shadowSampler.BorderColor[2] = 1.0f;
		shadowSampler.BorderColor[3] = 1.0f;

		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		p_render_target_vct_voxelization = new DX12RenderTarget(device, 
			descriptorManager, 
			shader_structure_cpu_voxelization_data.voxel_grid_res,
			shader_structure_cpu_voxelization_data.voxel_grid_res,
			format, 
			flags, 
			L"Voxelization Scene Data 3D",
			shader_structure_cpu_voxelization_data.voxel_grid_res,
			1,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		mVCTVoxelizationRS.Reset(5, 1);
		mVCTVoxelizationRS.InitStaticSampler(0, shadowSampler, D3D12_SHADER_VISIBILITY_PIXEL);
		mVCTVoxelizationRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTVoxelizationRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTVoxelizationRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTVoxelizationRS[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTVoxelizationRS[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTVoxelizationRS.Finalize(device, L"VCT voxelization pass RS", rootSignatureFlags);

		char* pVertexShaderByteCode;
		int vertexShaderByteCodeLength;
		char* pGeometryShadeByteCode;
		int geometryShaderByteCodeLength;
		char* pPixelShaderByteCode;
		int pixelShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("VoxelConeTracingVoxelization_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);
		LoadShaderByteCode(GetFilePath("VoxelConeTracingVoxelization_GS.cso"), pGeometryShadeByteCode, geometryShaderByteCodeLength);
		LoadShaderByteCode(GetFilePath("VoxelConeTracingVoxelization_PS.cso"), pPixelShaderByteCode, pixelShaderByteCodeLength);

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		mVCTVoxelizationPSO.SetRootSignature(mVCTVoxelizationRS);
		mVCTVoxelizationPSO.SetRasterizerState(mRasterizerStateNoCullNoDepth);
		mVCTVoxelizationPSO.SetRenderTargetFormats(0, nullptr, DXGI_FORMAT_D32_FLOAT);
		mVCTVoxelizationPSO.SetBlendState(mBlendState);
		mVCTVoxelizationPSO.SetDepthStencilState(mDepthStateDisabled);
		mVCTVoxelizationPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		mVCTVoxelizationPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		mVCTVoxelizationPSO.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
		mVCTVoxelizationPSO.SetGeometryShader(pGeometryShadeByteCode, geometryShaderByteCodeLength);
		mVCTVoxelizationPSO.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
		mVCTVoxelizationPSO.Finalize(device);

		delete[] pVertexShaderByteCode;
		delete[] pGeometryShadeByteCode;
		delete[] pPixelShaderByteCode;

		//create constant buffer for pass
		DX12Buffer::Description cbDesc;
		cbDesc.mElementSize = c_aligned_shader_structure_cpu_voxelization_data;
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;
		p_constant_buffer_voxelization = new DX12Buffer(descriptorManager,
			cbDesc,
			_deviceResources->GetBackBufferCount(),
			L"VCT Voxelization Pass CB");
	}

	// Debug 
	{
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		mVCTVoxelizationDebugRT = new DX12RenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, format, flags, L"Voxelization Debug RT");

		// Create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		mVCTVoxelizationDebugRS.Reset(2, 0);
		mVCTVoxelizationDebugRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTVoxelizationDebugRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTVoxelizationDebugRS.Finalize(device, L"VCT voxelization debug pass RS", rootSignatureFlags);

		char* pVertexShaderByteCode;
		int vertexShaderByteCodeLength;
		char* pGeometryShadeByteCode;
		int geometryShaderByteCodeLength;
		char* pPixelShaderByteCode;
		int pixelShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("VoxelConeTracingVoxelizationDebug_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);
		LoadShaderByteCode(GetFilePath("VoxelConeTracingVoxelizationDebug_GS.cso"), pGeometryShadeByteCode, geometryShaderByteCodeLength);
		LoadShaderByteCode(GetFilePath("VoxelConeTracingVoxelizationDebug_PS.cso"), pPixelShaderByteCode, pixelShaderByteCodeLength);

		DXGI_FORMAT formats[1];
		formats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		mVCTVoxelizationDebugPSO.SetRootSignature(mVCTVoxelizationDebugRS);
		mVCTVoxelizationDebugPSO.SetRasterizerState(mRasterizerState);
		mVCTVoxelizationDebugPSO.SetRenderTargetFormats(_countof(formats), formats, DXGI_FORMAT_D32_FLOAT);
		mVCTVoxelizationDebugPSO.SetBlendState(mBlendState);
		mVCTVoxelizationDebugPSO.SetDepthStencilState(mDepthStateRW);
		mVCTVoxelizationDebugPSO.SetInputLayout(0, nullptr);
		mVCTVoxelizationDebugPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
		mVCTVoxelizationDebugPSO.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
		mVCTVoxelizationDebugPSO.SetGeometryShader(pGeometryShadeByteCode, geometryShaderByteCodeLength);
		mVCTVoxelizationDebugPSO.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
		mVCTVoxelizationDebugPSO.Finalize(device);

		delete[] pVertexShaderByteCode;
		delete[] pGeometryShadeByteCode;
		delete[] pPixelShaderByteCode;
	}

	// Anisotropic mip generation
	{
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		int size = shader_structure_cpu_voxelization_data.voxel_grid_res >> 1;
		p_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare X+ 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		p_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare X- 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		p_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare Y+ 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		p_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare Y- 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		p_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare Z+ 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		p_render_targets_vct_voxelization_anisotropic_mip_generation.push_back(new DX12RenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare Z- 3D", size, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

		root_signature_vct_voxelization_anisotropic_mip_generation_level_zero.Reset(3, 0);
		root_signature_vct_voxelization_anisotropic_mip_generation_level_zero[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation_level_zero[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation_level_zero[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 6, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation_level_zero.Finalize(device, L"VCT aniso mipmapping level zero RS", rootSignatureFlags);

		char* pComputeShaderByteCode;
		int computeShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("VoxelConeTracingAnisotropicMipmapChainGenerationLevelZero_CS.cso"), pComputeShaderByteCode, computeShaderByteCodeLength);

		pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero.SetRootSignature(root_signature_vct_voxelization_anisotropic_mip_generation_level_zero);
		pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero.SetComputeShader(pComputeShaderByteCode, computeShaderByteCodeLength);
		pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero.Finalize(device);

		delete[] pComputeShaderByteCode;

		root_signature_vct_voxelization_anisotropic_mip_generation.Reset(3, 0);
		root_signature_vct_voxelization_anisotropic_mip_generation[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 6, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 6, D3D12_SHADER_VISIBILITY_ALL);
		root_signature_vct_voxelization_anisotropic_mip_generation.Finalize(device, L"VCT aniso mipmapping RS", rootSignatureFlags);

		LoadShaderByteCode(GetFilePath("VoxelConeTracingAnisotropicMipmapChainGeneration_CS.cso"), pComputeShaderByteCode, computeShaderByteCodeLength);

		pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero.SetRootSignature(root_signature_vct_voxelization_anisotropic_mip_generation);
		pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero.SetComputeShader(pComputeShaderByteCode, computeShaderByteCodeLength);
		pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero.Finalize(device);

		delete[] pComputeShaderByteCode;

		DX12Buffer::Description cbDesc;
		cbDesc.mElementSize = c_aligned_shader_structure_cpu_vct_anisotropic_mip_generation_data;
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;
		// The zero'th CB is for the VoxelConeTracingAnisotropicMipmapChainGenerationLevelZero_CS
		// the rest are for the VoxelConeTracingAnisotropicMipmapChainGeneration_CS
		for (UINT i = 0; i < shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count + 1; i++)
		{
			P_constant_buffers_vct_anisotropic_mip_generation.push_back(new DX12Buffer(descriptorManager,
					cbDesc,
				    _deviceResources->GetBackBufferCount(),
					L"VCT aniso mip mapping CB"));
		}
	}

	// Voxel cone tracing main 
	{
		DX12Buffer::Description cbDesc;
		cbDesc.mElementSize = c_aligned_shader_structure_cpu_vct_main_data;
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

		p_constant_buffer_vct_main = new DX12Buffer(descriptorManager,
			cbDesc, 
			_deviceResources->GetBackBufferCount(),
			L"VCT main CB");
		
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		mVCTMainRT = new DX12RenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH * mVCTRTRatio, MAX_SCREEN_HEIGHT * mVCTRTRatio, format, flags, L"VCT Final Output", -1, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// Create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		// CS
		{
			mVCTMainRS_Compute.Reset(4, 1);
			mVCTMainRS_Compute.InitStaticSampler(0, mBilinearSampler, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS_Compute[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS_Compute[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 1, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS_Compute[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS_Compute[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS_Compute.Finalize(device, L"VCT main pass compute version RS", rootSignatureFlags);

			char* pComputeShaderByteCode;
			int computeShaderByteCodeLength;
			LoadShaderByteCode(GetFilePath("VoxelConeTracing_CS.cso"), pComputeShaderByteCode, computeShaderByteCodeLength);


			mVCTMainPSO_Compute.SetRootSignature(mVCTMainRS_Compute);
			mVCTMainPSO_Compute.SetComputeShader(pComputeShaderByteCode, computeShaderByteCodeLength);
			mVCTMainPSO_Compute.Finalize(device);

			delete[] pComputeShaderByteCode;
		}
	}

	// Upsample and blur
	{
		//RTs
		mVCTMainUpsampleAndBlurRT = new DX12RenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT,
			DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"VCT Main RT Upsampled & Blurred", -1, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		mVCTMainUpsampleAndBlurRS.Reset(2, 1);
		mVCTMainUpsampleAndBlurRS.InitStaticSampler(0, mBilinearSampler, D3D12_SHADER_VISIBILITY_ALL);
		mVCTMainUpsampleAndBlurRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTMainUpsampleAndBlurRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTMainUpsampleAndBlurRS.Finalize(device, L"VCT Main RT Upsample & Blur pass RS", rootSignatureFlags);

		char* pComputeShaderByteCode;
		int computeShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("UpsampleBlur_CS.cso"), pComputeShaderByteCode, computeShaderByteCodeLength);

		mVCTMainUpsampleAndBlurPSO.SetRootSignature(mVCTMainUpsampleAndBlurRS);
		mVCTMainUpsampleAndBlurPSO.SetComputeShader(pComputeShaderByteCode, computeShaderByteCodeLength);
		mVCTMainUpsampleAndBlurPSO.Finalize(device);

		delete[] pComputeShaderByteCode;
	}
}
void SceneRendererDirectLightingVoxelGIandAO::RenderVoxelConeTracing(DX12DeviceResourcesSingleton* _deviceResources,
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList,
	DX12DescriptorHeapGPU* gpuDescriptorHeap,
	std::vector<Model*>& scene,
	DX12DescriptorHandleBlock& modelDataDescriptorHandleBlock,
	DX12DescriptorHandleBlock& directionalLightDescriptorHandleBlock,
	DX12DescriptorHandleBlock& cameraDataDescriptorHandleBlock,
	DX12DescriptorHandleBlock& shadowDepthDescriptorHandleBlock,
	RenderQueue aQueue)
{
	if (!shader_structure_cpu_illumination_flags_data.use_vct)
	{
		return;
	}

	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);

	if (aQueue == GRAPHICS_QUEUE) 
	{
		// Voxelizer
		{
			commandList->RSSetViewports(1, &viewport_voxel_cone_tracing_voxelization);
			commandList->RSSetScissorRects(1, &scissor_rect_voxel_cone_tracing_voxelization);
			commandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

			commandList->SetPipelineState(mVCTVoxelizationPSO.GetPipelineStateObject());
			commandList->SetGraphicsRootSignature(mVCTVoxelizationRS.GetSignature());

			_deviceResources->ResourceBarriersBegin(barriers);
			p_render_target_vct_voxelization->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mShadowDepth->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			_deviceResources->ResourceBarriersEnd(barriers, commandList);
			
			DX12DescriptorHandleBlock vctVoxelizationRenderTargetDescriptorHandleBlock;
			vctVoxelizationRenderTargetDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
			vctVoxelizationRenderTargetDescriptorHandleBlock.Add(p_render_target_vct_voxelization->GetUAV());
			
			commandList->ClearUnorderedAccessViewFloat(vctVoxelizationRenderTargetDescriptorHandleBlock.GetGPUHandle(), 
				p_render_target_vct_voxelization->GetUAV(),
				p_render_target_vct_voxelization->GetResource(),
				gs_clear_color_black, 
				0, 
				nullptr);

			DX12DescriptorHandleBlock vctVoxelizationDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
			vctVoxelizationDescriptorHandleBlock.Add(p_constant_buffer_voxelization->GetCBV(shader_structure_cpu_voxelization_data_most_updated_index));

			commandList->SetGraphicsRootDescriptorTable(0, vctVoxelizationDescriptorHandleBlock.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(1, directionalLightDescriptorHandleBlock.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(3, shadowDepthDescriptorHandleBlock.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(4, vctVoxelizationRenderTargetDescriptorHandleBlock.GetGPUHandle());
			CD3DX12_GPU_DESCRIPTOR_HANDLE modelDataGPUDescriptorHandle = modelDataDescriptorHandleBlock.GetGPUHandle();
			for (auto& model : scene) 
			{
				commandList->SetGraphicsRootDescriptorTable(2, modelDataGPUDescriptorHandle);
				modelDataGPUDescriptorHandle.Offset(gpuDescriptorHeap->GetDescriptorSize());
				model->Render(commandList);
			}

			// Reset back
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &rect);
		}

		if (mVCTRenderDebug) 
		{
			////PIXBeginEvent(commandList, 0, "VCT Voxelization Debug");
			//commandList->SetPipelineState(mVCTVoxelizationDebugPSO.GetPipelineStateObject());
			//commandList->SetGraphicsRootSignature(mVCTVoxelizationDebugRS.GetSignature());

			//D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesFinal[] =
			//{
			//	mVCTVoxelizationDebugRT->GetRTV().GetCPUHandle()
			//};

			//commandList->OMSetRenderTargets(_countof(rtvHandlesFinal), rtvHandlesFinal, FALSE, &_deviceResources->GetDepthStencilView());
			//commandList->ClearRenderTargetView(rtvHandlesFinal[0], clearColorBlack, 0, nullptr);
			//commandList->ClearDepthStencilView(_deviceResources->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			//_deviceResources->ResourceBarriersBegin(barriers);
			//mVCTVoxelization3DRT->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			//_deviceResources->ResourceBarriersEnd(barriers, commandList);

			//DX12DescriptorHandleBlock cbvHandle;
			//DX12DescriptorHandleBlock uavHandle;

			//cbvHandle = gpuDescriptorHeap->GetHandleBlock(1);
			//gpuDescriptorHeap->AddToHandle(device, cbvHandle, p_constant_buffer_voxelization->GetCBV());

			//uavHandle = gpuDescriptorHeap->GetHandleBlock(1);
			//gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTVoxelization3DRT->GetUAV());

			//commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			//commandList->SetGraphicsRootDescriptorTable(1, uavHandle.GetGPUHandle());

			//commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
			//commandList->DrawInstanced(VCT_SCENE_VOLUME_SIZE * VCT_SCENE_VOLUME_SIZE * VCT_SCENE_VOLUME_SIZE, 1, 0, 0);
			////PIXEndEvent(commandList);
		}
	
	}

	if (aQueue == COMPUTE_QUEUE) 
	{
		// Anisotropic mip chain generation
		{
			commandList->SetPipelineState(pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero.GetPipelineStateObject());
			commandList->SetComputeRootSignature(root_signature_vct_voxelization_anisotropic_mip_generation_level_zero.GetSignature());

			_deviceResources->ResourceBarriersBegin(barriers);
			p_render_target_vct_voxelization->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[0]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[1]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[2]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[3]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[4]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[5]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			_deviceResources->ResourceBarriersEnd(barriers, commandList);

			DX12DescriptorHandleBlock srvHandle = gpuDescriptorHeap->GetHandleBlock(1);
			srvHandle.Add(p_render_target_vct_voxelization->GetSRV());

			DX12DescriptorHandleBlock uavHandle = gpuDescriptorHeap->GetHandleBlock(6);
			uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[0]->GetUAV());
			uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[1]->GetUAV());
			uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[2]->GetUAV());
			uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[3]->GetUAV());
			uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[4]->GetUAV());
			uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[5]->GetUAV());

			shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index++;
			if (shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index == _deviceResources->GetBackBufferCount())
			{
				shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index = 0;
			}

			shader_structure_cpu_vct_anisotropic_mip_generation_data.mip_dimension = shader_structure_cpu_voxelization_data.voxel_grid_res >> 1;
			shader_structure_cpu_vct_anisotropic_mip_generation_data.source_mip_level = 0;
			shader_structure_cpu_vct_anisotropic_mip_generation_data.result_mip_level = 0;
			memcpy(P_constant_buffers_vct_anisotropic_mip_generation[0]->GetMappedData(shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index), &shader_structure_cpu_vct_anisotropic_mip_generation_data, sizeof(ShaderStructureCPUAnisotropicMipGenerationData));

			DX12DescriptorHandleBlock mipMappingDataDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
			mipMappingDataDescriptorHandleBlock.Add(P_constant_buffers_vct_anisotropic_mip_generation[0]->GetCBV(shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index));

			commandList->SetComputeRootDescriptorTable(0, mipMappingDataDescriptorHandleBlock.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(1, srvHandle.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(2, uavHandle.GetGPUHandle());

			UINT dispatchBlockSize = ((shader_structure_cpu_vct_anisotropic_mip_generation_data.mip_dimension + (DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION - 1u)) & ~(DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION - 1u)) / DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION;
			commandList->Dispatch(dispatchBlockSize, dispatchBlockSize, dispatchBlockSize);

			// Dispatch the rest of the mips
			commandList->SetPipelineState(pipeline_state_vct_voxelization_anisotropic_mip_generation.GetPipelineStateObject());
			commandList->SetComputeRootSignature(root_signature_vct_voxelization_anisotropic_mip_generation.GetSignature());
			for (UINT i = 1; i < shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count; i++)
			{
				// Move onto the next mip
				_deviceResources->ResourceBarriersBegin(barriers);
				p_render_targets_vct_voxelization_anisotropic_mip_generation[0]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				p_render_targets_vct_voxelization_anisotropic_mip_generation[1]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				p_render_targets_vct_voxelization_anisotropic_mip_generation[2]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				p_render_targets_vct_voxelization_anisotropic_mip_generation[3]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				p_render_targets_vct_voxelization_anisotropic_mip_generation[4]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				p_render_targets_vct_voxelization_anisotropic_mip_generation[5]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i - 1);
				_deviceResources->ResourceBarriersEnd(barriers, commandList);

				DX12DescriptorHandleBlock srvHandle = gpuDescriptorHeap->GetHandleBlock(6);
				srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[0]->GetSRV(i - 1));
				srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[1]->GetSRV(i - 1));
				srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[2]->GetSRV(i - 1));
				srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[3]->GetSRV(i - 1));
				srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[4]->GetSRV(i - 1));
				srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[5]->GetSRV(i - 1));

				DX12DescriptorHandleBlock uavHandle = gpuDescriptorHeap->GetHandleBlock(6);
				uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[0]->GetUAV(i));
				uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[1]->GetUAV(i));
				uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[2]->GetUAV(i));
				uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[3]->GetUAV(i));
				uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[4]->GetUAV(i));
				uavHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[5]->GetUAV(i));

				shader_structure_cpu_vct_anisotropic_mip_generation_data.mip_dimension = shader_structure_cpu_vct_anisotropic_mip_generation_data.mip_dimension >> 1;
				shader_structure_cpu_vct_anisotropic_mip_generation_data.source_mip_level = i - 1;
				shader_structure_cpu_vct_anisotropic_mip_generation_data.result_mip_level = i;
				memcpy(P_constant_buffers_vct_anisotropic_mip_generation[i]->GetMappedData(shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index), &shader_structure_cpu_vct_anisotropic_mip_generation_data, sizeof(ShaderStructureCPUAnisotropicMipGenerationData));

				DX12DescriptorHandleBlock mipMappingDataDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
				mipMappingDataDescriptorHandleBlock.Add(P_constant_buffers_vct_anisotropic_mip_generation[i]->GetCBV(shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index));

				commandList->SetComputeRootDescriptorTable(0, mipMappingDataDescriptorHandleBlock.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(1, srvHandle.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(2, uavHandle.GetGPUHandle());

				dispatchBlockSize = ((shader_structure_cpu_vct_anisotropic_mip_generation_data.mip_dimension + (DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION - 1u)) & ~(DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION - 1u)) / DISPATCH_BLOCK_SIZE_VCT_ANISOTROPIC_MIP_GENERATION;
				commandList->Dispatch(dispatchBlockSize, dispatchBlockSize, dispatchBlockSize);
			}

			// Transition the last mip level into a read only state as well
			_deviceResources->ResourceBarriersBegin(barriers);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[0]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[1]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[2]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[3]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[4]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			p_render_targets_vct_voxelization_anisotropic_mip_generation[5]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, shader_structure_cpu_voxelization_data.voxel_grid_anisotropic_mip_count - 1);
			_deviceResources->ResourceBarriersEnd(barriers, commandList);
		}
		
		// Voxel cone tracing main
		{
			commandList->SetPipelineState(mVCTMainPSO_Compute.GetPipelineStateObject());
			commandList->SetComputeRootSignature(mVCTMainRS_Compute.GetSignature());

			DX12DescriptorHandleBlock uavHandle = gpuDescriptorHeap->GetHandleBlock(1);
			uavHandle.Add(mVCTMainRT->GetUAV());

			DX12DescriptorHandleBlock srvHandle = gpuDescriptorHeap->GetHandleBlock(10);
			srvHandle.Add(P_render_targets_gbuffer[0]->GetSRV());
			srvHandle.Add(P_render_targets_gbuffer[1]->GetSRV());
			srvHandle.Add(P_render_targets_gbuffer[2]->GetSRV());

			srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[0]->GetSRV());
			srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[1]->GetSRV());
			srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[2]->GetSRV());
			srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[3]->GetSRV());
			srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[4]->GetSRV());
			srvHandle.Add(p_render_targets_vct_voxelization_anisotropic_mip_generation[5]->GetSRV());
			srvHandle.Add(p_render_target_vct_voxelization->GetSRV());

			DX12DescriptorHandleBlock cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			cbvHandle.Add(p_constant_buffer_voxelization->GetCBV(shader_structure_cpu_voxelization_data_most_updated_index));
			cbvHandle.Add(p_constant_buffer_vct_main->GetCBV(shader_structure_cpu_vct_main_data_most_updated_index));

			commandList->SetComputeRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(1, cameraDataDescriptorHandleBlock.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(2, srvHandle.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(3, uavHandle.GetGPUHandle());

			commandList->Dispatch(DivideByMultiple(static_cast<UINT>(mVCTMainRT->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(mVCTMainRT->GetHeight()), 8u), 1u);
		}
	
		// Upsample and blur
		if (mVCTMainRTUseUpsampleAndBlur) 
		{
			{
				commandList->SetPipelineState(mVCTMainUpsampleAndBlurPSO.GetPipelineStateObject());
				commandList->SetComputeRootSignature(mVCTMainUpsampleAndBlurRS.GetSignature());

				_deviceResources->ResourceBarriersBegin(barriers);
				mVCTMainRT->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				mVCTMainUpsampleAndBlurRT->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				_deviceResources->ResourceBarriersEnd(barriers, commandList);

				DX12DescriptorHandleBlock srvHandleBlur = gpuDescriptorHeap->GetHandleBlock(1);
				srvHandleBlur.Add(mVCTMainRT->GetSRV());

				DX12DescriptorHandleBlock uavHandleBlur = gpuDescriptorHeap->GetHandleBlock(1);
				uavHandleBlur.Add(mVCTMainUpsampleAndBlurRT->GetUAV());

				commandList->SetComputeRootDescriptorTable(0, srvHandleBlur.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(1, uavHandleBlur.GetGPUHandle());

				commandList->Dispatch(DivideByMultiple(static_cast<UINT>(mVCTMainUpsampleAndBlurRT->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(mVCTMainUpsampleAndBlurRT->GetHeight()), 8u), 1u);
			}
		}
	}
}

void SceneRendererDirectLightingVoxelGIandAO::InitLighting(DX12DeviceResourcesSingleton* _deviceResources, ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager)
{
	//RTs
	mLightingRT = new DX12RenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT,
		DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Lighting");

	//create root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	D3D12_SAMPLER_DESC shadowSampler;
	shadowSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	shadowSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	shadowSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	shadowSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	shadowSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	shadowSampler.MipLODBias = 0;
	shadowSampler.MaxAnisotropy = 16;
	shadowSampler.MinLOD = 0.0f;
	shadowSampler.MaxLOD = D3D12_FLOAT32_MAX;
	shadowSampler.BorderColor[0] = 1.0f;
	shadowSampler.BorderColor[1] = 1.0f;
	shadowSampler.BorderColor[2] = 1.0f;
	shadowSampler.BorderColor[3] = 1.0f;

	mLightingRS.Reset(5, 2);
	mLightingRS.InitStaticSampler(0, mBilinearSampler, D3D12_SHADER_VISIBILITY_PIXEL);
	mLightingRS.InitStaticSampler(1, shadowSampler, D3D12_SHADER_VISIBILITY_PIXEL);
	mLightingRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
	mLightingRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 1, D3D12_SHADER_VISIBILITY_ALL);
	mLightingRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 3, 1, D3D12_SHADER_VISIBILITY_ALL);
	mLightingRS[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4, D3D12_SHADER_VISIBILITY_PIXEL);
	mLightingRS[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	mLightingRS.Finalize(device, L"Lighting pass RS", rootSignatureFlags);

	//PSO
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	char* pVertexShaderByteCode;
	int vertexShaderByteCodeLength;
	char* pPixelShaderByteCode;
	int pixelShaderByteCodeLength;
	LoadShaderByteCode(GetFilePath("Lighting_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode(GetFilePath("Lighting_PS.cso"), pPixelShaderByteCode, pixelShaderByteCodeLength);

	DXGI_FORMAT m_rtFormats[1];
	m_rtFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	mLightingPSO.SetRootSignature(mLightingRS);
	mLightingPSO.SetRasterizerState(mRasterizerState);
	mLightingPSO.SetBlendState(mBlendState);
	mLightingPSO.SetDepthStencilState(mDepthStateDisabled);
	mLightingPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	mLightingPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	mLightingPSO.SetRenderTargetFormats(_countof(m_rtFormats), m_rtFormats, DXGI_FORMAT_D32_FLOAT);
	mLightingPSO.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
	mLightingPSO.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
	mLightingPSO.Finalize(device);

	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;

	//CB
	DX12Buffer::Description cbDesc;
	cbDesc.mElementSize = c_aligned_shader_structure_cpu_lighting_data;
	cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
	cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

	mLightingCB = new DX12Buffer(descriptorManager,
		cbDesc,
		_deviceResources->GetBackBufferCount(),
		L"Lighting Pass CB");

	cbDesc.mElementSize = c_aligned_shader_structure_cpu_illumination_flags_data;
	mIlluminationFlagsCB = new DX12Buffer(descriptorManager,
		cbDesc, 
		_deviceResources->GetBackBufferCount(),
		L"Illumination Flags CB");
}
void SceneRendererDirectLightingVoxelGIandAO::RenderLighting(DX12DeviceResourcesSingleton* _deviceResources, 
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* commandList, 
	DX12DescriptorHeapGPU* gpuDescriptorHeap,
	DX12DescriptorHandleBlock& directionalLightDescriptorHandleBlock,
	DX12DescriptorHandleBlock& cameraDataDescriptorHandleBlock,
	DX12DescriptorHandleBlock& shadowDepthDescriptorHandleBlock,
	D3D12_VERTEX_BUFFER_VIEW& fullScreenQuadVertexBufferView)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &rect);

	commandList->SetPipelineState(mLightingPSO.GetPipelineStateObject());
	commandList->SetGraphicsRootSignature(mLightingRS.GetSignature());

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesLighting[] =
	{
		mLightingRT->GetRTV()
	};

	_deviceResources->ResourceBarriersBegin(barriers);
	mLightingRT->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mVCTMainUpsampleAndBlurRT->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mVCTMainRT->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	P_render_targets_gbuffer[0]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	P_render_targets_gbuffer[1]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	P_render_targets_gbuffer[2]->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mShadowDepth->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_deviceResources->ResourceBarriersEnd(barriers, commandList);

	commandList->OMSetRenderTargets(_countof(rtvHandlesLighting), rtvHandlesLighting, FALSE, nullptr);
	commandList->ClearRenderTargetView(rtvHandlesLighting[0], gs_clear_color_black, 0, nullptr);

	DX12DescriptorHandleBlock cbvHandleLighting = gpuDescriptorHeap->GetHandleBlock(2);
	cbvHandleLighting.Add(mLightingCB->GetCBV(shader_structure_cpu_lighting_data_most_updated_index));
	cbvHandleLighting.Add(mIlluminationFlagsCB->GetCBV(shader_structure_cpu_illumination_flags_most_updated_index));

	DX12DescriptorHandleBlock srvHandleLighting = gpuDescriptorHeap->GetHandleBlock(4);
	srvHandleLighting.Add(P_render_targets_gbuffer[0]->GetSRV());
	srvHandleLighting.Add(P_render_targets_gbuffer[1]->GetSRV());
	srvHandleLighting.Add(P_render_targets_gbuffer[2]->GetSRV());

	if (mVCTRenderDebug)
	{
		srvHandleLighting.Add(mVCTVoxelizationDebugRT->GetSRV());
	}	
	else if (mVCTMainRTUseUpsampleAndBlur)
	{
		srvHandleLighting.Add(mVCTMainUpsampleAndBlurRT->GetSRV());
	}
	else
	{
		srvHandleLighting.Add(mVCTMainRT->GetSRV());
	}
			
	commandList->SetGraphicsRootDescriptorTable(0, cbvHandleLighting.GetGPUHandle());
	commandList->SetGraphicsRootDescriptorTable(1, directionalLightDescriptorHandleBlock.GetGPUHandle());
	commandList->SetGraphicsRootDescriptorTable(2, cameraDataDescriptorHandleBlock.GetGPUHandle());
	commandList->SetGraphicsRootDescriptorTable(3, srvHandleLighting.GetGPUHandle());
	commandList->SetGraphicsRootDescriptorTable(4, shadowDepthDescriptorHandleBlock.GetGPUHandle());

	commandList->IASetVertexBuffers(0, 1, &fullScreenQuadVertexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->DrawInstanced(4, 1, 0, 0);
}

void SceneRendererDirectLightingVoxelGIandAO::InitComposite(DX12DeviceResourcesSingleton* _deviceResources, ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager)
{
	//create root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	mCompositeRS.Reset(1, 0);
	mCompositeRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	mCompositeRS.Finalize(device, L"Composite RS", rootSignatureFlags);


	char* pVertexShaderByteCode;
	int vertexShaderByteCodeLength;
	char* pPixelShaderByteCode;
	int pixelShaderByteCodeLength;
	LoadShaderByteCode(GetFilePath("Composite_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode(GetFilePath("Composite_PS.cso"), pPixelShaderByteCode, pixelShaderByteCodeLength);



	mCompositePSO = mLightingPSO;
	mCompositePSO.SetRootSignature(mCompositeRS);
	mCompositePSO.SetRenderTargetFormat(_deviceResources->GetBackBufferFormat(), DXGI_FORMAT_D32_FLOAT);
	mCompositePSO.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
	mCompositePSO.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
	mCompositePSO.Finalize(device);

	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
}
void SceneRendererDirectLightingVoxelGIandAO::RenderComposite(DX12DeviceResourcesSingleton* _deviceResources, 
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* commandList, 
	DX12DescriptorHeapGPU* gpuDescriptorHeap,
	D3D12_VERTEX_BUFFER_VIEW& fullScreenQuadVertexBufferView)
{
	
	commandList->SetPipelineState(mCompositePSO.GetPipelineStateObject());
	commandList->SetGraphicsRootSignature(mCompositeRS.GetSignature());

	_deviceResources->ResourceBarriersBegin(barriers);
	mLightingRT->TransitionTo(barriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_deviceResources->ResourceBarriersEnd(barriers, commandList);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesFinal[] =
	{
			_deviceResources->GetRenderTargetView()
	};
	commandList->OMSetRenderTargets(_countof(rtvHandlesFinal), rtvHandlesFinal, FALSE, nullptr);
	commandList->ClearRenderTargetView(_deviceResources->GetRenderTargetView(), Colors::CornflowerBlue, 0, nullptr);
		
	DX12DescriptorHandleBlock srvHandleComposite = gpuDescriptorHeap->GetHandleBlock(1);
	srvHandleComposite.Add(mLightingRT->GetSRV());
		
	commandList->SetGraphicsRootDescriptorTable(0, srvHandleComposite.GetGPUHandle());
	commandList->IASetVertexBuffers(0, 1, &fullScreenQuadVertexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->DrawInstanced(4, 1, 0, 0);
	
}