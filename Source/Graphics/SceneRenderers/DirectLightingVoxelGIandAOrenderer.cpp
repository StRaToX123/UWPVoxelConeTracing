#include "Graphics\SceneRenderers\DirectLightingVoxelGIandAOrenderer.h"



namespace 
{
	D3D12_HEAP_PROPERTIES UploadHeapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
	D3D12_HEAP_PROPERTIES DefaultHeapProps = { D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };

	static const float clearColorBlack[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	static const float clearColorWhite[] = { 1.0f, 1.0f, 1.0f, 1.0f };
}

DirectLightingVoxelGIandAOrenderer::DirectLightingVoxelGIandAOrenderer()
{
}

DirectLightingVoxelGIandAOrenderer::~DirectLightingVoxelGIandAOrenderer()
{
	delete mLightingCB;
	delete mGbufferCB;
}

void DirectLightingVoxelGIandAOrenderer::Initialize(Windows::UI::Core::CoreWindow^ coreWindow, DX12DeviceResources* _deviceResources)
{
	_device_resources = _deviceResources;
	descriptor_heap_manager.Initialize(_device_resources->GetD3DDevice());

	
	ID3D12Device* device = _device_resources->GetD3DDevice();

	mStates = std::make_unique<CommonStates>(device);
	mGraphicsMemory = std::make_unique<GraphicsMemory>(device);

	_device_resources->CreateWindowResources();

	auto size = _device_resources->GetOutputSize();
	float aspectRatio = float(size.right) / float(size.bottom);

	#pragma region ImGui

	//// Setup Dear ImGui context
	//IMGUI_CHECKVERSION();
	//ImGui::CreateContext();
	//ImGuiIO& io = ImGui::GetIO(); (void)io;
	////io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	////io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	//// Setup Dear ImGui style
	//ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	//// Setup Platform/Renderer bindings
	////ImGui_ImplWin32_Init(window);

	//D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	//desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	//desc.NumDescriptors = 1;
	//desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	//ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mUIDescriptorHeap)));
	//ImGui_ImplDX12_Init(device, 3, DXGI_FORMAT_R8G8B8A8_UNORM, mUIDescriptorHeap.Get(), mUIDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), mUIDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

#pragma endregion

	mDepthStencil = new DXRSDepthBuffer(device, &descriptor_heap_manager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, DXGI_FORMAT_D32_FLOAT);

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
	//bilinearSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_EQUAL;
	mBilinearSampler.MipLODBias = 0;
	mBilinearSampler.MaxAnisotropy = 16;
	mBilinearSampler.MinLOD = 0.0f;
	mBilinearSampler.MaxLOD = D3D12_FLOAT32_MAX;
	mBilinearSampler.BorderColor[0] = 0.0f;
	mBilinearSampler.BorderColor[1] = 0.0f;
	mBilinearSampler.BorderColor[2] = 0.0f;
	mBilinearSampler.BorderColor[3] = 0.0f;

#pragma endregion

	//create upsample & blur buffer
	DX12Buffer::Description cbDesc;
	cbDesc.mElementSize = sizeof(UpsampleAndBlurBuffer);
	cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
	cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;
	mGIUpsampleAndBlurBuffer = new DX12Buffer(_device_resources->GetD3DDevice(), &descriptor_heap_manager, _device_resources->GetCommandListGraphics(), cbDesc, L"GI Upsample & Blur CB");
	mDXRBlurBuffer = new DX12Buffer(_device_resources->GetD3DDevice(), &descriptor_heap_manager, _device_resources->GetCommandListGraphics(), cbDesc, L"DXR Blur CB");
	
	UpsampleAndBlurBuffer ubData = {};
	ubData.Upsample = true;
	memcpy(mGIUpsampleAndBlurBuffer->Map(), &ubData, sizeof(ubData));
	ubData.Upsample = false;
	memcpy(mDXRBlurBuffer->Map(), &ubData, sizeof(ubData));

	InitGbuffer(device, &descriptor_heap_manager);
	InitShadowMapping(device, &descriptor_heap_manager);
	InitVoxelConeTracing(device, &descriptor_heap_manager);
	InitLighting(device, &descriptor_heap_manager);
	InitComposite(device, &descriptor_heap_manager);
}

void DirectLightingVoxelGIandAOrenderer::Clear(ID3D12GraphicsCommandList* cmdList)
{
	auto rtvDescriptor = _device_resources->GetRenderTargetView();
	auto dsvDescriptor = _device_resources->GetDepthStencilView();

	//cmdList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
	//cmdList->ClearRenderTargetView(rtvDescriptor, Colors::Green, 0, nullptr);
	cmdList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	auto viewport = _device_resources->GetScreenViewport();
	auto scissorRect = _device_resources->GetScissorRect();
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);
}

//void DXRSExampleGIScene::Run(DXRSTimer& mTimer, Camera& camera)
//{
//	
//
//
//	Render(mTimer);
//}

