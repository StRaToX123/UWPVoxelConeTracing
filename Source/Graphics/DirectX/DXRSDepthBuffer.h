#pragma once
#include "Common.h"
#include "Graphics/DirectX/DX12DescriptorHeap.h"

class DXRSDepthBuffer
{
	public:
		DXRSDepthBuffer(ID3D12Device* device, DX12DescriptorHeapManager* descriptorManager, int width, int height, DXGI_FORMAT format);

		ID3D12Resource* GetResource() { return mDepthStencilResource.Get(); }
		DXGI_FORMAT GetFormat() { return mFormat; }
		void TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES stateAfter);

		DX12DescriptorHandleBlock GetDSV()
		{
			return mDescriptorDSV;
		}

		DX12DescriptorHandleBlock& GetSRV()
		{
			return mDescriptorSRV;
		}

		const int GetWidth() { return mWidth; }
		const int GetHeight() { return mHeight; }

	private:

		int mWidth, mHeight;
		DXGI_FORMAT mFormat;
		D3D12_RESOURCE_STATES mCurrentResourceState;

		DX12DescriptorHandleBlock mDescriptorDSV;
		DX12DescriptorHandleBlock mDescriptorSRV;
		ComPtr<ID3D12Resource> mDepthStencilResource;
};