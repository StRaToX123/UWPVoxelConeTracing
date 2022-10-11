#pragma once

//#include <ppltasks.h>
//#include <synchapi.h>
#include <fstream>
//#include <pix.h>
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
			static const UINT c_alignedConstantBufferSize = (sizeof(ModelViewProjectionConstantBuffer) + 255) & ~255;
			// Direct3D resources for cube geometry.
			std::shared_ptr<DX::DeviceResources>                device_resources;
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	command_list_direct;
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	command_list_copy;
			Microsoft::WRL::ComPtr<ID3D12Fence>				    fence_copy_command_list_progress;
			UINT64                                              fence_copy_command_list_progress_highest_value;
			Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
			Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		descriptor_heap_cbv;
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		descriptor_heap_sampler;
			Microsoft::WRL::ComPtr<ID3D12Resource>				m_constantBuffer;
			ModelViewProjectionConstantBuffer					m_constantBufferData;
			UINT8*												m_mappedConstantBuffer;
			UINT												m_cbvDescriptorSize;
			Microsoft::WRL::ComPtr<ID3D12Resource>				test_texture;
			Microsoft::WRL::ComPtr<ID3D12Resource>				test_texture_upload;
			D3D12_CPU_DESCRIPTOR_HANDLE                         test_texture_srv;
			D3D12_RECT											m_scissorRect;


			float	m_radiansPerSecond;
			float	m_angle;
	};
}

