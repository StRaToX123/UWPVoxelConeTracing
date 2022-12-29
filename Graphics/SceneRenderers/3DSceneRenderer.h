#pragma once

#include <vector>
#include <unordered_map>
#include <DirectXColors.h>
#include <DirectXTex.h>
#include "Graphics/DeviceResources/DeviceResources.h"
#include "Graphics/Shaders/ShaderGlobalsCPU.h"
#include "Graphics/Shaders/ShaderGlobalsCPUGPU.hlsli"
#include "Utility/Debugging/DebugMessage.h"
#include "Utility/Memory/Allocators/PreAllocator.h"
#include "Scene/Camera.h"
#include "Scene/Light.h"
#include "c:\users\stratox\documents\visual studio 2019\projects\voxelconetracing\Graphics\Shaders\VoxelGI\RadianceGenerate3DMipChainCPUGPU.hlsli"




//using namespace VoxelConeTracing;
using namespace Concurrency;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace std;



// This sample renderer instantiates a basic rendering pipeline.
class SceneRenderer3D
{
	public:
		SceneRenderer3D(const std::shared_ptr<DeviceResources>& deviceResources, Camera& camera, unsigned int voxelGridResolution);
		~SceneRenderer3D();
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources(Camera& camera);
		void Render(vector<Mesh>& scene, SpotLight& spotLight, Camera& camera, bool showVoxelDebugView);
		void SaveState();
		void LoadState();
		static inline UINT AlignForUavCounter(UINT bufferSize)
		{
			return (bufferSize + (D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT - 1);
		};

		void AssignDescriptors(ID3D12GraphicsCommandList* _commandList, CD3DX12_GPU_DESCRIPTOR_HANDLE& descriptorHandle, UINT rootParameterIndex, bool assignCompute);
		// All the update buffers were put into a single function call so that they can all get batched into one Execute call
		void UpdateBuffers(bool updateVoxelizerBuffers, bool updateVoxelGridDataBuffers);
		void CopyDescriptorsIntoDescriptorHeap(CD3DX12_CPU_DESCRIPTOR_HANDLE& destinationDescriptorHandle);
		

		// Constant buffers must be aligned to the D3D12_TEXTURE_DATA_PITCH_ALIGNMENT.
		static const UINT c_aligned_transform_matrix_buffer = ((sizeof(ShaderStructureCPUModelAndInverseTransposeModelView) * TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES) + 255) & ~255;
		
		Microsoft::WRL::ComPtr<ID3D12Resource>			   	   temp_texture;
		std::shared_ptr<DeviceResources>                       device_resources;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>           descriptor_heap_cbv_srv_uav_voxelizer;
		int                                                    descriptor_heap_cbv_srv_uav_voxelizer_number_of_filled_descriptors;
		CD3DX12_CPU_DESCRIPTOR_HANDLE                          descriptor_cpu_handle_cbv_srv_uav_voxelizer;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>           descriptor_heap_cbv_srv_uav_all_other_resources;
		int                                                    descriptor_heap_cbv_srv_uav_all_other_resources_number_of_filled_descriptors;
		CD3DX12_CPU_DESCRIPTOR_HANDLE                          descriptor_cpu_handle_cbv_srv_uav_all_other_resources;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	       render_target_heap;
		UINT                                                   previous_frame_index;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	   command_list_direct;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	   command_list_copy_normal_priority;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	   command_list_copy_high_priority;
		Microsoft::WRL::ComPtr<ID3D12Fence>				       fence_command_list_direct_progress;
		Microsoft::WRL::ComPtr<ID3D12Fence>				       fence_command_list_compute_progress;
		Microsoft::WRL::ComPtr<ID3D12Fence>				       fence_command_list_copy_normal_priority_progress;
		Microsoft::WRL::ComPtr<ID3D12Fence>				       fence_command_list_copy_high_priority_progress;
		HANDLE                                                 event_wait_for_gpu;
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
		Microsoft::WRL::ComPtr<ID3D12Resource>			   	   test_texture;
		Microsoft::WRL::ComPtr<ID3D12Resource>				   test_texture_upload;
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
				
			};

