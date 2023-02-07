#pragma once


#include <d3dcompiler.h> // <------------------ remove when deprecated dxcompiler

#include "Graphics\Shaders\ShaderGlobalsCPU.h"
#include "Graphics\DirectX\DX12DeviceResources.h"
#include "Graphics\DirectX\DXRSRenderTarget.h"
#include "Graphics\DirectX\DXRSDepthBuffer.h"
#include "Graphics\DirectX\DX12Buffer.h"
#include "Graphics\DirectX\RootSignature.h"
#include "Graphics\DirectX\PipelineStateObject.h"
#include "Graphics\DirectX\RaytracingPipelineGenerator.h"
#include "Graphics\DirectX\ShaderBindingTableGenerator.h"

#include "imgui.h"
#include "imgui_impl_UWP.h"
#include "imgui_impl_dx12.h"

#include "Scene\Camera.h"
#include "Scene\Model.h"
#include "Scene\Lights.h"

#include "Utility\Time\DXRSTimer.h"
#include "Utility\FileSystem\FilePath.h"

#include <wrl.h>




#define SHADOWMAP_SIZE 2048
#define RSM_SIZE 2048
#define RSM_SAMPLES_COUNT 512
#define LPV_DIM 32
#define VCT_SCENE_VOLUME_SIZE 256
#define VCT_MIPS 6 // Only 6 mips becasue we don't go lower than 8x8x8, since thats how many thrads are inside a dispatch group
#define LOCKED_CAMERA_VIEWS 3
#define NUM_DYNAMIC_OBJECTS 40

class DirectLightingVoxelGIandAOrenderer
{
	enum RenderQueue 
	{
		GRAPHICS_QUEUE,
		COMPUTE_QUEUE
	};

	public:
		DirectLightingVoxelGIandAOrenderer();
		~DirectLightingVoxelGIandAOrenderer();

		void Initialize(Windows::UI::Core::CoreWindow^ coreWindow, DX12DeviceResources* _deviceResources);
	
		void OnWindowSizeChanged(Camera& camera);
		void Render(std::vector<Model*>& scene, DirectionalLight& directionalLight, DXRSTimer& mTimer, Camera& camera);
	private:
		void Clear(ID3D12GraphicsCommandList* cmdList);
		void Update(DXRSTimer const& timer, Camera& camera);
		void UpdateTransforms(DXRSTimer const& timer);
		void UpdateBuffers(DXRSTimer const& timer);
		void UpdateLights(DXRSTimer const& timer);
		void UpdateCamera(DXRSTimer const& timer, Camera& camera);
		void UpdateImGui();
	
		void InitGbuffer(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager);
		void InitShadowMapping(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager);
		void InitVoxelConeTracing(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager);
		void InitLighting(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager);
		void InitComposite(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager);

	

