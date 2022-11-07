#pragma once

#include <fstream>
#include <vector>
#include <unordered_map>
#include <DirectXColors.h>
#include <DirectXTex.h>
#include "Graphics/DeviceResources/DeviceResources.h"
#include "Graphics/Shaders/SampleShaders/ShaderStructures.h"
#include "Utility/Debugging/DebugMessage.h"
#include "Utility/Memory/Allocators/PreAllocator.h"
#include "Scene/Camera.h"
#include "Scene/Mesh.h"




//using namespace VoxelConeTracing;
using namespace Concurrency;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace std;

typedef UINT ModelTransformMatrixBufferIndex;


// This sample renderer instantiates a basic rendering pipeline.
class Sample3DSceneRenderer
{
	public:
		Sample3DSceneRenderer(const std::shared_ptr<DeviceResources>& deviceResources, Camera& camera);
		~Sample3DSceneRenderer();
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources(Camera& camera);
		ID3D12GraphicsCommandList* Render(vector<Mesh>& scene, Camera& camera);
		void SaveState();


	private:
		void LoadState();
		// Constant buffers must be aligned to the D3D12_TEXTURE_DATA_PITCH_ALIGNMENT.
		static const UINT c_aligned_view_projection_matrix_constant_buffer = (sizeof(ShaderStructureViewProjectionConstantBuffer) + 255) & ~255;
		static const UINT c_aligned_model_transform_matrix = sizeof(ShaderStructureModelTransformMatrix);
		static const UINT c_aligned_model_transform_matrix_constant_buffer = sizeof(ShaderStructureModelTransformMatrixBuffer);
		
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
		Microsoft::WRL::ComPtr<ID3D12RootSignature>			   root_signature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			   pipeline_state;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		   descriptor_heap_cbv_srv;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		   descriptor_heap_sampler;
		Microsoft::WRL::ComPtr<ID3D12Resource>				   camera_view_projection_constant_buffer;
		ShaderStructureViewProjectionConstantBuffer		       camera_view_projection_constant_buffer_data;
		UINT8*												   camera_view_projection_constant_mapped_buffer;
		vector<Microsoft::WRL::ComPtr<ID3D12Resource>>         model_transform_matrix_upload_buffers;
		vector<XMFLOAT4X4*>                                    model_transform_matrix_mapped_upload_buffers;
		vector<vector<UINT>>                                   model_transform_matrix_buffer_free_slots;
		UINT                                                   free_slots_preallocated_array[MODEL_TRANSFORM_MATRIX_BUFFER_NUMBER_OF_ENTRIES];
		vector<UINT>                                           available_model_transform_matrix_buffer;
		CD3DX12_CPU_DESCRIPTOR_HANDLE                          cbv_srv_cpu_handle;
		UINT										   	       cbv_srv_descriptor_size;
		Microsoft::WRL::ComPtr<ID3D12Resource>			   	   test_texture;
		Microsoft::WRL::ComPtr<ID3D12Resource>				   test_texture_upload;
		D3D12_RECT											   scissor_rect;
};