void DirectLightingVoxelGIandAOrenderer::Render(std::vector<Model*>& scene, DirectionalLight& directionalLight, DXRSTimer& mTimer, Camera& camera)
{
	mTimer.Tick([&]()
	{
		Update(mTimer, camera);
	});

	if (mTimer.GetFrameCount() == 0)
	{
		return;
	}
		
	_device_resources->Prepare(D3D12_RESOURCE_STATE_PRESENT, mUseAsyncCompute ? (mTimer.GetFrameCount() == 1) : true);

	auto commandListGraphics = _device_resources->GetCommandListGraphics(0);
	auto commandListGraphics2 = _device_resources->GetCommandListGraphics(1);
	auto commandListCompute = _device_resources->GetCommandListCompute();

	ID3D12CommandList* ppCommandLists[] = { commandListGraphics };
	ID3D12CommandList* ppCommandLists2[] = { commandListGraphics2 };

	Clear(commandListGraphics);

	auto device = _device_resources->GetD3DDevice();

	DX12DescriptorHeapGPU* gpuDescriptorHeap = descriptor_heap_manager.GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gpuDescriptorHeap->Reset();

	ID3D12DescriptorHeap* ppHeaps[] = { gpuDescriptorHeap->GetHeap() };

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// 																									//
	// Frame timeline:																					//
	//     Gfx Queue:     [----gfx cmd list #1----|----gfx cmd list #2----|----gfx cmd list #1----]		//
	//     Async Queue:   [-----------------------|-------cmd list--------|-----------------------]		//
	//																									//
	// 																									//
	//////////////////////////////////////////////////////////////////////////////////////////////////////

	//process compute command list - async during the frame (GI)
	{
		if (mTimer.GetFrameCount() > 1) 
		{
			commandListCompute->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

			RenderVoxelConeTracing(device, commandListCompute, gpuDescriptorHeap, scene, COMPUTE_QUEUE);

			_device_resources->ResourceBarriersBegin(mBarriers);
			mVCTMainRT->TransitionTo(mBarriers, commandListCompute, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			mVCTMainUpsampleAndBlurRT->TransitionTo(mBarriers, commandListCompute, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			_device_resources->ResourceBarriersEnd(mBarriers, commandListCompute);

			_device_resources->WaitForGraphicsFence2ToFinish(_device_resources->GetCommandQueueCompute());
			_device_resources->PresentCompute(); //execute compute queue and signal graphics queue to continue its' execution
		}
	}

	//process graphics command list 1 - start of the frame (G-buffer, copies for async compute)
	{
		commandListGraphics->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _device_resources->GetOutputSize().right, _device_resources->GetOutputSize().bottom);
		CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _device_resources->GetOutputSize().right, _device_resources->GetOutputSize().bottom);
		commandListGraphics->RSSetViewports(1, &viewport);
		commandListGraphics->RSSetScissorRects(1, &rect);

		if (mTimer.GetFrameCount() > 1) 
		{
			//PIXBeginEvent(commandListGraphics, 0, "Copy buffers for async");
			{
				auto stateVCT3D = mVCTVoxelization3DRT->GetCurrentState();

				_device_resources->ResourceBarriersBegin(mBarriers);
				mVCTVoxelization3DRT->TransitionTo(mBarriers, commandListGraphics, D3D12_RESOURCE_STATE_COPY_SOURCE);
				mVCTVoxelization3DRT_CopyForAsync->TransitionTo(mBarriers, commandListGraphics, D3D12_RESOURCE_STATE_COPY_DEST);
				_device_resources->ResourceBarriersEnd(mBarriers, commandListGraphics);

				commandListGraphics->CopyResource(mVCTVoxelization3DRT_CopyForAsync->GetResource(), mVCTVoxelization3DRT->GetResource());

				_device_resources->ResourceBarriersBegin(mBarriers);
				mVCTVoxelization3DRT->TransitionTo(mBarriers, commandListGraphics, stateVCT3D);
				mVCTVoxelization3DRT_CopyForAsync->TransitionTo(mBarriers, commandListGraphics, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				_device_resources->ResourceBarriersEnd(mBarriers, commandListGraphics);
			}
			//PIXEndEvent(commandListGraphics);
		}

		RenderGbuffer(device, commandListGraphics, gpuDescriptorHeap, scene);

		commandListGraphics->Close();
		_device_resources->GetCommandQueueGraphics()->ExecuteCommandLists(1, ppCommandLists);
		_device_resources->SignalGraphicsFence2();
	}

	//process graphics command list 2 - middle of the frame (Shadows, GI) 
	{
		if (mTimer.GetFrameCount() > 1) 
		{

			commandListGraphics2->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

			RenderShadowMapping(device, commandListGraphics2, gpuDescriptorHeap, scene, directionalLight);
			RenderVoxelConeTracing(device, commandListGraphics2, gpuDescriptorHeap, scene, GRAPHICS_QUEUE);//only voxelization there which cant go to compute

			_device_resources->WaitForGraphicsFence2ToFinish(_device_resources->GetCommandQueueGraphics(), true);

			commandListGraphics2->Close();
			_device_resources->GetCommandQueueGraphics()->ExecuteCommandLists(1, ppCommandLists2);
		}
	}

	//process graphics command list 1 - end of the frame (Lighting, Composite, UI)
	{
		// submit the rest of the frame to graphics queue
		commandListGraphics->Reset(_device_resources->GetCommandAllocatorGraphics(), nullptr);
		commandListGraphics->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		_device_resources->ResourceBarriersBegin(mBarriers);
		mVCTMainRT->TransitionTo(mBarriers, commandListGraphics, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mVCTMainUpsampleAndBlurRT->TransitionTo(mBarriers, commandListGraphics, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mGbufferRTs[1]->TransitionTo(mBarriers, commandListGraphics, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mGbufferRTs[2]->TransitionTo(mBarriers, commandListGraphics, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		_device_resources->ResourceBarriersEnd(mBarriers, commandListGraphics);

		RenderLighting(device, commandListGraphics, gpuDescriptorHeap);
		RenderComposite(device, commandListGraphics, gpuDescriptorHeap);

		//draw imgui 
		/*PIXBeginEvent(commandListGraphics, 0, "ImGui");
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesFinal[] =
			{
				 _device_resources->GetRenderTargetView()
			};
			commandListGraphics->OMSetRenderTargets(_countof(rtvHandlesFinal), rtvHandlesFinal, FALSE, nullptr);

			ID3D12DescriptorHeap* ppHeaps[] = { mUIDescriptorHeap.Get() };
			commandListGraphics->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandListGraphics);
		}
		PIXEndEvent(commandListGraphics);*/

		_device_resources->ResourceBarriersBegin(mBarriers);
		mVCTMainRT->TransitionTo(mBarriers, commandListGraphics, D3D12_RESOURCE_STATE_COMMON);
		mVCTMainUpsampleAndBlurRT->TransitionTo(mBarriers, commandListGraphics, D3D12_RESOURCE_STATE_COMMON);
		_device_resources->ResourceBarriersEnd(mBarriers, commandListGraphics);

		_device_resources->WaitForComputeToFinish(); //wait for compute queue to finish the current frame
		_device_resources->Present();
		mGraphicsMemory->Commit(_device_resources->GetCommandQueueGraphics());
	}
}

void DirectLightingVoxelGIandAOrenderer::Update(DXRSTimer const& timer, Camera& camera)
{
	UpdateCamera(timer, camera);
	UpdateLights(timer);
	UpdateBuffers(timer);
	//UpdateImGui();
	UpdateTransforms(timer);
}

void DirectLightingVoxelGIandAOrenderer::UpdateTransforms(DXRSTimer const& timer) 
{
	/*if (mUseDynamicObjects && !mStopDynamicObjects) 
	{
		for (auto& model : mRenderableObjects) 
		{
			if (model->GetIsDynamic())
			{
				XMFLOAT3 trans = model->GetTranslation();
				model->UpdateWorldMatrix(XMMatrixTranslation(
					trans.x,
					trans.y + sin(timer.GetTotalSeconds() * model->GetAmplitude()) * model->GetSpeed(),
					trans.z
				));
			}
		}
	}*/
}

void DirectLightingVoxelGIandAOrenderer::UpdateBuffers(DXRSTimer const& timer)
{
	float width = _device_resources->GetOutputSize().right;
	float height = _device_resources->GetOutputSize().bottom;
	GBufferCBData gbufferPassData;
	gbufferPassData.ViewProjection = XMMatrixTranspose(XMMatrixMultiply(mCameraView, mCameraProjection));
	memcpy(mGbufferCB->Map(), &gbufferPassData, sizeof(gbufferPassData));

	LightingCBData lightPassData = {};
	lightPassData.InvViewProjection = gbufferPassData.InvViewProjection;
	lightPassData.ShadowViewProjection = XMMatrixTranspose(mLightViewProjection);
	lightPassData.ShadowTexelSize = XMFLOAT2(1.0f / mShadowDepth->GetWidth(), 1.0f / mShadowDepth->GetHeight());
	lightPassData.ShadowIntensity = mShadowIntensity;
	lightPassData.CameraPos = XMFLOAT4(mCameraEye.x, mCameraEye.y, mCameraEye.z, 1);
	lightPassData.ScreenSize = { width, height, 1.0f / width, 1.0f / height };
	memcpy(mLightingCB->Map(), &lightPassData, sizeof(lightPassData));

	IlluminationFlagsCBData illumData = {};
	illumData.useDirect = mUseDirectLight ? 1 : 0;
	illumData.useShadows = mUseShadows ? 1 : 0;
	illumData.useVCT = mUseVCT ? 1 : 0;
	illumData.useVCTDebug = mVCTRenderDebug ? 1 : 0;
	illumData.vctGIPower = mVCTGIPower;
	illumData.showOnlyAO = mShowOnlyAO ? 1 : 0;
	memcpy(mIlluminationFlagsCB->Map(), &illumData, sizeof(illumData));

	VCTVoxelizationCBData voxelData = {};
	float scale = 1.0f;// VCT_SCENE_VOLUME_SIZE / mWorldVoxelScale;
	voxelData.WorldVoxelCube = XMMatrixTranspose(XMMatrixMultiply(XMMatrixScaling(scale, -scale, scale), XMMatrixTranslation(-VCT_SCENE_VOLUME_SIZE * 0.25f, -VCT_SCENE_VOLUME_SIZE * 0.25f, -VCT_SCENE_VOLUME_SIZE * 0.25f)));
	voxelData.ViewProjection = XMMatrixTranspose(XMMatrixMultiply(mCameraView, mCameraProjection));
	voxelData.ShadowViewProjection = XMMatrixTranspose(mLightViewProjection);
	voxelData.WorldVoxelScale = mWorldVoxelScale;
	memcpy(mVCTVoxelizationCB->Map(), &voxelData, sizeof(voxelData));

	VCTMainCBData vctMainData = {};
	vctMainData.CameraPos = XMFLOAT4(mCameraEye.x, mCameraEye.y, mCameraEye.z, 1);
	vctMainData.UpsampleRatio = XMFLOAT2(mGbufferRTs[0]->GetWidth() / mVCTMainRT->GetWidth(), mGbufferRTs[0]->GetHeight() / mVCTMainRT->GetHeight());
	vctMainData.IndirectDiffuseStrength = mVCTIndirectDiffuseStrength;
	vctMainData.IndirectSpecularStrength = mVCTIndirectSpecularStrength;
	vctMainData.MaxConeTraceDistance = mVCTMaxConeTraceDistance;
	vctMainData.AOFalloff = mVCTAoFalloff;
	vctMainData.SamplingFactor = mVCTSamplingFactor;
	vctMainData.VoxelSampleOffset = mVCTVoxelSampleOffset;
	memcpy(mVCTMainCB->Map(), &vctMainData, sizeof(vctMainData));
}

void DirectLightingVoxelGIandAOrenderer::UpdateImGui()
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

//void DirectLightingVoxelGIandAOrenderer::UpdateLights(DXRSTimer const& timer)
//{
//	
//		//mDirectionalLightDir[0] = sin(timer.GetTotalSeconds() * mDynamicDirectionalLightSpeed);
//
//	LightsInfoCBData lightData = {};
//	lightData.LightColor = XMFLOAT4(mDirectionalLightColor[0], mDirectionalLightColor[1], mDirectionalLightColor[2], mDirectionalLightColor[3]);
//	lightData.inverse_light_dir_world_space = XMFLOAT4(-mDirectionalLightDir[0], -mDirectionalLightDir[1], -mDirectionalLightDir[2], mDirectionalLightDir[3]);
//	lightData.LightIntensity = mDirectionalLightIntensity;
//
//	memcpy(mLightsInfoCB->Map(), &lightData, sizeof(lightData));
//}

void DirectLightingVoxelGIandAOrenderer::UpdateCamera(DXRSTimer const& timer, Camera& camera)
{
	//XMVECTOR cameraTranslate = camera.GetTranslation();
	mCameraView = camera.GetViewMatrix();
	mCameraProjection = camera.GetProjectionMatrix();
	DirectX::XMStoreFloat3(&mCameraEye, camera.GetTranslation());
}

void DirectLightingVoxelGIandAOrenderer::OnWindowSizeChanged(Camera& camera)
{
	if (!_device_resources->WindowSizeChanged())
		return;

	/*auto size = _device_resources->GetOutputSize();
	camera.SetAspectRatio(float(size.right) / float(size.bottom));*/

	;
		

	float aspectRatio = _device_resources->mAppWindow->Bounds.Width / _device_resources->mAppWindow->Bounds.Height;
	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		camera.SetFoV(60.0f * 2.0f);
	}
	else
	{
		camera.SetFoV(60.0f);
	}

	camera.SetProjection(camera.GetFoV(), aspectRatio, 0.01f, 500.0f);

	//scene_renderer->CreateWindowSizeDependentResources(camera);
}

void DirectLightingVoxelGIandAOrenderer::ThrowFailedErrorBlob(ID3DBlob* blob)
{
	std::string message = "";
	if (blob)
		message.append((char*)blob->GetBufferPointer());

	throw std::runtime_error(message.c_str());
}

void DirectLightingVoxelGIandAOrenderer::InitGbuffer(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager)
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

	// root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	mGbufferRS.Reset(1, 1);
	mGbufferRS.InitStaticSampler(0, sampler, D3D12_SHADER_VISIBILITY_PIXEL);
	mGbufferRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
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

	//create constant buffer for pass
	DX12Buffer::Description cbDesc;
	cbDesc.mElementSize = sizeof(GBufferCBData);
	cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
	cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

	mGbufferCB = new DX12Buffer(_device_resources->GetD3DDevice(), descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"GBuffer CB");
}
void DirectLightingVoxelGIandAOrenderer::RenderGbuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DX12DescriptorHeapGPU* gpuDescriptorHeap, std::vector<Model*>& scene)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _device_resources->GetOutputSize().right, _device_resources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _device_resources->GetOutputSize().right, _device_resources->GetOutputSize().bottom);

	//PIXBeginEvent(commandList, 0, "GBuffer");
	{
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &rect);

		commandList->SetPipelineState(mGbufferPSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mGbufferRS.GetSignature());

		// Transition buffers to rendertarget outputs
		_device_resources->ResourceBarriersBegin(mBarriers);
		mGbufferRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mGbufferRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mGbufferRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		_device_resources->ResourceBarriersEnd(mBarriers, commandList);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] =
		{
			mGbufferRTs[0]->GetRTV().GetCPUHandle(),
			mGbufferRTs[1]->GetRTV().GetCPUHandle(),
			mGbufferRTs[2]->GetRTV().GetCPUHandle(),
		};

		commandList->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, FALSE, &_device_resources->GetDepthStencilView());
		commandList->ClearRenderTargetView(rtvHandles[0], clearColorBlack, 0, nullptr);
		commandList->ClearRenderTargetView(rtvHandles[1], clearColorBlack, 0, nullptr);
		commandList->ClearRenderTargetView(rtvHandles[2], clearColorBlack, 0, nullptr);

		DX12DescriptorHandle cbvHandle;
		
		for (auto& model : scene) 
		{
			if (!mUseDynamicObjects && model->GetIsDynamic())
			{
				continue;
			}
				
			cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mGbufferCB->GetCBV());
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, model->GetCB()->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			model->Render(commandList);
		}
	}
	//PIXEndEvent(commandList);
}

