#include "Graphics\SceneRenderers\SceneRendererDirectLightingVoxelGIandAO.h"


static const float gs_clear_color_black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
static const float gs_clear_color_white[] = { 1.0f, 1.0f, 1.0f, 1.0f };

SceneRendererDirectLightingVoxelGIandAO::SceneRendererDirectLightingVoxelGIandAO()
{
}

SceneRendererDirectLightingVoxelGIandAO::~SceneRendererDirectLightingVoxelGIandAO()
{
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

	mDepthStencil = new DXRSDepthBuffer(_d3dDevice, &descriptor_heap_manager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, DXGI_FORMAT_D32_FLOAT);

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

	mDepthStateRead = mDepthStateRW;
	mDepthStateRead.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

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

	mBlendStateLPVPropagation.IndependentBlendEnable = TRUE;
	for (size_t i = 0; i < 6; i++)
	{
		mBlendStateLPVPropagation.RenderTarget[i].BlendEnable = (i < 3) ? FALSE : TRUE;
		mBlendStateLPVPropagation.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
		mBlendStateLPVPropagation.RenderTarget[i].DestBlend = D3D12_BLEND_ONE;
		mBlendStateLPVPropagation.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		mBlendStateLPVPropagation.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		mBlendStateLPVPropagation.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ONE;
		mBlendStateLPVPropagation.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		mBlendStateLPVPropagation.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

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

	InitGbuffer(_d3dDevice, &descriptor_heap_manager);
	InitShadowMapping(_d3dDevice, &descriptor_heap_manager);
	InitVoxelConeTracing(_deviceResources, _d3dDevice, &descriptor_heap_manager);
	InitLighting(_deviceResources, _d3dDevice, &descriptor_heap_manager);
	InitComposite(_deviceResources, _d3dDevice, &descriptor_heap_manager);
}


// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
// This function expects to be given an already reset direct command list and a non reset compute command list.
// When returning the functions leaves these command lists in the same state they we're in
// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
void SceneRendererDirectLightingVoxelGIandAO::Render(std::vector<Model*>& scene,
	DirectionalLight& directionalLight,
	DXRSTimer& mTimer,
	Camera& camera,
	ID3D12GraphicsCommandList* _commandListDirect,
	ID3D12GraphicsCommandList* _commandListCompute,
	ID3D12CommandAllocator* _commandAllocator)
{	
	// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!
	// This function expects to be given an already reset direct command list and a non reset compute command list.
	// When returning the functions leaves these command lists in the same state they we're in
	// !!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!

	DX12DeviceResourcesSingleton* _deviceResources = DX12DeviceResourcesSingleton::GetDX12DeviceResources();
	auto _commandListDirect = _deviceResources->GetCommandListDirect();
	auto _commandListCompute = _deviceResources->GetCommandListCompute();

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

	// Update ShaderStructureCPULightPassData
	shader_structure_cpu_lighting_data.shadow_texel_size = XMFLOAT2(1.0f / mShadowDepth->GetWidth(), 1.0f / mShadowDepth->GetHeight());
	memcpy(mLightingCB->GetMappedData(), &shader_structure_cpu_lighting_data, sizeof(ShaderSTructureCPULightingData));

	shader_structure_cpu_illumination_flags_data.use_direct = mUseDirectLight ? 1 : 0;
	shader_structure_cpu_illumination_flags_data.use_shadows = mUseShadows ? 1 : 0;
	shader_structure_cpu_illumination_flags_data.use_vct = mUseVCT ? 1 : 0;
	shader_structure_cpu_illumination_flags_data.use_vct_debug = mVCTRenderDebug ? 1 : 0;
	shader_structure_cpu_illumination_flags_data.vct_gi_power = mVCTGIPower;
	shader_structure_cpu_illumination_flags_data.show_only_ao = mShowOnlyAO ? 1 : 0;
	memcpy(mIlluminationFlagsCB->GetMappedData(), &shader_structure_cpu_illumination_flags_data, sizeof(ShaderStructureCPUIlluminationFlagsData));

	// Update ShaderStructureCPUVoxelizationData
	float scale = 1.0f;// VCT_SCENE_VOLUME_SIZE / mWorldVoxelScale;
	DirectX::XMStoreFloat4x4(&shader_structure_cpu_voxelization_data.voxel_grid_space, XMMatrixTranspose(XMMatrixMultiply(XMMatrixScaling(scale, -scale, scale), XMMatrixTranslation(-VCT_SCENE_VOLUME_SIZE * 0.25f, -VCT_SCENE_VOLUME_SIZE * 0.25f, -VCT_SCENE_VOLUME_SIZE * 0.25f))));
	shader_structure_cpu_voxelization_data.voxel_scale = mWorldVoxelScale;
	memcpy(mVCTVoxelizationCB->GetMappedData(), &shader_structure_cpu_voxelization_data, sizeof(ShaderStructureCPUVoxelizationData));

	// Update ShaderStructureCPUVCTMainData
	shader_structure_cpu_vct_main_data.upsample_ratio = DirectX::XMFLOAT2(mGbufferRTs[0]->GetWidth() / mVCTMainRT->GetWidth(), mGbufferRTs[0]->GetHeight() / mVCTMainRT->GetHeight());
	shader_structure_cpu_vct_main_data.indirect_diffuse_strength = mVCTIndirectDiffuseStrength;
	shader_structure_cpu_vct_main_data.indirect_specular_strength = mVCTIndirectSpecularStrength;
	shader_structure_cpu_vct_main_data.max_cone_trace_distance = mVCTMaxConeTraceDistance;
	shader_structure_cpu_vct_main_data.ao_falloff = mVCTAoFalloff;
	shader_structure_cpu_vct_main_data.sampling_factor = mVCTSamplingFactor;
	shader_structure_cpu_vct_main_data.voxel_sample_offset = mVCTVoxelSampleOffset;
	memcpy(mVCTMainCB->GetMappedData(), &shader_structure_cpu_vct_main_data, sizeof(ShaderStructureCPUVCTMainData));


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
		_deviceResources->ResourceBarriersBegin(mBarriers);
		mGbufferRTs[0]->TransitionTo(mBarriers, _commandListDirect, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		mGbufferRTs[1]->TransitionTo(mBarriers, _commandListDirect, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		mGbufferRTs[2]->TransitionTo(mBarriers, _commandListDirect, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		_deviceResources->ResourceBarriersEnd(mBarriers, _commandListDirect);

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
		_commandListDirect->
		_commandListDirect->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		RenderLighting(_deviceResources,
			_d3dDevice,
			_commandListDirect,
			gpuDescriptorHeap,
			directionalLightDataDescriptorHeapBlock,
			cameraDataDescriptorHandleBlock,
			shadowDepthDescriptorHandleBlock);
		RenderComposite(_deviceResources, _d3dDevice, _commandListDirect, gpuDescriptorHeap);

		//	// Draw imgui 
		//	{
		//		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesFinal[] =
		//		{
		//			 _deviceResources->GetRenderTargetView()
		//		};
		//		commandListGraphics->OMSetRenderTargets(_countof(rtvHandlesFinal), rtvHandlesFinal, FALSE, nullptr);

		//		ID3D12DescriptorHeap* ppHeaps[] = { mUIDescriptorHeap.Get() };
		//		commandListGraphics->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		//		ImGui::Render();
		//		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandListGraphics);
		//	}

			// We need to transition these resources here inside of the graphics command list,
			// since the compute command list will not allow us to transition a resource 
			// from a pixel shader resource state.
		_deviceResources->ResourceBarriersBegin(mBarriers);
		mVCTMainRT->TransitionTo(mBarriers, _commandListDirect, D3D12_RESOURCE_STATE_COMMON);
		mVCTMainUpsampleAndBlurRT->TransitionTo(mBarriers, _commandListDirect, D3D12_RESOURCE_STATE_COMMON);
		_deviceResources->ResourceBarriersEnd(mBarriers, _commandListDirect);

		_deviceResources->GetCommandQueueDirect()->Wait(fence_compute_queue.Get(), fence_unused_value_compute_queue - 1);
	}
}

void SceneRendererDirectLightingVoxelGIandAO::UpdateImGui()
{
	////capture mouse clicks
	//ImGui::GetIO().MouseDown[0] = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
	//ImGui::GetIO().MouseDown[1] = (GetKeyState(VK_RBUTTON) & 0x8000) != 0;
	//ImGui::GetIO().MouseDown[2] = (GetKeyState(VK_MBUTTON) & 0x8000) != 0;

	//ImGui_ImplDX12_NewFrame();
	//ImGui_ImplWin32_NewFrame();
	//ImGui::NewFrame();

	//{
	//	ImGui::Begin("DirectX GI Sandbox");
	//	ImGui::TextColored(ImVec4(0.95f, 0.5f, 0.0f, 1), "FPS: (%.1f FPS), %.3f ms/frame", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
	//	ImGui::Text("Camera pos: %f, %f, %f", mCameraEye.x, mCameraEye.y, mCameraEye.z);
	//	ImGui::Checkbox("Lock camera", &mLockCamera);
	//	mCamera->SetLock(mLockCamera);
	//	if (mLockCamera) {
	//		for (int i = 0; i < LOCKED_CAMERA_VIEWS; i++)
	//		{
	//			std::string name = "Mode " + std::to_string(i);
	//			if (ImGui::Button(name.c_str())) {
	//				mCamera->Reset();
	//				mCamera->SetPosition(mLockedCameraPositions[i]);
	//				mCamera->ApplyRotation(mLockedCameraRotMatrices[i]);
	//				mCamera->UpdateViewMatrix();
	//			}
	//			if (i < LOCKED_CAMERA_VIEWS - 1)
	//				ImGui::SameLine();
	//		}
	//	}

	//	ImGui::SliderFloat4("Light Color", mDirectionalLightColor, 0.0f, 1.0f);
	//	ImGui::SliderFloat4("Light Direction", mDirectionalLightDir, -1.0f, 1.0f);
	//	ImGui::SliderFloat("Light Intensity", &mDirectionalLightIntensity, 0.0f, 5.0f);
	//	ImGui::Checkbox("Dynamic Light", &mDynamicDirectionalLight);
	//	ImGui::SameLine();
	//	ImGui::SliderFloat("Speed", &mDynamicDirectionalLightSpeed, 0.0f, 5.0f);
	//	ImGui::Checkbox("Dynamic objects", &mUseDynamicObjects);
	//	if (mUseDynamicObjects) {
	//		ImGui::SameLine();
	//		ImGui::Checkbox("Stop", &mStopDynamicObjects);
	//	}

	//	ImGui::Separator();

	//	ImGui::Checkbox("Direct Light", &mUseDirectLight);
	//	ImGui::Checkbox("Direct Shadows", &mUseShadows);
	//	ImGui::Checkbox("Reflective Shadow Mapping", &mUseRSM);
	//	ImGui::Checkbox("Light Propagation Volume", &mUseLPV);
	//	ImGui::Checkbox("Voxel Cone Tracing", &mUseVCT);
	//	ImGui::Checkbox("Show AO", &mShowOnlyAO);

	//	ImGui::Separator();
	//	
	//	if (ImGui::CollapsingHeader("Global Illumination Config"))
	//	{
	//		if (ImGui::CollapsingHeader("RSM")) {
	//			ImGui::SliderFloat("RSM GI Intensity", &mRSMGIPower, 0.0f, 15.0f);
	//			ImGui::Checkbox("Use CS for main RSM pass", &mRSMComputeVersion);
	//			ImGui::Checkbox("Upsample & blur RSM result in CS", &mRSMUseUpsampleAndBlur);
	//			ImGui::Separator();
	//			ImGui::SliderFloat("RSM Rmax", &mRSMRMax, 0.0f, 1.0f);
	//			//ImGui::SliderInt("RSM Samples Count", &mRSMSamplesCount, 1, 1000);
	//		}
	//		if (ImGui::CollapsingHeader("LPV")) {
	//			ImGui::SliderFloat("LPV GI Intensity", &mLPVGIPower, 0.0f, 15.0f);
	//			ImGui::Checkbox("Use downsampled RSM", &mRSMDownsampleForLPV);
	//			ImGui::SameLine();
	//			ImGui::Checkbox("in CS", &mRSMDownsampleUseCS);
	//			ImGui::Separator();
	//			ImGui::SliderInt("Propagation steps", &mLPVPropagationSteps, 0, 100);
	//			ImGui::SliderFloat("Cutoff", &mLPVCutoff, 0.0f, 1.0f);
	//			ImGui::SliderFloat("Power", &mLPVPower, 0.0f, 2.0f);
	//			ImGui::SliderFloat("Attenuation", &mLPVAttenuation, 0.0f, 5.0f);
	//			ImGui::Separator();
	//			ImGui::Checkbox("DX12 bundles for propagation", &mUseBundleForLPVPropagation);
	//			if (mUseBundleForLPVPropagation)
	//			{
	//				ImGui::SameLine();
	//				if (ImGui::Button("Update")) {
	//					mLPVPropagationBundle1Closed = false;
	//					mLPVPropagationBundle2Closed = false;
	//					mLPVPropagationBundlesClosed = false;
	//					mLPVPropagationBundle1->Reset(mLPVPropagationBundle1Allocator.Get(), nullptr);
	//					mLPVPropagationBundle2->Reset(mLPVPropagationBundle2Allocator.Get(), nullptr);
	//				}
	//			}
	//		}
	//		if (ImGui::CollapsingHeader("VCT")) {
	//			ImGui::SliderFloat("VCT GI Intensity", &mVCTGIPower, 0.0f, 15.0f);
	//			ImGui::Checkbox("Use CS for main VCT pass", &mVCTUseMainCompute);
	//			ImGui::Checkbox("Upsample & blur VCT result in CS", &mVCTMainRTUseUpsampleAndBlur);
	//			ImGui::Separator();
	//			ImGui::SliderFloat("Diffuse Strength", &mVCTIndirectDiffuseStrength, 0.0f, 1.0f);
	//			ImGui::SliderFloat("Specular Strength", &mVCTIndirectSpecularStrength, 0.0f, 1.0f);
	//			ImGui::SliderFloat("Max Cone Trace Dist", &mVCTMaxConeTraceDistance, 0.0f, 2500.0f);
	//			ImGui::SliderFloat("AO Falloff", &mVCTAoFalloff, 0.0f, 2.0f);
	//			ImGui::SliderFloat("Sampling Factor", &mVCTSamplingFactor, 0.01f, 3.0f);
	//			ImGui::SliderFloat("Sample Offset", &mVCTVoxelSampleOffset, -0.1f, 0.1f);
	//			ImGui::Separator();
	//			ImGui::Checkbox("Render voxels for debug", &mVCTRenderDebug);
	//		}
	//	}
	//	
	//	ImGui::Separator();
	//	
	//	if (ImGui::CollapsingHeader("Extras")) {
	//		ImGui::Checkbox("DX12 Asynchronous Compute", &mUseAsyncCompute);
	//		ImGui::Checkbox("DXR Reflections", &mUseDXRReflections);
	//		if (mUseDXRReflections)
	//		{
	//			ImGui::SameLine();
	//			ImGui::Checkbox("Blur", &mDXRBlurReflections);
	//			if (mDXRBlurReflections)
	//				ImGui::SliderInt("Blur steps", &mDXRBlurPasses, 1, 100);

	//			ImGui::SliderFloat("Blend", &mDXRReflectionsBlend, 0.0f, 1.0f);
	//		}
	//	}

	//	ImGui::End();
	//}
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

	mGbufferRTs.push_back(new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, rtFormat, flags, L"Albedo"));

	rtFormat = DXGI_FORMAT_R16G16B16A16_SNORM;
	mGbufferRTs.push_back(new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, rtFormat, flags, L"Normals"));

	rtFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	mGbufferRTs.push_back(new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, rtFormat, flags, L"World Positions"));

	// Root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	mGbufferRS.Reset(2, 1);
	mGbufferRS.InitStaticSampler(0, sampler, D3D12_SHADER_VISIBILITY_PIXEL);
	mGbufferRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
	mGbufferRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, D3D12_SHADER_VISIBILITY_ALL);
	mGbufferRS.Finalize(device, L"GPrepassRS", rootSignatureFlags);

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

	mGbufferPSO.SetRootSignature(mGbufferRS);
	mGbufferPSO.SetRasterizerState(mRasterizerState);
	mGbufferPSO.SetBlendState(mBlendState);
	mGbufferPSO.SetDepthStencilState(mDepthStateRW);
	mGbufferPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	mGbufferPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	mGbufferPSO.SetRenderTargetFormats(_countof(formats), formats, DXGI_FORMAT_D32_FLOAT);
	mGbufferPSO.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
	mGbufferPSO.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
	mGbufferPSO.Finalize(device);

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

		commandList->SetPipelineState(mGbufferPSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mGbufferRS.GetSignature());

		// Transition buffers to rendertarget outputs
		_deviceResources->ResourceBarriersBegin(mBarriers);
		mGbufferRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mGbufferRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mGbufferRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		_deviceResources->ResourceBarriersEnd(mBarriers, commandList);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] =
		{
			mGbufferRTs[0]->GetRTV().GetCPUHandle(),
			mGbufferRTs[1]->GetRTV().GetCPUHandle(),
			mGbufferRTs[2]->GetRTV().GetCPUHandle(),
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

		_deviceResources->ResourceBarriersBegin(mBarriers);
		mShadowDepth->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		_deviceResources->ResourceBarriersEnd(mBarriers, commandList);

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
		viewport_voxel_cone_tracing_voxelization = CD3DX12_VIEWPORT(0.0f, 0.0f, VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE);
		scissor_rect_voxel_cone_tracing_voxelization = CD3DX12_RECT(0.0f, 0.0f, VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE);
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

		mVCTVoxelization3DRT = new DXRSRenderTarget(device, descriptorManager, VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE, format, flags, L"Voxelization Scene Data 3D", VCT_SCENE_VOLUME_SIZE);
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
		mVCTVoxelizationCB = new DX12Buffer(descriptorManager,
			cbDesc,
			1,
			L"VCT Voxelization Pass CB");
	}

	// Debug 
	{
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		mVCTVoxelizationDebugRT = new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, format, flags, L"Voxelization Debug RT");

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

	// Aniso mipmapping prepare
	{
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		int size = VCT_SCENE_VOLUME_SIZE >> 1;
		mVCTAnisoMipmappinPrepare3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare X+ 3D", size, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mVCTAnisoMipmappinPrepare3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare X- 3D", size, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mVCTAnisoMipmappinPrepare3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare Y+ 3D", size, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mVCTAnisoMipmappinPrepare3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare Y- 3D", size, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mVCTAnisoMipmappinPrepare3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare Z+ 3D", size, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mVCTAnisoMipmappinPrepare3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Prepare Z- 3D", size, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

		mVCTAnisoMipmappingPrepareRS.Reset(3, 0);
		mVCTAnisoMipmappingPrepareRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTAnisoMipmappingPrepareRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTAnisoMipmappingPrepareRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 6, D3D12_SHADER_VISIBILITY_ALL);
		mVCTAnisoMipmappingPrepareRS.Finalize(device, L"VCT aniso mipmapping prepare pass compute version RS", rootSignatureFlags);

		char* pComputeShaderByteCode;
		int computeShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("VoxelConeTracingAnisoMipmapPrepare_CS.cso"), pComputeShaderByteCode, computeShaderByteCodeLength);

		mVCTAnisoMipmappingPreparePSO.SetRootSignature(mVCTAnisoMipmappingPrepareRS);
		mVCTAnisoMipmappingPreparePSO.SetComputeShader(pComputeShaderByteCode, computeShaderByteCodeLength);
		mVCTAnisoMipmappingPreparePSO.Finalize(device);

		delete[] pComputeShaderByteCode;

		DX12Buffer::Description cbDesc;
		cbDesc.mElementSize = c_aligned_shader_structure_cpu_mip_mapping_data;
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

		mVCTAnisoMipmappingCB = new DX12Buffer(descriptorManager, 
			_deviceResources->GetCommandListDirect(), 
			cbDesc, 
			1,
			L"VCT aniso mip mapping CB");
	}

	// Aniso mipmapping main
	{
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		int size = VCT_SCENE_VOLUME_SIZE >> 1;
		mVCTAnisoMipmappinMain3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Main X+ 3D", size, VCT_MIPS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mVCTAnisoMipmappinMain3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Main X- 3D", size, VCT_MIPS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mVCTAnisoMipmappinMain3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Main Y+ 3D", size, VCT_MIPS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mVCTAnisoMipmappinMain3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Main Y- 3D", size, VCT_MIPS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mVCTAnisoMipmappinMain3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Main Z+ 3D", size, VCT_MIPS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		mVCTAnisoMipmappinMain3DRTs.push_back(new DXRSRenderTarget(device, descriptorManager, size, size, format, flags, L"Voxelization Scene Mip Main Z- 3D", size, VCT_MIPS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

		mVCTAnisoMipmappingMainRS.Reset(2, 0);
		mVCTAnisoMipmappingMainRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTAnisoMipmappingMainRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 12, D3D12_SHADER_VISIBILITY_ALL);
		mVCTAnisoMipmappingMainRS.Finalize(device, L"VCT aniso mipmapping main pass comptue version RS", rootSignatureFlags);

		char* pComputeShaderByteCode;
		int computeShaderByteCodeLength;
		LoadShaderByteCode(GetFilePath("VoxelConeTracingAnisoMipmapMain_CS.cso"), pComputeShaderByteCode, computeShaderByteCodeLength);

		mVCTAnisoMipmappingMainPSO.SetRootSignature(mVCTAnisoMipmappingMainRS);
		mVCTAnisoMipmappingMainPSO.SetComputeShader(pComputeShaderByteCode, computeShaderByteCodeLength);
		mVCTAnisoMipmappingMainPSO.Finalize(device);

		delete[] pComputeShaderByteCode;

		DX12Buffer::Description cbDesc;
		cbDesc.mElementSize = c_aligned_shader_structure_cpu_mip_mapping_data;
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(descriptorManager, cbDesc, 1, L"VCT aniso mip mapping main mip 0 CB"));
		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(descriptorManager, cbDesc, 1, L"VCT aniso mip mapping main mip 1 CB"));
		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(descriptorManager, cbDesc, 1, L"VCT aniso mip mapping main mip 2 CB"));
		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(descriptorManager, cbDesc, 1, L"VCT aniso mip mapping main mip 3 CB"));
		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(descriptorManager, cbDesc, 1, L"VCT aniso mip mapping main mip 4 CB"));
		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(descriptorManager, cbDesc, 1, L"VCT aniso mip mapping main mip 5 CB"));

	}

	// Voxel cone tracing main 
	{
		DX12Buffer::Description cbDesc;
		cbDesc.mElementSize = c_aligned_shader_structure_cpu_vct_main_data;
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

		mVCTMainCB = new DX12Buffer(descriptorManager,
			cbDesc, 
			1,
			L"VCT main CB");
		
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		mVCTMainRT = new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH * mVCTRTRatio, MAX_SCREEN_HEIGHT * mVCTRTRatio, format, flags, L"VCT Final Output", -1, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// Create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		// PS
		{
			mVCTMainRS.Reset(3, 1);
			mVCTMainRS.InitStaticSampler(0, mBilinearSampler, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 1, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS.Finalize(device, L"VCT main pass pixel version RS", rootSignatureFlags);

			char* pVertexShaderByteCode;
			int vertexShaderByteCodeLength;
			char* pPixelShaderByteCode;
			int pixelShaderByteCodeLength;
			LoadShaderByteCode(GetFilePath("VoxelConeTracing_VS.cso"), pVertexShaderByteCode, vertexShaderByteCodeLength);
			LoadShaderByteCode(GetFilePath("VoxelConeTracing_PS.cso"), pPixelShaderByteCode, pixelShaderByteCodeLength);

			D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			DXGI_FORMAT formats[1];
			formats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

			mVCTMainPSO.SetRootSignature(mVCTMainRS);
			mVCTMainPSO.SetRasterizerState(mRasterizerState);
			mVCTMainPSO.SetBlendState(mBlendState);
			mVCTMainPSO.SetDepthStencilState(mDepthStateDisabled);
			mVCTMainPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
			mVCTMainPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			mVCTMainPSO.SetRenderTargetFormats(_countof(formats), formats, DXGI_FORMAT_D32_FLOAT);
			mVCTMainPSO.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
			mVCTMainPSO.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
			mVCTMainPSO.Finalize(device);

			delete[] pVertexShaderByteCode;
			delete[] pPixelShaderByteCode;
		}

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
		mVCTMainUpsampleAndBlurRT = new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT,
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
	if (!mUseVCT)
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

			_deviceResources->ResourceBarriersBegin(mBarriers);
			mVCTVoxelization3DRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mShadowDepth->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			_deviceResources->ResourceBarriersEnd(mBarriers, commandList);
			
			DX12DescriptorHandleBlock vctVoxelizationRenderTargetDescriptorHandleBlock;
			vctVoxelizationRenderTargetDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
			vctVoxelizationRenderTargetDescriptorHandleBlock.Add(mVCTVoxelization3DRT->GetUAV());
			
			commandList->ClearUnorderedAccessViewFloat(vctVoxelizationRenderTargetDescriptorHandleBlock.GetGPUHandle(), 
				mVCTVoxelization3DRT->GetUAV().GetCPUHandle(), 
				mVCTVoxelization3DRT->GetResource(), 
				gs_clear_color_black, 
				0, 
				nullptr);

			DX12DescriptorHandleBlock vctVoxelizationDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
			vctVoxelizationDescriptorHandleBlock.Add(mVCTVoxelizationCB->GetCBV());

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

			//_deviceResources->ResourceBarriersBegin(mBarriers);
			//mVCTVoxelization3DRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			//_deviceResources->ResourceBarriersEnd(mBarriers, commandList);

			//DX12DescriptorHandleBlock cbvHandle;
			//DX12DescriptorHandleBlock uavHandle;

			//cbvHandle = gpuDescriptorHeap->GetHandleBlock(1);
			//gpuDescriptorHeap->AddToHandle(device, cbvHandle, mVCTVoxelizationCB->GetCBV());

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
		// Anisotropic Prepare
		{
			commandList->SetPipelineState(mVCTAnisoMipmappingPreparePSO.GetPipelineStateObject());
			commandList->SetComputeRootSignature(mVCTAnisoMipmappingPrepareRS.GetSignature());
		
			_deviceResources->ResourceBarriersBegin(mBarriers);
			mVCTVoxelization3DRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			mVCTAnisoMipmappinPrepare3DRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[3]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[4]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[5]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			_deviceResources->ResourceBarriersEnd(mBarriers, commandList);
		
			shader_structure_cpu_mip_mapping_data.mip_dimension = VCT_SCENE_VOLUME_SIZE >> 1;
			shader_structure_cpu_mip_mapping_data.mip_level = 0;
			memcpy(mVCTAnisoMipmappingCB->GetMappedData(), &shader_structure_cpu_mip_mapping_data, sizeof(ShaderStructureCPUMipMappingData));
			DX12DescriptorHandleBlock mipMappingDataDescriptorHandleBlock = gpuDescriptorHeap->GetHandleBlock();
			mipMappingDataDescriptorHandleBlock.Add(mVCTAnisoMipmappingCB->GetCBV());
		
			DX12DescriptorHandleBlock uavHandle = gpuDescriptorHeap->GetHandleBlock(6);
			uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[0]->GetUAV());
			uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[1]->GetUAV());
			uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[2]->GetUAV());
			uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[3]->GetUAV());
			uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[4]->GetUAV());
			uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[5]->GetUAV());

			DX12DescriptorHandleBlock srvHandle;
			srvHandle = gpuDescriptorHeap->GetHandleBlock();
			srvHandle.Add(mVCTVoxelization3DRT->GetSRV());
		
			commandList->SetComputeRootDescriptorTable(0, mipMappingDataDescriptorHandleBlock.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(1, srvHandle.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(2, uavHandle.GetGPUHandle());
		
			commandList->Dispatch(DivideByMultiple(static_cast<UINT>(shader_structure_cpu_mip_mapping_data.mip_dimension), 8u), DivideByMultiple(static_cast<UINT>(shader_structure_cpu_mip_mapping_data.mip_dimension), 8u), DivideByMultiple(static_cast<UINT>(shader_structure_cpu_mip_mapping_data.mip_dimension), 8u));
		}

		// Anisotropic Main
		{
			commandList->SetPipelineState(mVCTAnisoMipmappingMainPSO.GetPipelineStateObject());
			commandList->SetComputeRootSignature(mVCTAnisoMipmappingMainRS.GetSignature());

			_deviceResources->ResourceBarriersBegin(mBarriers);
			mVCTAnisoMipmappinMain3DRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinMain3DRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinMain3DRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinMain3DRTs[3]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinMain3DRTs[4]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinMain3DRTs[5]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			_deviceResources->ResourceBarriersEnd(mBarriers, commandList);

			int mipDimension = VCT_SCENE_VOLUME_SIZE >> 1;
			for (int mip = 0; mip < VCT_MIPS; mip++)
			{
				DX12DescriptorHandleBlock cbvHandle = gpuDescriptorHeap->GetHandleBlock(1);
				cbvHandle.Add(mVCTAnisoMipmappingMainCB[mip]->GetCBV());

				DX12DescriptorHandleBlock uavHandle = gpuDescriptorHeap->GetHandleBlock(12);
				if (mip == 0) 
				{
					uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[0]->GetUAV());
					uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[1]->GetUAV());
					uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[2]->GetUAV());
					uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[3]->GetUAV());
					uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[4]->GetUAV());
					uavHandle.Add(mVCTAnisoMipmappinPrepare3DRTs[5]->GetUAV());
				}
				else 
				{
					uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[0]->GetUAV(mip - 1));
					uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[1]->GetUAV(mip - 1));
					uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[2]->GetUAV(mip - 1));
					uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[3]->GetUAV(mip - 1));
					uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[4]->GetUAV(mip - 1));
					uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[5]->GetUAV(mip - 1));
				}

				uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[0]->GetUAV(mip));
				uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[1]->GetUAV(mip));
				uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[2]->GetUAV(mip));
				uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[3]->GetUAV(mip));
				uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[4]->GetUAV(mip));
				uavHandle.Add(mVCTAnisoMipmappinMain3DRTs[5]->GetUAV(mip));

				shader_structure_cpu_mip_mapping_data.mip_dimension = mipDimension;
				shader_structure_cpu_mip_mapping_data.mip_level = mip;
				memcpy(mVCTAnisoMipmappingMainCB[mip]->GetMappedData(), &shader_structure_cpu_mip_mapping_data, sizeof(ShaderStructureCPUMipMappingData));
				commandList->SetComputeRootDescriptorTable(0, cbvHandle.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(1, uavHandle.GetGPUHandle());

				commandList->Dispatch(DivideByMultiple(static_cast<UINT>(shader_structure_cpu_mip_mapping_data.mip_dimension), 8u), DivideByMultiple(static_cast<UINT>(shader_structure_cpu_mip_mapping_data.mip_dimension), 8u), DivideByMultiple(static_cast<UINT>(shader_structure_cpu_mip_mapping_data.mip_dimension), 8u));
				mipDimension >>= 1;
			}
		}
		
		// Voxel cone tracing main
		{
			commandList->SetPipelineState(mVCTMainPSO_Compute.GetPipelineStateObject());
			commandList->SetComputeRootSignature(mVCTMainRS_Compute.GetSignature());

			_deviceResources->ResourceBarriersBegin(mBarriers);
			mVCTMainRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinMain3DRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			mVCTAnisoMipmappinMain3DRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			mVCTAnisoMipmappinMain3DRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			mVCTAnisoMipmappinMain3DRTs[3]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			mVCTAnisoMipmappinMain3DRTs[4]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			mVCTAnisoMipmappinMain3DRTs[5]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			_deviceResources->ResourceBarriersEnd(mBarriers, commandList);

			DX12DescriptorHandleBlock uavHandle = gpuDescriptorHeap->GetHandleBlock(1);
			uavHandle.Add(mVCTMainRT->GetUAV());

			DX12DescriptorHandleBlock srvHandle = gpuDescriptorHeap->GetHandleBlock(10);
			srvHandle.Add(mGbufferRTs[0]->GetSRV());
			srvHandle.Add(mGbufferRTs[1]->GetSRV());
			srvHandle.Add(mGbufferRTs[2]->GetSRV());

			srvHandle.Add(mVCTAnisoMipmappinMain3DRTs[0]->GetSRV());
			srvHandle.Add(mVCTAnisoMipmappinMain3DRTs[1]->GetSRV());
			srvHandle.Add(mVCTAnisoMipmappinMain3DRTs[2]->GetSRV());
			srvHandle.Add(mVCTAnisoMipmappinMain3DRTs[3]->GetSRV());
			srvHandle.Add(mVCTAnisoMipmappinMain3DRTs[4]->GetSRV());
			srvHandle.Add(mVCTAnisoMipmappinMain3DRTs[5]->GetSRV());
			srvHandle.Add(mVCTVoxelization3DRT->GetSRV());

			DX12DescriptorHandleBlock cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			cbvHandle.Add(mVCTVoxelizationCB->GetCBV());
			cbvHandle.Add(mVCTMainCB->GetCBV());

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

				_deviceResources->ResourceBarriersBegin(mBarriers);
				mVCTMainRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				mVCTMainUpsampleAndBlurRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				_deviceResources->ResourceBarriersEnd(mBarriers, commandList);

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
	mLightingRT = new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT,
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
		1,
		L"Lighting Pass CB");

	cbDesc.mElementSize = c_aligned_shader_structure_cpu_illumination_flags_data;
	mIlluminationFlagsCB = new DX12Buffer(descriptorManager,
		cbDesc, 
		1,
		L"Illumination Flags CB");
}
void SceneRendererDirectLightingVoxelGIandAO::RenderLighting(DX12DeviceResourcesSingleton* _deviceResources, 
	ID3D12Device* device, 
	ID3D12GraphicsCommandList* commandList, 
	DX12DescriptorHeapGPU* gpuDescriptorHeap,
	DX12DescriptorHandleBlock& directionalLightDescriptorHandleBlock,
	DX12DescriptorHandleBlock& cameraDataDescriptorHandleBlock,
	DX12DescriptorHandleBlock& shadowDepthDescriptorHandleBlock)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _deviceResources->GetOutputSize().right, _deviceResources->GetOutputSize().bottom);

	{
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &rect);

		commandList->SetPipelineState(mLightingPSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mLightingRS.GetSignature());

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesLighting[] =
		{
			mLightingRT->GetRTV().GetCPUHandle()
		};

		_deviceResources->ResourceBarriersBegin(mBarriers);
		mLightingRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mVCTMainUpsampleAndBlurRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mVCTMainRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mGbufferRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mGbufferRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mGbufferRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mShadowDepth->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		_deviceResources->ResourceBarriersEnd(mBarriers, commandList);

		commandList->OMSetRenderTargets(_countof(rtvHandlesLighting), rtvHandlesLighting, FALSE, nullptr);
		commandList->ClearRenderTargetView(rtvHandlesLighting[0], gs_clear_color_black, 0, nullptr);

		DX12DescriptorHandleBlock cbvHandleLighting = gpuDescriptorHeap->GetHandleBlock(2);
		cbvHandleLighting.Add(mLightingCB->GetCBV());
		cbvHandleLighting.Add(mIlluminationFlagsCB->GetCBV());

		DX12DescriptorHandleBlock srvHandleLighting = gpuDescriptorHeap->GetHandleBlock(4);
		srvHandleLighting.Add(mGbufferRTs[0]->GetSRV());
		srvHandleLighting.Add(mGbufferRTs[1]->GetSRV());
		srvHandleLighting.Add(mGbufferRTs[2]->GetSRV());

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

		commandList->IASetVertexBuffers(0, 1, &_deviceResources->GetFullscreenQuadBufferView());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		commandList->DrawInstanced(4, 1, 0, 0);
	}
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
	DX12DescriptorHeapGPU* gpuDescriptorHeap)
{
	{
		commandList->SetPipelineState(mCompositePSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mCompositeRS.GetSignature());

		_deviceResources->ResourceBarriersBegin(mBarriers);
		mLightingRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		_deviceResources->ResourceBarriersEnd(mBarriers, commandList);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesFinal[] =
		{
			 _deviceResources->GetRenderTargetView()
		};
		commandList->OMSetRenderTargets(_countof(rtvHandlesFinal), rtvHandlesFinal, FALSE, nullptr);
		commandList->ClearRenderTargetView(_deviceResources->GetRenderTargetView(), Colors::CornflowerBlue, 0, nullptr);
		
		DX12DescriptorHandleBlock srvHandleComposite = gpuDescriptorHeap->GetHandleBlock(1);
		srvHandleComposite.Add(mLightingRT->GetSRV());
		
		commandList->SetGraphicsRootDescriptorTable(0, srvHandleComposite.GetGPUHandle());
		commandList->IASetVertexBuffers(0, 1, &_deviceResources->GetFullscreenQuadBufferView());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		commandList->DrawInstanced(4, 1, 0, 0);
	}
}