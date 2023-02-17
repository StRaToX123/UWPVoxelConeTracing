#pragma once


#include "Graphics\Shaders\ShaderGlobalsCPU.h"
#include "Graphics\DirectX\DX12DeviceResourcesSingleton.h"
#include "Graphics\DirectX\DXRSRenderTarget.h"
#include "Graphics\DirectX\DXRSDepthBuffer.h"
#include "Graphics\DirectX\DX12Buffer.h"
#include "Graphics\DirectX\RootSignature.h"
#include "Graphics\DirectX\PipelineStateObject.h"
#include "Graphics\DirectX\RaytracingPipelineGenerator.h"
#include "Graphics\DirectX\ShaderBindingTableGenerator.h"

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

class SceneRendererDirectLightingVoxelGIandAO
{
	enum RenderQueue 
	{
		GRAPHICS_QUEUE,
		COMPUTE_QUEUE
	};

	public:
		SceneRendererDirectLightingVoxelGIandAO();
		~SceneRendererDirectLightingVoxelGIandAO();

		void Initialize(Windows::UI::Core::CoreWindow^ coreWindow);
		void Render(std::vector<Model*>& scene, 
			DirectionalLight& directionalLight, 
			DXRSTimer& mTimer, 
			Camera& camera, 
			D3D12_VERTEX_BUFFER_VIEW& fullScreenQuadVertexBufferView,
			ID3D12GraphicsCommandList* _commandListDirect, 
			ID3D12GraphicsCommandList* _commandListCompute,
			ID3D12CommandAllocator* _commandAllocatorDirect);

		enum UpdatableBuffers
		{
			VCT_MAIN_DATA_BUFFER,
			ILLUMINATION_FLAGS_DATA_BUFFER,
			VOXELIZATION_DATA_BUFFER,
			LIGHTING_DATA_BUFFER
		};
		void UpdateBuffers(UpdatableBuffers whichBufferToUpdate);
		__declspec(align(16)) struct ShaderSTructureCPULightingData
		{
			DirectX::XMFLOAT2 shadow_texel_size;
			DirectX::XMFLOAT2 padding;
		};

		ShaderSTructureCPULightingData shader_structure_cpu_lighting_data;
		UINT8 shader_structure_cpu_lighting_data_most_updated_index;
		static const UINT c_aligned_shader_structure_cpu_lighting_data = (sizeof(ShaderSTructureCPULightingData) + 255) & ~255;
		__declspec(align(16)) struct ShaderStructureCPUIlluminationFlagsData
		{
			int use_direct = 1;
			int use_shadows = 1;
			int use_vct = 1;
			int use_vct_debug = 0;
			float vct_gi_power = 1.0f;
			int show_only_ao = 0;
			DirectX::XMFLOAT2 padding;
		};

		ShaderStructureCPUIlluminationFlagsData shader_structure_cpu_illumination_flags_data;
		UINT8 shader_structure_cpu_illumination_flags_most_updated_index;
		static const UINT c_aligned_shader_structure_cpu_illumination_flags_data = (sizeof(ShaderStructureCPUIlluminationFlagsData) + 255) & ~255;
		__declspec(align(16)) struct ShaderStructureCPUVoxelizationData
		{
			DirectX::XMFLOAT3 voxel_grid_top_left_back_point_world_space;
			UINT32 voxel_grid_res;
			float voxel_grid_extent_world_space = 256.0f;
			float voxel_grid_half_extent_world_space_rcp;
			float voxel_extent_rcp;
			float voxel_scale;
		};

		ShaderStructureCPUVoxelizationData shader_structure_cpu_voxelization_data;
		UINT8 shader_structure_cpu_voxelization_data_most_updated_index;
		static const UINT c_aligned_shader_structure_cpu_voxelization_data = (sizeof(ShaderStructureCPUVoxelizationData) + 255) & ~255;
		__declspec(align(16)) struct ShaderStructureCPUMipMappingData
		{
			int mip_dimension;
			int mip_level;
			DirectX::XMFLOAT2 padding;
		};

		ShaderStructureCPUMipMappingData shader_structure_cpu_mip_mapping_data;
		static const UINT c_aligned_shader_structure_cpu_mip_mapping_data = (sizeof(ShaderStructureCPUMipMappingData) + 255) & ~255;
		__declspec(align(16)) struct ShaderStructureCPUVCTMainData
		{
			DirectX::XMFLOAT2 upsample_ratio;
			float indirect_diffuse_strength = 1.0f;
			float indirect_specular_strength = 1.0f;
			float max_cone_trace_distance = 100.0f;
			float ao_falloff = 2.0f;
			float sampling_factor = 0.5f;
			float voxel_sample_offset = 0.0f;
		};

