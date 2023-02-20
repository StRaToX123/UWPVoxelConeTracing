#pragma once
#include "Common.h"
#include "Graphics\DirectX\DX12DescriptorHeap.h"

class DX12RenderTarget
{
	public:
		DX12RenderTarget(ID3D12Device* device,
			DX12DescriptorHeapManager* descriptorManger, 
			int width, 
			int height, 
			DXGI_FORMAT aFormat,
			D3D12_RESOURCE_FLAGS flags,
			LPCWSTR name, 
			int depth = -1, 
			int mips = 1, 
			D3D12_RESOURCE_STATES defaultState = D3D12_RESOURCE_STATE_RENDER_TARGET);

		ID3D12Resource* GetResource() { return render_target.Get(); }

		int GetWidth() { return width; }
		int GetHeight() { return height; }
		int GetDepth() { return depth; }
		void TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, 
			ID3D12GraphicsCommandList* commandList,
			D3D12_RESOURCE_STATES stateAfter,
			UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		D3D12_RESOURCE_STATES GetCurrentState(UINT mip) { return resource_state_current_per_mip[mip]; }

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetRTV(UINT mip = 0);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetSRV(UINT mip = 0);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetUAV(UINT mip = 0);

	private:
		int width, height, depth;
		DXGI_FORMAT format;
		std::vector<D3D12_RESOURCE_STATES> resource_state_current_per_mip;

		DX12DescriptorHandleBlock descriptor_handle_block_per_mip_srv;
		ComPtr<ID3D12Resource> render_target;

		DX12DescriptorHandleBlock descriptor_handle_block_per_mip_uav;
		DX12DescriptorHandleBlock descriptor_handle_block_per_mip_rtv;
};