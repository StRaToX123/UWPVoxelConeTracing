#pragma once

#include <fstream>
#include <vector>
#include <DirectXColors.h>
#include <DirectXTex.h>
#include "Graphics/DeviceResources/DeviceResources.h"
#include "Graphics/Shaders/SampleShaders/ShaderStructures.h"
#include "Utility/Time/StepTimer.h"
#include "Utility/Debugging/DebugMessage.h"
#include "Scene/Camera.h"
#include "Scene/Mesh.h"
#include "ImGUI/imgui_impl_UWP.h"
#include "ImGUI/imgui_impl_dx12.h"




using namespace VoxelConeTracing;
using namespace Concurrency;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace std;

namespace VoxelConeTracing
{
	// This sample renderer instantiates a basic rendering pipeline.
	class Sample3DSceneRenderer
	{
		public:
			Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, Camera& camera);
			~Sample3DSceneRenderer();
			void CreateDeviceDependentResources();
			void CreateWindowSizeDependentResources(Camera& camera);
			ID3D12GraphicsCommandList* Render(vector<Mesh>& scene, Camera& camera, bool showImGui);
			void SaveState();


		private:
			void LoadState();
			// Constant buffers must be 256-byte aligned.
			static const UINT c_aligned_constant_buffer_size = (sizeof(ViewProjectionConstantBuffer) + 255) & ~255;
			// Direct3D resources for cube geometry.
			std::shared_ptr<DX::DeviceResources>                   device_resources;
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>      command_list_direct;
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	   command_list_copy;
			Microsoft::WRL::ComPtr<ID3D12Fence>				       fence_copy_command_list_progress;
			UINT64                                                 fence_copy_command_list_progress_highest_value;
			bool                                                   copy_command_allocator_already_reset;
			bool                                                   copy_command_list_requires_reset;
			Microsoft::WRL::ComPtr<ID3D12RootSignature>			   root_signature;
			Microsoft::WRL::ComPtr<ID3D12PipelineState>			   pipeline_state;
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		   descriptor_heap_cbv_srv;
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		   descriptor_heap_sampler;
			Microsoft::WRL::ComPtr<ID3D12Resource>				   camera_view_projection_constant_buffer;
			ViewProjectionConstantBuffer					       camera_view_projection_constant_buffer_data;
			UINT8*												   camera_view_projection_constant_mapped_buffer;
			vector<Microsoft::WRL::ComPtr<ID3D12Resource>>         per_scene_object_model_transform_matrix_constant_buffers;
			vector<Microsoft::WRL::ComPtr<ID3D12Resource>>         per_scene_object_model_transform_matrix_constant_upload_buffers;
			vector<vector<UINT>>                                   per_scene_object_model_transform_matrix_constant_buffer_occupied_slot;
			vector<vector<UINT>>                                   per_scene_object_model_transform_matrix_constant_buffer_free_slots;
			UINT											   	   cbv_srv_descriptor_size;
			Microsoft::WRL::ComPtr<ID3D12Resource>			   	   test_texture;
			Microsoft::WRL::ComPtr<ID3D12Resource>				   test_texture_upload;
			D3D12_RECT											   scissor_rect;
	};
}