		ShaderStructureCPUVCTMainData shader_structure_cpu_vct_main_data;
		UINT8 shader_structure_cpu_vct_main_data_most_updated_index;
		static const UINT c_aligned_shader_structure_cpu_vct_main_data = (sizeof(ShaderStructureCPUVCTMainData) + 255) & ~255;
	private:
		void InitGbuffer(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager);
		void InitShadowMapping(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager);
		void InitVoxelConeTracing(DX12DeviceResourcesSingleton* _deviceResources, ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager);
		void InitLighting(DX12DeviceResourcesSingleton* _deviceResources, ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager);
		void InitComposite(DX12DeviceResourcesSingleton* _deviceResources, ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager);

	

		void RenderGbuffer(DX12DeviceResourcesSingleton* _deviceResources,
			ID3D12Device* device,
			ID3D12GraphicsCommandList* commandList,
			DX12DescriptorHeapGPU* gpuDescriptorHeap,
			std::vector<Model*>& scene,
			DX12DescriptorHandleBlock& modelDataDescriptorHandleBlock,
			DX12DescriptorHandleBlock& cameraDataDescriptorHandleBlock);
		void RenderShadowMapping(DX12DeviceResourcesSingleton* _deviceResources,
			ID3D12Device* device,
			ID3D12GraphicsCommandList* commandList,
			DX12DescriptorHeapGPU* gpuDescriptorHeap,
			std::vector<Model*>& scene,
			DX12DescriptorHandleBlock& modelDataDescriptorHandleBlock,
			DX12DescriptorHandleBlock& directionalLightDataDescriptorHeapBlock);
		void RenderVoxelConeTracing(DX12DeviceResourcesSingleton* _deviceResources,
			ID3D12Device* device,
			ID3D12GraphicsCommandList* commandList,
			DX12DescriptorHeapGPU* gpuDescriptorHeap,
			std::vector<Model*>& scene,
			DX12DescriptorHandleBlock& modelDataDescriptorHandleBlock,
			DX12DescriptorHandleBlock& directionalLightDescriptorHandleBlock,
			DX12DescriptorHandleBlock& cameraDataDescriptorHandleBlock,
			DX12DescriptorHandleBlock& shadowDepthDescriptorHandleBlock,
			RenderQueue aQueue = GRAPHICS_QUEUE);
		void RenderLighting(DX12DeviceResourcesSingleton* _deviceResources, 
			ID3D12Device* device, 
			ID3D12GraphicsCommandList* commandList,
			DX12DescriptorHeapGPU* gpuDescriptorHeap,
			DX12DescriptorHandleBlock& directionalLightDescriptorHandleBlock,
			DX12DescriptorHandleBlock& cameraDataDescriptorHandleBlock,
			DX12DescriptorHandleBlock& shadowDepthDescriptorHandleBlock,
			D3D12_VERTEX_BUFFER_VIEW& fullScreenQuadVertexBufferView);
		void RenderComposite(DX12DeviceResourcesSingleton* _deviceResources, 
			ID3D12Device* device, 
			ID3D12GraphicsCommandList* commandList, 
			DX12DescriptorHeapGPU* gpuDescriptorHeap,
			D3D12_VERTEX_BUFFER_VIEW& fullScreenQuadVertexBufferView);

		void ThrowFailedErrorBlob(ID3DBlob* blob);

		DX12DescriptorHeapManager descriptor_heap_manager;
		std::vector<CD3DX12_RESOURCE_BARRIER> mBarriers;
		Microsoft::WRL::ComPtr<ID3D12Fence> fence_direct_queue;
		Microsoft::WRL::ComPtr<ID3D12Fence>	fence_compute_queue;
		HANDLE fence_event;
		UINT64 fence_unused_value_direct_queue;
		UINT64 fence_unused_value_compute_queue;

		// Gbuffer
		RootSignature root_signature_gbuffer;
		std::vector<DXRSRenderTarget*> render_targets_gbuffer;
		GraphicsPSO pipeline_state_gbuffer;
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
		

		DX12Buffer* p_constant_buffer_voxelization;
		DX12Buffer* p_constant_buffer_aniso_mipmapping;
		DX12Buffer* p_constant_buffer_vct_main;
		std::vector<DX12Buffer*> p_constant_buffers_vct_mipmapping_main;
		bool mVCTRenderDebug = false;
		float mVCTRTRatio = 0.5f; // from MAX_SCREEN_WIDTH/HEIGHT
		bool mVCTUseMainCompute = true;
		bool mVCTMainRTUseUpsampleAndBlur = true;

		// Composite
		RootSignature mCompositeRS;
		GraphicsPSO mCompositePSO;

		// UI
		ComPtr<ID3D12DescriptorHeap> mUIDescriptorHeap;

		// Lighting
		RootSignature mLightingRS;
		DXRSRenderTarget* mLightingRT;
		GraphicsPSO mLightingPSO;
		

		DX12Buffer* mLightingCB;
		DX12Buffer* mIlluminationFlagsCB;

		// Shadows
		GraphicsPSO mShadowMappingPSO;
		DXRSDepthBuffer* mShadowDepth;
		XMMATRIX mLightViewProjection;
		XMMATRIX mLightView;
		XMMATRIX mLightProj;
		RootSignature mShadowMappingRS;
		float mShadowIntensity = 0.5f;

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