			void UpdateRes(UINT newRes)
			{
				res = newRes;
				voxel_half_extent = (grid_extent / (float)res) / 2.0f;
				voxel_extent_rcp = 1.0f / (grid_extent / (float)res);
				top_left_point_world_space_x = -((float)res * voxel_half_extent);
				top_left_point_world_space_y = -top_left_point_world_space_x;
				top_left_point_world_space_z = top_left_point_world_space_x;
				grid_half_extent_rcp = 1.0f / (grid_extent / 2.0f);
			};

			void UpdateGirdExtent(float gridExtent)
			{
				grid_extent = gridExtent;
				grid_half_extent_rcp = 1.0f / (grid_extent / 2.0f);
				voxel_half_extent = (grid_extent / (float)res) / 2.0f;
				voxel_extent_rcp = 1.0f / (grid_extent / (float)res);
				top_left_point_world_space_x = -((float)res * voxel_half_extent);
				top_left_point_world_space_y = -top_left_point_world_space_x;
				top_left_point_world_space_z = top_left_point_world_space_x;
			};

			uint32_t res = 128;
			float grid_extent = 2.00f;
			float grid_half_extent_rcp = 1.0f;
			float voxel_half_extent = 0.0078125f;
			float voxel_extent_rcp = 1.0f / 0.0078125f;
			float top_left_point_world_space_x = 0.0f;
			float top_left_point_world_space_y = 0.0f;
			float top_left_point_world_space_z = 0.0f;
			float center_world_space_x = 0.0f;
			float center_world_space_y = 0.0f;
			float center_world_space_z = 0.0f;
			int num_cones = 2;
			float ray_step_size = 0.0058593f;
			float max_distance = 5.0f;
			int secondary_bounce_enabled = 1;
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
			unsigned int index_x;
			unsigned int index_y;
			unsigned int index_z;
		};

		struct ShaderStructureCPUGenerate3DMipChainData
		{
			UINT output_resolution;
			float output_resolution_rcp;
			UINT input_mip_level_index;
			UINT output_mip_level_index;
		};

		struct ShaderStructureCPUVoxelType
		{
			UINT color;
			UINT normal;
			float u_coord_shadow_map;
			float v_coord_shadow_map;
		};

		// This value is saved as a member variable so that we dont have to keep recalculating it every frame
		// when we're trying to dispatch a compute shader with the number of thread equal to the number of voxels in the scene
		UINT64                                                 number_of_voxels_in_grid;
		ShaderStructureCPUVoxelGridData                        voxel_grid_data;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 voxel_grid_data_constant_buffer;
		UINT8*                                                 voxel_grid_data_constant_mapped_buffer;
		UINT                                                   voxel_grid_data_constant_buffer_frame_index_containing_most_updated_version;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>      command_list_compute;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   pipeline_state_unlit;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   pipeline_state_voxelizer;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   pipeline_state_radiance_temporal_clear;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>	           pipeline_state_radiance_mip_chain_generation;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   pipeline_voxel_debug_visualization;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   pipeline_state_spot_light_write_only_depth;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>	           pipeline_state_spot_light_shadow_pass;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>            pipeline_state_full_screen_texture_visualization;
		D3D12_VIEWPORT									       viewport_voxelizer;
		D3D12_RECT                                             scissor_rect_voxelizer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 voxel_data_structured_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 voxel_data_structured_upload_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 indirect_draw_required_voxel_debug_data_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 indirect_draw_required_voxel_debug_data_counter_reset_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 indirect_command_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 indirect_command_upload_buffer;
		UINT                                                   indirect_draw_required_voxel_debug_data_counter_offset;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 voxel_debug_constant_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 voxel_debug_constant_upload_buffer;
		Mesh                                                   voxel_debug_cube;
		Microsoft::WRL::ComPtr<ID3D12CommandSignature>         voxel_debug_command_signature;
		Microsoft::WRL::ComPtr<ID3D12Resource>                 radiance_texture_3D;
		UINT                                                   radiance_texture_3D_mip_level_count;
		ShaderStructureCPUGenerate3DMipChainData               radiance_texture_3D_generate_mip_chain_data;
		UINT temp;

	private:
		void UpdateVoxelizerBuffers();
		void UpdateVoxelGridDataBuffer();
};


