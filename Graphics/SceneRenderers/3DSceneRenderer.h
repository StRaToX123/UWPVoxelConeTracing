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
		static const UINT c_aligned_transform_matrix_buffer = ((sizeof(ShaderStructureCPUModelAndInverseTransposeModelView) * TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES) + 255) & ~255;
		
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
			enum AllowedResolution
			{
				RES_16 = 16,
				RES_32 = 32,
				RES_64 = 64,
				RES_128 = 128,
				RES_256 = 256,
				MAX_RES = 256
			};

			ShaderStructureCPUVoxelGridData()
			{
				UpdateRes(AllowedResolution::MAX_RES);
			};

			

			void UpdateRes(AllowedResolution newRes)
			{
				res = newRes;
				voxel_extent = grid_extent / (float)res;
				voxel_extent_rcp = 1.0f / voxel_extent;
				bottom_left_point_world_space_x = -((float)res * (voxel_extent / 2.0f));
				bottom_left_point_world_space_y = bottom_left_point_world_space_x;
				bottom_left_point_world_space_z = bottom_left_point_world_space_x;
				grid_half_extent_rcp = 1.0f / (-bottom_left_point_world_space_x);
			};

			void UpdateGirdExtent(float gridExtent)
			{
				grid_extent = gridExtent;
				grid_half_extent_rcp = 1.0f / (grid_extent / 2.0f);
				voxel_extent = grid_extent / (float)res;
				voxel_extent_rcp = 1.0f / voxel_extent;
			};

			uint32_t res = 256;
			float grid_extent = 2.0f;
			float grid_half_extent_rcp = 1.0f;
			float voxel_extent = 0.0078125f;
			float voxel_extent_rcp = 1.0f / 0.0078125f;
			float bottom_left_point_world_space_x = 0.0f;
			float bottom_left_point_world_space_y = 0.0f;
			float bottom_left_point_world_space_z = 0.0f;
			float center_world_space_x = 0.0f;
			float center_world_space_y = 0.0f;
			float center_world_space_z = 0.0f;
			uint32_t num_cones = 2;
			float ray_step_size = 0.0058593f;
			float max_distance = 5.0f;
			uint32_t secondary_bounce_enabled = 1;
			uint32_t reflections_enabled = 1;
			uint32_t center_changed_this_frame = 0;
			uint32_t mips = 7;
		};

		const UINT c_aligned_shader_structure_cpu_voxel_grid_data = (sizeof(ShaderStructureCPUVoxelGridData) + 255) & ~255;
		struct IndirectCommand
		{
			D3D12_DRAW_INDEXED_ARGUMENTS draw_indexed_arguments;
		};

		struct ShaderStructureCPUVoxelDebugData
		{
			float color_r;
			float color_g;
			float color_b;
			XMFLOAT4X4 debug_cube_model_transform_matrix;
		};

		// This value is saved as a member variable so that we dont have to keep recalculating it every frame
		// when we're trying to dispatch a compute shader with the number of thread equal to the number of voxels in the scene
		UINT64                                                 number_of_voxels_in_grid;
		ShaderStructureCPUVoxelGridData                        voxel_grid_data;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 voxel_grid_data_constant_buffer;
		UINT8*                                                 voxel_grid_data_constant_mapped_buffer;
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
		Microsoft::WRL::ComPtr<ID3D12Resource>                 per_frame_indirect_draw_required_voxel_debug_data_buffer[c_frame_count];
		Microsoft::WRL::ComPtr<ID3D12Resource>                 per_frame_indirect_draw_required_voxel_debug_data_upload_buffer[c_frame_count];
		Microsoft::WRL::ComPtr<ID3D12Resource>                 indirect_draw_required_voxel_debug_data_counter_reset_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 per_frame_indirect_command_buffer[c_frame_count];
		Microsoft::WRL::ComPtr<ID3D12Resource>                 per_frame_indirect_command_upload_buffer[c_frame_count];
		UINT                                                   indirect_draw_required_voxel_debug_data_counter_offset;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 voxel_debug_constant_buffer;
		Mesh                                                   voxel_debug_cube;
		Microsoft::WRL::ComPtr<ID3D12CommandSignature>         voxel_debug_command_signature;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 per_frame_radiance_texture_3d[c_frame_count];

		
};


