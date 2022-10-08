#pragma once

//#include <ppltasks.h>
//#include <synchapi.h>
#include <fstream>
//#include <pix.h>
#include <DirectXColors.h>
#include "Graphics/DeviceResources/DeviceResources.h"
#include "Graphics/Shaders/SampleShaders/ShaderStructures.h"
#include "Utility/Time/StepTimer.h"
#include "Utility/Debugging/DebugMessage.h"
#include "Scene/Camera.h"
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
			Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
			~Sample3DSceneRenderer();
			void CreateDeviceDependentResources();
			void CreateWindowSizeDependentResources();
			ID3D12GraphicsCommandList* Render(Camera& camera, DX::StepTimer const& timer, bool showImGui);
			void SaveState();


		private:
			void LoadState();
			// Constant buffers must be 256-byte aligned.
			static const UINT c_alignedConstantBufferSize = (sizeof(ModelViewProjectionConstantBuffer) + 255) & ~255;
			// Direct3D resources for cube geometry.
			std::shared_ptr<DX::DeviceResources>                device_resources;
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_commandList;
			Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
			Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_cbvHeap;
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		imGui_cbv_heap;
			Microsoft::WRL::ComPtr<ID3D12Resource>				m_vertexBuffer;
			Microsoft::WRL::ComPtr<ID3D12Resource>				m_indexBuffer;
			Microsoft::WRL::ComPtr<ID3D12Resource>				m_constantBuffer;
			ModelViewProjectionConstantBuffer					m_constantBufferData;
			UINT8*												m_mappedConstantBuffer;
			UINT												m_cbvDescriptorSize;
			D3D12_RECT											m_scissorRect;
			D3D12_VERTEX_BUFFER_VIEW							m_vertexBufferView;
			D3D12_INDEX_BUFFER_VIEW								m_indexBufferView;



			float	m_radiansPerSecond;
			float	m_angle;
	};
}

