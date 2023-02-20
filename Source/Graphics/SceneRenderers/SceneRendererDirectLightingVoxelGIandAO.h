#pragma once


#include "Graphics\Shaders\ShaderGlobalsCPU.h"
#include "Graphics\DirectX\DX12DeviceResourcesSingleton.h"
#include "Graphics\DirectX\DX12RenderTarget.h"
#include "Graphics\DirectX\DXRSDepthBuffer.h"
#include "Graphics\DirectX\DX12Buffer.h"
#include "Graphics\DirectX\RootSignature.h"
#include "Graphics\DirectX\PipelineStateObject.h"
#include "Graphics\Shaders\ShaderGlobalsCPUGPU.hlsli"

#include "Scene\Camera.h"
#include "Scene\Model.h"
#include "Scene\Lights.h"

#include "Utility\Time\DXRSTimer.h"
#include "Utility\FileSystem\FilePath.h"

#include <wrl.h>




#define SHADOWMAP_SIZE 2048


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
		void OnWindowSizeChanged(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight);
		// So that we can change the voxel grid from outside of the renderer
		void UpdateVoxelConeTracingVoxelizationBuffers(ID3D12Device* _d3dDevice);

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
			UINT32 voxel_grid_res = 256;
			float voxel_grid_extent_world_space = 80.0f;
			float voxel_grid_half_extent_world_space_rcp;
			// For the mip count we don't want to count the 1x1x1 mip since the anisotropic mip chain generation 
			// requires at least a 2x2x2 texture in order to perform calculations
			// We also don't want to count the mip level 0 (full res mip) since the anisotropic mip chain starts from what would
			// be the original 3D textures mip level 1
			UINT32 voxel_grid_anisotropic_mip_count = (UINT32)log2(voxel_grid_res >> 1) - 1;
			float voxel_extent_rcp;
			float voxel_scale;
			DirectX::XMFLOAT3 padding;
		};

		ShaderStructureCPUVoxelizationData shader_structure_cpu_voxelization_data;
		UINT8 shader_structure_cpu_voxelization_data_most_updated_index;
		static const UINT c_aligned_shader_structure_cpu_voxelization_data = (sizeof(ShaderStructureCPUVoxelizationData) + 255) & ~255;
		__declspec(align(16)) struct ShaderStructureCPUAnisotropicMipGenerationData
		{
			UINT32 mip_dimension;
			UINT32 source_mip_level;
			UINT32 result_mip_level;
			float padding;
		};

		ShaderStructureCPUAnisotropicMipGenerationData shader_structure_cpu_vct_anisotropic_mip_generation_data;
		UINT8 shader_structure_cpu_vct_anisotropic_mip_generation_data_most_updated_index;
		static const UINT c_aligned_shader_structure_cpu_vct_anisotropic_mip_generation_data = (sizeof(ShaderStructureCPUAnisotropicMipGenerationData) + 255) & ~255;
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
		void InitGbuffer(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight);
		void UpdateGBuffer(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight);
		void InitShadowMapping(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight);
		void UpdateShadowMappingBuffers(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight);
		void InitVoxelConeTracing(DX12DeviceResourcesSingleton* _deviceResources, ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight);
		void UpdateVoxelConeTracingBuffers(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight);
		// This function 
		
		void InitLighting(DX12DeviceResourcesSingleton* _deviceResources, ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight);
		void UpdateLightingBuffers(ID3D12Device* _d3dDevice, float backBufferWidth, float backBufferHeight);
		void InitComposite(DX12DeviceResourcesSingleton* _deviceResources, ID3D12Device* _d3dDevice);
		
	

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
		std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
		Microsoft::WRL::ComPtr<ID3D12Fence> fence_direct_queue;
		Microsoft::WRL::ComPtr<ID3D12Fence>	fence_compute_queue;
		HANDLE fence_event;
		UINT64 fence_unused_value_direct_queue;
		UINT64 fence_unused_value_compute_queue;
		// GBuffer
		RootSignature root_signature_gbuffer;
		std::vector<DX12RenderTarget*> P_render_targets_gbuffer;
		GraphicsPSO pipeline_state_gbuffer;
		// Voxel Cone Tracing Voxelization
		CD3DX12_VIEWPORT viewport_vct_voxelization;
		CD3DX12_RECT scissor_rect_vct_voxelization;
		DX12RenderTarget* p_render_target_vct_voxelization;
		RootSignature root_signature_vct_voxelization;
		GraphicsPSO pipeline_state_vct_voxelization;
		RootSignature root_signature_vct_voxelization_anisotropic_mip_generation_level_zero;
		ComputePSO pipeline_state_vct_voxelization_anisotropic_mip_generation_level_zero;
		RootSignature root_signature_vct_voxelization_anisotropic_mip_generation;
		ComputePSO pipeline_state_vct_voxelization_anisotropic_mip_generation;
		std::vector<DX12RenderTarget*> P_render_targets_vct_voxelization_anisotropic_mip_generation;
		// Voxel Cone Tracing Main
		RootSignature root_signature_vct_main;
		RootSignature root_signature_vct_main_upsample_and_blur;
		ComputePSO pipeline_state_vct_main;
		ComputePSO pipeline_state_vct_main_upsample_and_blur;
		DX12RenderTarget* p_render_target_vct_main;
		DX12RenderTarget* p_render_target_vct_main_upsample_and_blur;
		// Voxel Cone Tracing Debug
		RootSignature root_signature_vct_debug;
		GraphicsPSO pipeline_state_vct_debug;
		// Composite
		RootSignature root_signature_composite;
		GraphicsPSO pipeline_state_composite;
		// Lighting
		RootSignature root_signature_lighting;
		DX12RenderTarget* p_render_target_lighting;
		GraphicsPSO pipeline_state_lighting;
		// Shadows
		GraphicsPSO pipeline_state_shadow_mapping;
		DXRSDepthBuffer* p_depth_buffer_shadow_mapping;
		RootSignature root_signature_shadow_mapping;
		// Constant Buffers
		DX12Buffer* p_constant_buffer_voxelization;
		DX12Buffer* p_constant_buffer_vct_main;
		std::vector<DX12Buffer*> P_constant_buffers_vct_anisotropic_mip_generation;
		DX12Buffer* p_constant_buffer_lighting;
		DX12Buffer* p_constant_buffer_illumination_flags;

		bool vct_render_debug = false;
		float mVCTRTRatio = 0.5f; // from MAX_SCREEN_WIDTH/HEIGHT
		bool use_vct_main_upsample_and_blur = true;

		D3D12_DEPTH_STENCIL_DESC depth_state_read_write;
		D3D12_DEPTH_STENCIL_DESC depth_state_disabled;
		D3D12_BLEND_DESC blend_state_default;
		D3D12_RASTERIZER_DESC rasterizer_state_default;
		D3D12_RASTERIZER_DESC rasterizer_state_no_cull_no_depth;
		D3D12_RASTERIZER_DESC rasterizer_state_shadow_mapping;
		D3D12_SAMPLER_DESC sampler_bilinear;

};