		void RenderGbuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DX12DescriptorHeapGPU* gpuDescriptorHeap, std::vector<Model*>& scene);
		void RenderShadowMapping(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DX12DescriptorHeapGPU* gpuDescriptorHeap, std::vector<Model*>& scene, DirectionalLight& directionalLight);
		void RenderVoxelConeTracing(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DX12DescriptorHeapGPU* gpuDescriptorHeap, std::vector<Model*>& scene, RenderQueue aQueue = GRAPHICS_QUEUE);
		void RenderLighting(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DX12DescriptorHeapGPU* gpuDescriptorHeap);
		void RenderComposite(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DX12DescriptorHeapGPU* gpuDescriptorHeap);

		void ThrowFailedErrorBlob(ID3DBlob* blob);

	
		DX12DeviceResources* _device_resources;
		DX12DescriptorHeapManager descriptor_heap_manager;
		std::vector<CD3DX12_RESOURCE_BARRIER> mBarriers;
	

		U_PTR<GraphicsMemory> mGraphicsMemory;
		U_PTR<CommonStates> mStates;

		// Gbuffer
		RootSignature mGbufferRS;
		std::vector<DXRSRenderTarget*> mGbufferRTs;
		GraphicsPSO mGbufferPSO;
		DXRSDepthBuffer* mDepthStencil;
		__declspec(align(16)) struct GBufferCBData
		{
			XMMATRIX ViewProjection;
		};

		DX12Buffer* mGbufferCB;

		// Voxel Cone Tracing
		CD3DX12_VIEWPORT viewport_voxel_cone_tracing_voxelization;
		CD3DX12_RECT scissor_rect_voxel_cone_tracing_voxelization;
		RootSignature mVCTVoxelizationRS;
		RootSignature mVCTMainRS;
		RootSignature mVCTMainRS_Compute;
		RootSignature mVCTMainUpsampleAndBlurRS;
		RootSignature mVCTAnisoMipmappingPrepareRS;
		RootSignature mVCTAnisoMipmappingMainRS;
		GraphicsPSO mVCTVoxelizationPSO;
		GraphicsPSO mVCTMainPSO;
		ComputePSO mVCTMainPSO_Compute;
		ComputePSO mVCTAnisoMipmappingPreparePSO;
		ComputePSO mVCTAnisoMipmappingMainPSO;
		ComputePSO mVCTMainUpsampleAndBlurPSO;
		RootSignature mVCTVoxelizationDebugRS;
		GraphicsPSO mVCTVoxelizationDebugPSO;
		DXRSRenderTarget* mVCTVoxelization3DRT;
		DXRSRenderTarget* mVCTVoxelization3DRT_CopyForAsync;
		DXRSRenderTarget* mVCTVoxelizationDebugRT;
		DXRSRenderTarget* mVCTMainRT;
		DXRSRenderTarget* mVCTMainUpsampleAndBlurRT;
		std::vector<DXRSRenderTarget*> mVCTAnisoMipmappinPrepare3DRTs;
		std::vector<DXRSRenderTarget*> mVCTAnisoMipmappinMain3DRTs;
		__declspec(align(16)) struct VCTVoxelizationCBData
		{
			XMMATRIX WorldVoxelCube;
			XMMATRIX ViewProjection;
			XMMATRIX ShadowViewProjection;
			float WorldVoxelScale;
		};
		__declspec(align(16)) struct VCTAnisoMipmappingCBData
		{
			int MipDimension;
			int MipLevel;
		};
		__declspec(align(16)) struct VCTMainCBData
		{
			XMFLOAT4 CameraPos;
			XMFLOAT2 UpsampleRatio;
			float IndirectDiffuseStrength;
			float IndirectSpecularStrength;
			float MaxConeTraceDistance;
			float AOFalloff;
			float SamplingFactor;
			float VoxelSampleOffset;
		};
		DX12Buffer* mVCTVoxelizationCB;
		DX12Buffer* mVCTAnisoMipmappingCB;
		DX12Buffer* mVCTMainCB;
		std::vector<DX12Buffer*> mVCTAnisoMipmappingMainCB;
		bool mVCTRenderDebug = false;
		float mWorldVoxelScale = VCT_SCENE_VOLUME_SIZE * 0.5f;
		float mVCTIndirectDiffuseStrength = 1.0f;
		float mVCTIndirectSpecularStrength = 1.0f;
		float mVCTMaxConeTraceDistance = 100.0f;
		float mVCTAoFalloff = 2.0f;
		float mVCTSamplingFactor = 0.5f;
		float mVCTVoxelSampleOffset = 0.0f;
		float mVCTRTRatio = 0.5f; // from MAX_SCREEN_WIDTH/HEIGHT
		bool mVCTUseMainCompute = true;
		bool mVCTMainRTUseUpsampleAndBlur = true;
		float mVCTGIPower = 1.0f;

		// Composite
		RootSignature mCompositeRS;
		GraphicsPSO mCompositePSO;

		// UI
		ComPtr<ID3D12DescriptorHeap> mUIDescriptorHeap;

		// Lighting
		RootSignature mLightingRS;
		std::vector<DXRSRenderTarget*> mLightingRTs;
		GraphicsPSO mLightingPSO;
		__declspec(align(16)) struct LightingCBData
		{
			XMMATRIX InvViewProjection;
			XMMATRIX ShadowViewProjection;
			XMFLOAT4 CameraPos;
			XMFLOAT4 ScreenSize;
			XMFLOAT2 ShadowTexelSize;
			float ShadowIntensity;
			float pad;
		};

		__declspec(align(16)) struct IlluminationFlagsCBData
		{
			int useDirect;
			int useShadows;
			int useVCT;
			int useVCTDebug;
			float vctGIPower;
			int showOnlyAO;
		};

		DX12Buffer* mLightingCB;
		DX12Buffer* mIlluminationFlagsCB;
		
		bool mUseDirectLight = true;
		bool mUseShadows = true;
		bool mUseRSM = false;
		bool mUseLPV = false;
		bool mUseVCT = true;
		bool mShowOnlyAO = false;

		// Shadows
		GraphicsPSO mShadowMappingPSO;
		DXRSDepthBuffer* mShadowDepth;
		XMMATRIX mLightViewProjection;
		XMMATRIX mLightView;
		XMMATRIX mLightProj;
		RootSignature mShadowMappingRS;
		float mShadowIntensity = 0.5f;

		// Upsample & Blur
		__declspec(align(16)) struct UpsampleAndBlurBuffer
		{
			bool Upsample;
		};
		DX12Buffer* mGIUpsampleAndBlurBuffer;
		DX12Buffer* mDXRBlurBuffer;

		D3D12_DEPTH_STENCIL_DESC mDepthStateRW;
		D3D12_DEPTH_STENCIL_DESC mDepthStateRead;
		D3D12_DEPTH_STENCIL_DESC mDepthStateDisabled;
		D3D12_BLEND_DESC mBlendState;
		D3D12_BLEND_DESC mBlendStateLPVPropagation;
		D3D12_RASTERIZER_DESC mRasterizerState;
		D3D12_RASTERIZER_DESC mRasterizerStateNoCullNoDepth;
		D3D12_RASTERIZER_DESC mRasterizerStateShadow;
		D3D12_SAMPLER_DESC mBilinearSampler;

		XMFLOAT3 mCameraEye{ 0.0f, 0.0f, 0.0f };
		XMMATRIX mCameraView;
		XMMATRIX mCameraProjection;
		XMFLOAT2 mLastMousePosition;
		float mFOV = 60.0f;
		bool mLockCamera = false;
		XMVECTOR mLockedCameraPositions[LOCKED_CAMERA_VIEWS] = {
			{2.88f, 16.8f, -0.6f},
			{-23.3f, 10.7f, 25.6f},
			{0.0f, 7.0f, 33.0f}
		};
		XMMATRIX mLockedCameraRotMatrices[LOCKED_CAMERA_VIEWS] = {
			XMMatrixRotationX(-XMConvertToRadians(20.0f)) * XMMatrixRotationY(-XMConvertToRadians(40.0f)),
			XMMatrixRotationX(-XMConvertToRadians(10.0f)) * XMMatrixRotationY(-XMConvertToRadians(30.0f)),
			XMMatrixIdentity()
		};

		bool mUseAsyncCompute = true;
		bool mUseDynamicObjects = true;
		bool mStopDynamicObjects = false;
};
