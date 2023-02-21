#include "DX12DepthBuffer.h"

DX12DepthBuffer::DX12DepthBuffer(ID3D12Device* device, DX12DescriptorHeapManager* _descriptorManager, int width, int height, DXGI_FORMAT aFormat)
{
	this->width = width;
	this->height = height;
	format = aFormat;
	current_resource_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = aFormat;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	DXGI_FORMAT format = DXGI_FORMAT_R32_TYPELESS;

	if (aFormat == DXGI_FORMAT_D16_UNORM)
	{
		format = DXGI_FORMAT_R16_TYPELESS;
	}

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		current_resource_state,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&depth_stencil)
	));

	// DSV
	descriptor_handle_block_dsv = _descriptorManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
	depthStencilDesc.Format = aFormat;
	depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	device->CreateDepthStencilView(depth_stencil.Get(), &depthStencilDesc, descriptor_handle_block_dsv.GetCPUHandle());

	// SRV
	descriptor_handle_block_srv = _descriptorManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	format = DXGI_FORMAT_R32_FLOAT;
	if (aFormat == DXGI_FORMAT_D16_UNORM)
	{
		format = DXGI_FORMAT_R16_UNORM;
	}
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(depth_stencil.Get(), &srvDesc, descriptor_handle_block_srv.GetCPUHandle());
}

void DX12DepthBuffer::TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES stateAfter)
{
	if (stateAfter != current_resource_state)
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), current_resource_state, stateAfter));
		current_resource_state = stateAfter;
	}
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12DepthBuffer::GetDSV()
{
	return descriptor_handle_block_dsv.GetCPUHandle();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12DepthBuffer::GetSRV()
{
	return descriptor_handle_block_srv.GetCPUHandle();
}