void DirectLightingVoxelGIandAOrenderer::InitShadowMapping(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager)
// create resources for shadow mapping
{
	mShadowDepth = new DXRSDepthBuffer(device, descriptorManager, SHADOWMAP_SIZE, SHADOWMAP_SIZE, DXGI_FORMAT_D32_FLOAT);

	//create root signature
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
void DirectLightingVoxelGIandAOrenderer::RenderShadowMapping(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DX12DescriptorHeapGPU* gpuDescriptorHeap, std::vector<Model*>& scene, DirectionalLight& directionalLight)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _device_resources->GetOutputSize().right, _device_resources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _device_resources->GetOutputSize().right, _device_resources->GetOutputSize().bottom);

	//PIXBeginEvent(commandList, 0, "Shadows");
	{
		CD3DX12_VIEWPORT shadowMapViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, mShadowDepth->GetWidth(), mShadowDepth->GetHeight());
		CD3DX12_RECT shadowRect = CD3DX12_RECT(0.0f, 0.0f, mShadowDepth->GetWidth(), mShadowDepth->GetHeight());
		commandList->RSSetViewports(1, &shadowMapViewport);
		commandList->RSSetScissorRects(1, &shadowRect);

		commandList->SetPipelineState(mShadowMappingPSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mShadowMappingRS.GetSignature());

		_device_resources->ResourceBarriersBegin(mBarriers);
		mShadowDepth->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		_device_resources->ResourceBarriersEnd(mBarriers, commandList);

		commandList->OMSetRenderTargets(0, nullptr, FALSE, &mShadowDepth->GetDSV().GetCPUHandle());
		commandList->ClearDepthStencilView(mShadowDepth->GetDSV().GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		DX12DescriptorHandle cbvHandle;
		cbvHandle = gpuDescriptorHeap->GetHandleBlock(scene.size() + 1);
		gpuDescriptorHeap->AddToHandle(device, cbvHandle, directionalLight.p_constant_buffer->GetCBV());
		commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle = cbvHandle.GetGPUHandle();
		for (auto& model : scene) 
		{
			if (!mUseDynamicObjects && model->GetIsDynamic())
			{
				continue;
			}

			gpuDescriptorHeap->AddToHandle(device, cbvHandle, model->GetCB()->GetCBV());
			gpuDescriptorHandle.ptr += gpuDescriptorHeap->GetDescriptorSize();
			commandList->SetGraphicsRootDescriptorTable(1, gpuDescriptorHandle);
			model->Render(commandList);
		}

		//reset back
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &rect);
	}
	//PIXEndEvent(commandList);
}

