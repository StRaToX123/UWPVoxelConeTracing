#pragma once
#include "Common.h"
#include "Graphics\DirectX\DX12DescriptorHeap.h"

class DXRSRenderTarget
{
public:
	DXRSRenderTarget(ID3D12Device* device, 
		DX12DescriptorHeapManager* descriptorManger, 
		int width, 
		int height, 
		DXGI_FORMAT aFormat,
		D3D12_RESOURCE_FLAGS flags,
		LPCWSTR name, 
		int depth = -1, 
		int mips = 1, 
		D3D12_RESOURCE_STATES defaultState = D3D12_RESOURCE_STATE_RENDER_TARGET);

	ID3D12Resource* GetResource() { return mRenderTarget.Get(); }

	int GetWidth() { return mWidth; }
	int GetHeight() { return mHeight; }
	int GetDepth() { return mDepth; }
	void TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES stateAfter);
	D3D12_RESOURCE_STATES GetCurrentState() { return mCurrentResourceState; }

	DX12DescriptorHandleBlock& GetRTV(int mip = 0)
	{
		return mDescriptorRTVMipsHandles[mip];
	}

	DX12DescriptorHandleBlock& GetSRV()
	{
		return mDescriptorSRV;
	}

	DX12DescriptorHandleBlock& GetUAV(int mip = 0)
	{
		return mDescriptorUAVMipsHandles[mip];
	}

private:

	int mWidth, mHeight, mDepth;
	DXGI_FORMAT mFormat;
	D3D12_RESOURCE_STATES mCurrentResourceState;

	//DX12DescriptorHandle mDescriptorUAV;
	DX12DescriptorHandleBlock mDescriptorSRV;
	//DX12DescriptorHandle mDescriptorRTV;
	ComPtr<ID3D12Resource> mRenderTarget;

	std::vector<DX12DescriptorHandleBlock> mDescriptorUAVMipsHandles;
	std::vector<DX12DescriptorHandleBlock> mDescriptorRTVMipsHandles;
};