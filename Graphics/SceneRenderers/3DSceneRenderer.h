#pragma once

#include <fstream>
#include <vector>
#include <unordered_map>
#include <DirectXColors.h>
#include <DirectXTex.h>
#include "Graphics/DeviceResources/DeviceResources.h"
#include "Graphics/Shaders/ShaderGlobalsCPUGPU.hlsli"
#include "Utility/Debugging/DebugMessage.h"
#include "Utility/Memory/Allocators/PreAllocator.h"
#include "Scene/Camera.h"
#include "Scene/Mesh.h"
#include "Scene/Light.h"




//using namespace VoxelConeTracing;
using namespace Concurrency;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace std;

typedef UINT ModelTransformMatrixBufferIndex;


// This sample renderer instantiates a basic rendering pipeline.
class SceneRenderer3D
{
	public:
		SceneRenderer3D(const std::shared_ptr<DeviceResources>& deviceResources, Camera& camera);
		~SceneRenderer3D();
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources(Camera& camera);
		ID3D12GraphicsCommandList* Render(vector<Mesh>& scene, Camera& camera, bool voxelDebugVisualization);
		void SaveState();

		//////////////
		// Voxel GI //
		//////////////
		struct VoxelizedSceneData
		{
			bool enabled = false;
			uint32_t res = 256;
			float voxel_size = 0.0078125f;
			XMFLOAT3 center = XMFLOAT3(0, 0, 0);
			XMFLOAT3 extents = XMFLOAT3(0, 0, 0);
			uint32_t num_cones = 2;
			float ray_step_size = 0.0058593f;
			float max_distance = 5.0f;
			bool secondary_bounce_enabled = false;
			bool reflections_enabled = true;
			bool center_changed_this_frame = true;
			uint32_t mips = 7;
		};

		VoxelizedSceneData                      voxelizer_scene_data;


	private:
		void LoadState();
		void LoadShaderByteCode(const char* shaderPath, _Out_ char*& shaderByteCode, _Out_ int& shaderByteCodeLength);
		// Constant buffers must be aligned to the D3D12_TEXTURE_DATA_PITCH_ALIGNMENT.
		static const UINT c_aligned_view_projection_matrix_constant_buffer = (sizeof(ShaderStructureCPUViewProjectionBuffer) + 255) & ~255;
		
		// Direct3D resources for cube geometry.
		std::shared_ptr<DeviceResources>                       device_resources;
		UINT                                                   previous_frame_index;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>      command_list_direct;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	   command_list_copy_normal_priority;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	   command_list_copy_high_priority;
		Microsoft::WRL::ComPtr<ID3D12Fence>				       fence_command_list_copy_normal_priority_progress;
		Microsoft::WRL::ComPtr<ID3D12Fence>				       fence_command_list_copy_high_priority_progress;
		HANDLE                                                 event_command_list_copy_high_priority_progress;
		UINT64                                                 fence_command_list_copy_normal_priority_progress_latest_unused_value;
		UINT64                                                 fence_command_list_copy_high_priority_progress_latest_unused_value;
		UINT64                                                 fence_per_frame_command_list_copy_high_priority_values[c_frame_count];
		bool                                                   command_allocator_copy_normal_priority_already_reset;
		bool                                                   command_list_copy_normal_priority_requires_reset;
		bool                                                   command_allocator_copy_high_priority_already_reset;
		bool                                                   command_list_copy_high_priority_requires_reset;
		Microsoft::WRL::ComPtr<ID3D12RootSignature>			   default_root_signature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   default_pipeline_state;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		   descriptor_heap_cbv_srv;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		   descriptor_heap_sampler;
		CD3DX12_CPU_DESCRIPTOR_HANDLE                          cbv_srv_cpu_handle;
		UINT										   	       cbv_srv_descriptor_size;
		Microsoft::WRL::ComPtr<ID3D12Resource>			   	   test_texture;
		Microsoft::WRL::ComPtr<ID3D12Resource>				   test_texture_upload;
		D3D12_RECT											   scissor_rect;
		Microsoft::WRL::ComPtr<ID3D12Resource>				   camera_view_projection_constant_buffer;
		ShaderStructureCPUViewProjectionBuffer		       camera_view_projection_constant_buffer_data;
		UINT8*												   camera_view_projection_constant_mapped_buffer;
		vector<Microsoft::WRL::ComPtr<ID3D12Resource>>         model_transform_matrix_upload_buffers;
		vector<XMFLOAT4X4*>                                    model_transform_matrix_mapped_upload_buffers;
		vector<vector<UINT>>                                   model_transform_matrix_buffer_free_slots;
		UINT                                                   free_slots_preallocated_array[MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES];
		vector<UINT>                                           available_model_transform_matrix_buffer;
		
		//////////////
		// Voxel GI //
		//////////////
		struct ShaderStructureCPUVoxelGridData
		{
			uint32_t res = 256;
			float res_rcp = 1.0f / 256.0f;
			float voxel_size = 0.0078125f;
			float voxel_size_rcp = 1.0f / 0.0078125f;
			XMFLOAT3 center = XMFLOAT3(0, 0, 0);
			XMFLOAT3 extents = XMFLOAT3(0, 0, 0);
			uint32_t num_cones = 2;
			float ray_step_size = 0.0058593f;
			float max_distance = 5.0f;
			bool secondary_bounce_enabled = false;
			bool reflections_enabled = true;
			bool center_changed_this_frame = true;
			uint32_t mips = 7;
		};

		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   voxelizer_pipeline_state;
		Microsoft::WRL::ComPtr<ID3D12RootSignature>	           voxelizer_root_signature;
		D3D12_VIEWPORT									       voxelizer_viewport;
};