void DirectLightingVoxelGIandAOrenderer::InitVoxelConeTracing(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager)
{
	// voxelization
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
		mVCTVoxelization3DRT_CopyForAsync = new DXRSRenderTarget(device, descriptorManager, VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE, format, flags, L"Voxelization Scene Data 3D Copy", VCT_SCENE_VOLUME_SIZE);

		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		mVCTVoxelizationRS.Reset(4, 1);
		mVCTVoxelizationRS.InitStaticSampler(0, shadowSampler, D3D12_SHADER_VISIBILITY_PIXEL);
		mVCTVoxelizationRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTVoxelizationRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTVoxelizationRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTVoxelizationRS[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
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
		delete[] pPixelShaderByteCode;
		delete[] pGeometryShadeByteCode;

		//create constant buffer for pass
		DX12Buffer::Description cbDesc;
		cbDesc.mElementSize = sizeof(VCTVoxelizationCBData);
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;
		mVCTVoxelizationCB = new DX12Buffer(device, descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"VCT Voxelization Pass CB");

		VCTVoxelizationCBData data = {};
		float scale = 1.0f;
		data.WorldVoxelCube = XMMatrixScaling(scale, scale, scale);
		data.ViewProjection = mCameraView * mCameraProjection;
		data.ShadowViewProjection = mLightViewProjection;
		data.WorldVoxelScale = mWorldVoxelScale;
		memcpy(mVCTVoxelizationCB->Map(), &data, sizeof(data));
	}

	//debug 
	{
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		mVCTVoxelizationDebugRT = new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, format, flags, L"Voxelization Debug RT");

		//create root signature
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

	// aniso mipmapping prepare
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
		cbDesc.mElementSize = sizeof(VCTAnisoMipmappingCBData);
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

		mVCTAnisoMipmappingCB = new DX12Buffer(_device_resources->GetD3DDevice(), descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"VCT aniso mip mapping CB");
	}

	// aniso mipmapping main
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
		cbDesc.mElementSize = sizeof(VCTAnisoMipmappingCBData);
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(_device_resources->GetD3DDevice(), descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"VCT aniso mip mapping main mip 0 CB"));
		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(_device_resources->GetD3DDevice(), descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"VCT aniso mip mapping main mip 1 CB"));
		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(_device_resources->GetD3DDevice(), descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"VCT aniso mip mapping main mip 2 CB"));
		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(_device_resources->GetD3DDevice(), descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"VCT aniso mip mapping main mip 3 CB"));
		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(_device_resources->GetD3DDevice(), descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"VCT aniso mip mapping main mip 4 CB"));
		mVCTAnisoMipmappingMainCB.push_back(new DX12Buffer(_device_resources->GetD3DDevice(), descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"VCT aniso mip mapping main mip 5 CB"));

	}

	// main 
	{
		DX12Buffer::Description cbDesc;
		cbDesc.mElementSize = sizeof(VCTMainCBData);
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

		mVCTMainCB = new DX12Buffer(_device_resources->GetD3DDevice(), descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"VCT main CB");
		
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		mVCTMainRT = new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH * mVCTRTRatio, MAX_SCREEN_HEIGHT * mVCTRTRatio, format, flags, L"VCT Final Output", -1, 1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

		// PS
		{
			mVCTMainRS.Reset(2, 1);
			mVCTMainRS.InitStaticSampler(0, mBilinearSampler, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10, D3D12_SHADER_VISIBILITY_ALL);
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
			mVCTMainRS_Compute.Reset(3, 1);
			mVCTMainRS_Compute.InitStaticSampler(0, mBilinearSampler, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS_Compute[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS_Compute[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10, D3D12_SHADER_VISIBILITY_ALL);
			mVCTMainRS_Compute[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
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

	// upsample and blur
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

		mVCTMainUpsampleAndBlurRS.Reset(3, 1);
		mVCTMainUpsampleAndBlurRS.InitStaticSampler(0, mBilinearSampler, D3D12_SHADER_VISIBILITY_ALL);
		mVCTMainUpsampleAndBlurRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTMainUpsampleAndBlurRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		mVCTMainUpsampleAndBlurRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
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
void DirectLightingVoxelGIandAOrenderer::RenderVoxelConeTracing(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DX12DescriptorHeapGPU* gpuDescriptorHeap, std::vector<Model*>& scene, RenderQueue aQueue)
{
	if (!mUseVCT)
	{
		return;
	}

	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _device_resources->GetOutputSize().right, _device_resources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _device_resources->GetOutputSize().right, _device_resources->GetOutputSize().bottom);

	if (aQueue == GRAPHICS_QUEUE) 
	{
		//PIXBeginEvent(commandList, 0, "VCT Voxelization");
		{
			
			commandList->RSSetViewports(1, &viewport_voxel_cone_tracing_voxelization);
			commandList->RSSetScissorRects(1, &scissor_rect_voxel_cone_tracing_voxelization);
			commandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

			commandList->SetPipelineState(mVCTVoxelizationPSO.GetPipelineStateObject());
			commandList->SetGraphicsRootSignature(mVCTVoxelizationRS.GetSignature());

			_device_resources->ResourceBarriersBegin(mBarriers);
			mVCTVoxelization3DRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			_device_resources->ResourceBarriersEnd(mBarriers, commandList);

			
			DX12DescriptorHandle uavHandle;
			DX12DescriptorHandle srvHandle;
			uavHandle = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTVoxelization3DRT->GetUAV());
			
			commandList->ClearUnorderedAccessViewFloat(uavHandle.GetGPUHandle(), mVCTVoxelization3DRT->GetUAV().GetCPUHandle(), mVCTVoxelization3DRT->GetResource(), clearColorBlack, 0, nullptr);

			srvHandle = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(device, srvHandle, mShadowDepth->GetSRV());

			DX12DescriptorHandle cbvHandle01 = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle01, mVCTVoxelizationCB->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle01.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(2, srvHandle.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(3, uavHandle.GetGPUHandle());
			for (auto& model : scene) 
			{
				if (!mUseDynamicObjects && model->GetIsDynamic())
				{
					continue;
				}

				DX12DescriptorHandle cbvHandle02 = gpuDescriptorHeap->GetHandleBlock(1);
				gpuDescriptorHeap->AddToHandle(device, cbvHandle02, model->GetCB()->GetCBV());
				commandList->SetGraphicsRootDescriptorTable(1, cbvHandle02.GetGPUHandle());

				model->Render(commandList);
			}

			// reset back
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &rect);
		}
		//PIXEndEvent(commandList);

		if (mVCTRenderDebug) 
		{
			//PIXBeginEvent(commandList, 0, "VCT Voxelization Debug");
			commandList->SetPipelineState(mVCTVoxelizationDebugPSO.GetPipelineStateObject());
			commandList->SetGraphicsRootSignature(mVCTVoxelizationDebugRS.GetSignature());

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesFinal[] =
			{
				mVCTVoxelizationDebugRT->GetRTV().GetCPUHandle()
			};

			commandList->OMSetRenderTargets(_countof(rtvHandlesFinal), rtvHandlesFinal, FALSE, &_device_resources->GetDepthStencilView());
			commandList->ClearRenderTargetView(rtvHandlesFinal[0], clearColorBlack, 0, nullptr);
			commandList->ClearDepthStencilView(_device_resources->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			_device_resources->ResourceBarriersBegin(mBarriers);
			mVCTVoxelization3DRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			_device_resources->ResourceBarriersEnd(mBarriers, commandList);

			DX12DescriptorHandle cbvHandle;
			DX12DescriptorHandle uavHandle;

			cbvHandle = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mVCTVoxelizationCB->GetCBV());

			uavHandle = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTVoxelization3DRT->GetUAV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(1, uavHandle.GetGPUHandle());

			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
			commandList->DrawInstanced(VCT_SCENE_VOLUME_SIZE * VCT_SCENE_VOLUME_SIZE * VCT_SCENE_VOLUME_SIZE, 1, 0, 0);
			//PIXEndEvent(commandList);
		}
	
	}

	if (aQueue == COMPUTE_QUEUE) 
	{
		//PIXBeginEvent(commandList, 0, "VCT Mipmapping prepare CS");
		{
			commandList->SetPipelineState(mVCTAnisoMipmappingPreparePSO.GetPipelineStateObject());
			commandList->SetComputeRootSignature(mVCTAnisoMipmappingPrepareRS.GetSignature());
		
			_device_resources->ResourceBarriersBegin(mBarriers);
			mVCTAnisoMipmappinPrepare3DRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[3]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[4]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[5]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			_device_resources->ResourceBarriersEnd(mBarriers, commandList);
		
			VCTAnisoMipmappingCBData cbData = {};
			cbData.MipDimension = VCT_SCENE_VOLUME_SIZE >> 1;
			cbData.MipLevel = 0;
			memcpy(mVCTAnisoMipmappingCB->Map(), &cbData, sizeof(cbData));
			DX12DescriptorHandle cbvHandle = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mVCTAnisoMipmappingCB->GetCBV());
		
			DX12DescriptorHandle srvHandle = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTVoxelization3DRT_CopyForAsync->GetSRV());
		
			DX12DescriptorHandle uavHandle = gpuDescriptorHeap->GetHandleBlock(6);
			gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[0]->GetUAV());
			gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[1]->GetUAV());
			gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[2]->GetUAV());
			gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[3]->GetUAV());
			gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[4]->GetUAV());
			gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[5]->GetUAV());
		
			commandList->SetComputeRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(1, srvHandle.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(2, uavHandle.GetGPUHandle());
		
			commandList->Dispatch(DivideByMultiple(static_cast<UINT>(cbData.MipDimension), 8u), DivideByMultiple(static_cast<UINT>(cbData.MipDimension), 8u), DivideByMultiple(static_cast<UINT>(cbData.MipDimension), 8u));
		}
		//PIXEndEvent(commandList);

		//PIXBeginEvent(commandList, 0, "VCT Mipmapping main CS");
		{
			commandList->SetPipelineState(mVCTAnisoMipmappingMainPSO.GetPipelineStateObject());
			commandList->SetComputeRootSignature(mVCTAnisoMipmappingMainRS.GetSignature());

			_device_resources->ResourceBarriersBegin(mBarriers);
			mVCTAnisoMipmappinPrepare3DRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[3]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[4]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinPrepare3DRTs[5]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			mVCTAnisoMipmappinMain3DRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinMain3DRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinMain3DRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinMain3DRTs[3]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinMain3DRTs[4]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mVCTAnisoMipmappinMain3DRTs[5]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			_device_resources->ResourceBarriersEnd(mBarriers, commandList);

			int mipDimension = VCT_SCENE_VOLUME_SIZE >> 1;
			for (int mip = 0; mip < VCT_MIPS; mip++)
			{
				DX12DescriptorHandle cbvHandle = gpuDescriptorHeap->GetHandleBlock(1);
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mVCTAnisoMipmappingMainCB[mip]->GetCBV());

				DX12DescriptorHandle uavHandle = gpuDescriptorHeap->GetHandleBlock(12);
				if (mip == 0) 
				{
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[0]->GetUAV());
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[1]->GetUAV());
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[2]->GetUAV());
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[3]->GetUAV());
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[4]->GetUAV());
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinPrepare3DRTs[5]->GetUAV());
				}
				else 
				{
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[0]->GetUAV(mip - 1));
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[1]->GetUAV(mip - 1));
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[2]->GetUAV(mip - 1));
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[3]->GetUAV(mip - 1));
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[4]->GetUAV(mip - 1));
					gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[5]->GetUAV(mip - 1));
				}

				gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[0]->GetUAV(mip));
				gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[1]->GetUAV(mip));
				gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[2]->GetUAV(mip));
				gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[3]->GetUAV(mip));
				gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[4]->GetUAV(mip));
				gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTAnisoMipmappinMain3DRTs[5]->GetUAV(mip));

				VCTAnisoMipmappingCBData cbData = {};
				cbData.MipDimension = mipDimension;
				cbData.MipLevel = mip;
				memcpy(mVCTAnisoMipmappingMainCB[mip]->Map(), &cbData, sizeof(cbData));
				commandList->SetComputeRootDescriptorTable(0, cbvHandle.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(1, uavHandle.GetGPUHandle());

				commandList->Dispatch(DivideByMultiple(static_cast<UINT>(cbData.MipDimension), 8u), DivideByMultiple(static_cast<UINT>(cbData.MipDimension), 8u), DivideByMultiple(static_cast<UINT>(cbData.MipDimension), 8u));
				mipDimension >>= 1;
			}
		}
		//PIXEndEvent(commandList);

		if (!mVCTUseMainCompute) 
		{
			//PIXBeginEvent(commandList, 0, "VCT Main PS");
			{
				CD3DX12_VIEWPORT vctResViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, mVCTMainRT->GetWidth(), mVCTMainRT->GetHeight());
				CD3DX12_RECT vctRect = CD3DX12_RECT(0.0f, 0.0f, mVCTMainRT->GetWidth(), mVCTMainRT->GetHeight());
				commandList->RSSetViewports(1, &vctResViewport);
				commandList->RSSetScissorRects(1, &vctRect);

				commandList->SetPipelineState(mVCTMainPSO.GetPipelineStateObject());
				commandList->SetGraphicsRootSignature(mVCTMainRS.GetSignature());

				_device_resources->ResourceBarriersBegin(mBarriers);
				mVCTMainRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
				_device_resources->ResourceBarriersEnd(mBarriers, commandList);

				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesFinal[] =
				{
					 mVCTMainRT->GetRTV().GetCPUHandle()
				};

				commandList->OMSetRenderTargets(_countof(rtvHandlesFinal), rtvHandlesFinal, FALSE, nullptr);
				commandList->ClearRenderTargetView(rtvHandlesFinal[0], clearColorBlack, 0, nullptr);

				DX12DescriptorHandle srvHandle = gpuDescriptorHeap->GetHandleBlock(10);
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mGbufferRTs[0]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mGbufferRTs[1]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mGbufferRTs[2]->GetSRV());

				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[0]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[1]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[2]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[3]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[4]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[5]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTVoxelization3DRT->GetSRV());

				DX12DescriptorHandle cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mVCTVoxelizationCB->GetCBV());
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mVCTMainCB->GetCBV());

				commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
				commandList->SetGraphicsRootDescriptorTable(1, srvHandle.GetGPUHandle());

				commandList->IASetVertexBuffers(0, 1, &_device_resources->GetFullscreenQuadBufferView());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				commandList->DrawInstanced(4, 1, 0, 0);

				//reset back
				commandList->RSSetViewports(1, &viewport);
				commandList->RSSetScissorRects(1, &rect);
			}
			//PIXEndEvent(commandList);
		}
		else 
		{
			//PIXBeginEvent(commandList, 0, "VCT Main CS");
			{
				commandList->SetPipelineState(mVCTMainPSO_Compute.GetPipelineStateObject());
				commandList->SetComputeRootSignature(mVCTMainRS_Compute.GetSignature());

				_device_resources->ResourceBarriersBegin(mBarriers);
				mVCTMainRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				_device_resources->ResourceBarriersEnd(mBarriers, commandList);

				DX12DescriptorHandle uavHandle = gpuDescriptorHeap->GetHandleBlock(1);
				gpuDescriptorHeap->AddToHandle(device, uavHandle, mVCTMainRT->GetUAV());

				DX12DescriptorHandle srvHandle = gpuDescriptorHeap->GetHandleBlock(10);
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mGbufferRTs[0]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mGbufferRTs[1]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mGbufferRTs[2]->GetSRV());

				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[0]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[1]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[2]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[3]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[4]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle, mVCTAnisoMipmappinMain3DRTs[5]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandle,  mVCTVoxelization3DRT_CopyForAsync->GetSRV());

				DX12DescriptorHandle cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mVCTVoxelizationCB->GetCBV());
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mVCTMainCB->GetCBV());

				commandList->SetComputeRootDescriptorTable(0, cbvHandle.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(1, srvHandle.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(2, uavHandle.GetGPUHandle());

				commandList->Dispatch(DivideByMultiple(static_cast<UINT>(mVCTMainRT->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(mVCTMainRT->GetHeight()), 8u), 1u);
			}
			//PIXEndEvent(commandList);
		}

		// upsample and blur
		if (mVCTMainRTUseUpsampleAndBlur) 
		{
			//PIXBeginEvent(commandList, 0, "VCT Main RT upsample & blur CS");
			{
				commandList->SetPipelineState(mVCTMainUpsampleAndBlurPSO.GetPipelineStateObject());
				commandList->SetComputeRootSignature(mVCTMainUpsampleAndBlurRS.GetSignature());

				_device_resources->ResourceBarriersBegin(mBarriers);
				mVCTMainUpsampleAndBlurRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				_device_resources->ResourceBarriersEnd(mBarriers, commandList);

				DX12DescriptorHandle srvHandleBlur = gpuDescriptorHeap->GetHandleBlock(1);
				gpuDescriptorHeap->AddToHandle(device, srvHandleBlur, mVCTMainRT->GetSRV());

				DX12DescriptorHandle uavHandleBlur = gpuDescriptorHeap->GetHandleBlock(1);
				gpuDescriptorHeap->AddToHandle(device, uavHandleBlur, mVCTMainUpsampleAndBlurRT->GetUAV());

				DX12DescriptorHandle cbvHandleBlur = gpuDescriptorHeap->GetHandleBlock(1);
				gpuDescriptorHeap->AddToHandle(device, cbvHandleBlur, mGIUpsampleAndBlurBuffer->GetCBV());

				commandList->SetComputeRootDescriptorTable(0, srvHandleBlur.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(1, uavHandleBlur.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(2, cbvHandleBlur.GetGPUHandle());

				commandList->Dispatch(DivideByMultiple(static_cast<UINT>(mVCTMainUpsampleAndBlurRT->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(mVCTMainUpsampleAndBlurRT->GetHeight()), 8u), 1u);
			}
			//PIXEndEvent(commandList);
		}
	}
}

void DirectLightingVoxelGIandAOrenderer::InitLighting(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager)
{
	//RTs
	mLightingRTs.push_back(new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT,
		DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Lighting"));

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

	mLightingRS.Reset(2, 2);
	mLightingRS.InitStaticSampler(0, mBilinearSampler, D3D12_SHADER_VISIBILITY_PIXEL);
	mLightingRS.InitStaticSampler(1, shadowSampler, D3D12_SHADER_VISIBILITY_PIXEL);
	mLightingRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 3, D3D12_SHADER_VISIBILITY_ALL);
	mLightingRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 11, D3D12_SHADER_VISIBILITY_PIXEL);
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
	cbDesc.mElementSize = sizeof(LightingCBData);
	cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
	cbDesc.mDescriptorType = DX12Buffer::DescriptorType::CBV;

	mLightingCB = new DX12Buffer(device, descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"Lighting Pass CB");

	cbDesc.mElementSize = sizeof(LightsInfoCBData);
	mLightsInfoCB = new DX12Buffer(device, descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"Lights Info CB");

	cbDesc.mElementSize = sizeof(IlluminationFlagsCBData);
	mIlluminationFlagsCB = new DX12Buffer(device, descriptorManager, _device_resources->GetCommandListGraphics(), cbDesc, L"Illumination Flags CB");
}
void DirectLightingVoxelGIandAOrenderer::RenderLighting(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DX12DescriptorHeapGPU* gpuDescriptorHeap)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, _device_resources->GetOutputSize().right, _device_resources->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, _device_resources->GetOutputSize().right, _device_resources->GetOutputSize().bottom);

	//PIXBeginEvent(commandList, 0, "Lighting");
	{
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &rect);

		commandList->SetPipelineState(mLightingPSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mLightingRS.GetSignature());

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesLighting[] =
		{
			mLightingRTs[0]->GetRTV().GetCPUHandle()
		};

		_device_resources->ResourceBarriersBegin(mBarriers);
		mLightingRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		_device_resources->ResourceBarriersEnd(mBarriers, commandList);

		commandList->OMSetRenderTargets(_countof(rtvHandlesLighting), rtvHandlesLighting, FALSE, nullptr);
		commandList->ClearRenderTargetView(rtvHandlesLighting[0], clearColorWhite, 0, nullptr);

		DX12DescriptorHandle cbvHandleLighting = gpuDescriptorHeap->GetHandleBlock(3);
		gpuDescriptorHeap->AddToHandle(device, cbvHandleLighting, mLightingCB->GetCBV());
		gpuDescriptorHeap->AddToHandle(device, cbvHandleLighting, mLightsInfoCB->GetCBV());
		gpuDescriptorHeap->AddToHandle(device, cbvHandleLighting, mIlluminationFlagsCB->GetCBV());

		DX12DescriptorHandle srvHandleLighting = gpuDescriptorHeap->GetHandleBlock(11);
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mGbufferRTs[0]->GetSRV());
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mGbufferRTs[1]->GetSRV());
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mGbufferRTs[2]->GetSRV());
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mShadowDepth->GetSRV());

		if (mVCTRenderDebug)
		{
			gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mVCTVoxelizationDebugRT->GetSRV());
		}	
		else if (mVCTMainRTUseUpsampleAndBlur)
		{
			gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mVCTMainUpsampleAndBlurRT->GetSRV());
		}
		else
		{
			gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mVCTMainRT->GetSRV());
		}
			
		commandList->SetGraphicsRootDescriptorTable(0, cbvHandleLighting.GetGPUHandle());
		commandList->SetGraphicsRootDescriptorTable(1, srvHandleLighting.GetGPUHandle());

		commandList->IASetVertexBuffers(0, 1, &_device_resources->GetFullscreenQuadBufferView());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		commandList->DrawInstanced(4, 1, 0, 0);
	}
	//PIXEndEvent(commandList);
}

