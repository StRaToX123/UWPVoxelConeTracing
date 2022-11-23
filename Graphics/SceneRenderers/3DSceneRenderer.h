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
//#include "Scene/Mesh.h"
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

	private:
		void LoadState();
		void LoadShaderByteCode(const char* shaderPath, _Out_ char*& shaderByteCode, _Out_ int& shaderByteCodeLength);
		static inline UINT AlignForUavCounter(UINT bufferSize)
		{
			const UINT alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
			return (bufferSize + (alignment - 1)) & ~(alignment - 1);
		};
		void AssignDescriptors(ID3D12GraphicsCommandList* _commandList, bool isComputeCommandList, UINT currentFrameIndex);

		// Constant buffers must be aligned to the D3D12_TEXTURE_DATA_PITCH_ALIGNMENT.
		static const UINT c_aligned_view_projection_matrix_constant_buffer = (sizeof(ShaderStructureCPUViewProjectionBuffer) + 255) & ~255;
		
		// Direct3D resources for cube geometry.
		std::shared_ptr<DeviceResources>                       device_resources;
		UINT                                                   previous_frame_index;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	   command_list_direct;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	   command_list_copy_normal_priority;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	   command_list_copy_high_priority;
		Microsoft::WRL::ComPtr<ID3D12Fence>				       fence_command_list_direct_progress;
		Microsoft::WRL::ComPtr<ID3D12Fence>				       fence_command_list_compute_progress;
		Microsoft::WRL::ComPtr<ID3D12Fence>				       fence_command_list_copy_normal_priority_progress;
		Microsoft::WRL::ComPtr<ID3D12Fence>				       fence_command_list_copy_high_priority_progress;
		HANDLE                                                 event_command_list_copy_high_priority_progress;
		UINT64                                                 fence_command_list_direct_progress_latest_unused_value;
		UINT64                                                 fence_command_list_compute_progress_latest_unused_value;
		UINT64                                                 fence_command_list_copy_normal_priority_progress_latest_unused_value;
		UINT64                                                 fence_command_list_copy_high_priority_progress_latest_unused_value;
		UINT64                                                 fence_per_frame_command_list_copy_high_priority_values[c_frame_count];
		bool                                                   command_allocator_copy_normal_priority_already_reset;
		bool                                                   command_list_copy_normal_priority_requires_reset;
		bool                                                   command_allocator_copy_high_priority_already_reset;
		bool                                                   command_list_copy_high_priority_requires_reset;
		Microsoft::WRL::ComPtr<ID3D12RootSignature>			   root_signature;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		   descriptor_heap_cbv_srv_uav;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		   descriptor_heap_sampler;
		CD3DX12_CPU_DESCRIPTOR_HANDLE                          cbv_srv_uav_cpu_handle;
		UINT										   	       cbv_srv_uav_descriptor_size;
		Microsoft::WRL::ComPtr<ID3D12Resource>			   	   test_texture;
		Microsoft::WRL::ComPtr<ID3D12Resource>				   test_texture_upload;
		D3D12_RECT											   scissor_rect_default;
		Microsoft::WRL::ComPtr<ID3D12Resource>				   camera_view_projection_constant_buffer;
		ShaderStructureCPUViewProjectionBuffer		           camera_view_projection_constant_buffer_data;
		UINT8*												   camera_view_projection_constant_mapped_buffer;
		vector<Microsoft::WRL::ComPtr<ID3D12Resource>>         transform_matrix_upload_buffers;
		vector<ShaderStructureCPUModelAndInverseTransposeModelView*> transform_matrix_mapped_upload_buffers;
		vector<vector<UINT>>                                   transform_matrix_buffer_free_slots;
		UINT                                                   free_slots_preallocated_array[TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES];
		vector<UINT>                                           available_transform_matrix_buffers;
		vector<UINT>                                           scene_object_indexes_that_require_rendering;
		
		//////////////
		// Voxel GI //
		//////////////
		struct ShaderStructureCPUVoxelGridData
		{
			ShaderStructureCPUVoxelGridData()
			{
				UpdateBottomLeftPointWorldSpace();
			};

			void UpdateBottomLeftPointWorldSpace()
			{
				bottom_left_point_world_space.x = -(res * voxel_half_extent);
				bottom_left_point_world_space.y = bottom_left_point_world_space.x;
				bottom_left_point_world_space.z = bottom_left_point_world_space.x;
			};

			uint32_t res = 256;
			float res_rcp = 1.0f / 256.0f;
			float voxel_extent = 0.0078125f;
			float voxel_extent_rcp = 1.0f / 0.0078125f;
			float voxel_half_extent = 0.0078125f / 2.0f;
			float voxel_half_extent_rcp = 1.0f / (0.0078125f / 2.0f);
			XMFLOAT3 bottom_left_point_world_space = XMFLOAT3(0, 0, 0);
			XMFLOAT3 center_world_space = XMFLOAT3(0, 0, 0);
			uint32_t num_cones = 2;
			float ray_step_size = 0.0058593f;
			float max_distance = 5.0f;
			bool secondary_bounce_enabled = false;
			bool reflections_enabled = true;
			bool center_changed_this_frame = true;
			uint32_t mips = 7;
		};

		const UINT c_aligned_shader_structure_cpu_voxel_grid_data = (sizeof(ShaderStructureCPUVoxelGridData) + 255) & ~255;
		struct IndirectCommand
		{
			D3D12_DRAW_ARGUMENTS draw_arguments;
		};

		struct ShaderStructureCPUVoxelDebugData
		{
			UINT voxel_index;
			XMFLOAT4X4 debug_cube_model_transform_matrix;
		};

		UINT64                                                 max_possible_number_of_voxels;
		ShaderStructureCPUVoxelGridData                        voxel_grid_data;
		UINT                                                   voxel_grid_allowed_resolutions[4];
		Microsoft::WRL::ComPtr<ID3D12Resource>                 voxel_grid_data_constant_buffer;
		ShaderStructureCPUVoxelGridData*                       voxel_grid_data_constant_mapped_buffer;
		UINT                                                   voxel_grid_data_constant_buffer_frame_index_containing_most_updated_version;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>      command_list_compute;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   pipeline_state_default;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   pipeline_state_voxelizer;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   pipeline_state_radiance_temporal_clear;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   pipeline_voxel_debug_visualization;
		D3D12_VIEWPORT									       viewport_voxelizer;
		D3D12_RECT                                             scissor_rect_voxelizer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 voxel_data_structured_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 indirect_command_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 per_frame_indirect_procesed_commands_buffer[c_frame_count];
		Microsoft::WRL::ComPtr<ID3D12Resource>                 per_frame_indirect_procesed_commands_upload_buffer[c_frame_count];
		Microsoft::WRL::ComPtr<ID3D12Resource>                 indirect_processed_command_counter_reset_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 per_frame_indirect_command_buffer[c_frame_count];
		Microsoft::WRL::ComPtr<ID3D12Resource>                 per_frame_indirect_command_upload_buffer[c_frame_count];
		UINT                                                   indirect_processed_command_counter_offset;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 voxel_debug_constant_buffer;
		Mesh                                                   voxel_debug_cube;
		Microsoft::WRL::ComPtr<ID3D12CommandSignature>         voxel_debug_command_signature;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 per_frame_radiance_texture_3d[c_frame_count];

		
};


