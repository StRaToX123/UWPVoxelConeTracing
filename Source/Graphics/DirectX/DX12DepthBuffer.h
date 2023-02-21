#pragma once
#include "Common.h"
#include "Graphics/DirectX/DX12DescriptorHeap.h"

class DX12DepthBuffer
{
	public:
		DX12DepthBuffer(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager, int width, int height, DXGI_FORMAT format);

		ID3D12Resource* GetResource() { return depth_stencil.Get(); }
		DXGI_FORMAT GetFormat() { return format; }
		void TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES stateAfter);

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDSV();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetSRV();

		const int GetWidth() { return width; }
		const int GetHeight() { return height; }

	private:
		int width, height;
		DXGI_FORMAT format;
		D3D12_RESOURCE_STATES current_resource_state;

		DX12DescriptorHandleBlock descriptor_handle_block_dsv;
		DX12DescriptorHandleBlock descriptor_handle_block_srv;
		ComPtr<ID3D12Resource> depth_stencil;
};