void DirectLightingVoxelGIandAOrenderer::InitComposite(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager)
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
	mCompositePSO.SetRenderTargetFormat(_device_resources->GetBackBufferFormat(), DXGI_FORMAT_D32_FLOAT);
	mCompositePSO.SetVertexShader(pVertexShaderByteCode, vertexShaderByteCodeLength);
	mCompositePSO.SetPixelShader(pPixelShaderByteCode, pixelShaderByteCodeLength);
	mCompositePSO.Finalize(device);

	delete[] pVertexShaderByteCode;
	delete[] pPixelShaderByteCode;
}
void DirectLightingVoxelGIandAOrenderer::RenderComposite(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DX12DescriptorHeapGPU* gpuDescriptorHeap)
{
	//PIXBeginEvent(commandList, 0, "Composite");
	{
		commandList->SetPipelineState(mCompositePSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mCompositeRS.GetSignature());

		_device_resources->TransitionMainRT(commandList, D3D12_RESOURCE_STATE_PRESENT);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesFinal[] =
		{
			 _device_resources->GetRenderTargetView()
		};
		commandList->OMSetRenderTargets(_countof(rtvHandlesFinal), rtvHandlesFinal, FALSE, nullptr);
		commandList->ClearRenderTargetView(_device_resources->GetRenderTargetView(), Colors::Green, 0, nullptr);
		
		DX12DescriptorHandle srvHandleComposite = gpuDescriptorHeap->GetHandleBlock(1);
		gpuDescriptorHeap->AddToHandle(device, srvHandleComposite, mLightingRTs[0]->GetSRV());
		
		commandList->SetGraphicsRootDescriptorTable(0, srvHandleComposite.GetGPUHandle());
		commandList->IASetVertexBuffers(0, 1, &_device_resources->GetFullscreenQuadBufferView());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		commandList->DrawInstanced(4, 1, 0, 0);
	}
	//PIXEndEvent(commandList);
}