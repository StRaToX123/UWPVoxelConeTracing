#include "Graphics/DirectX/DX12RenderTarget.h"

DX12RenderTarget::DX12RenderTarget(ID3D12Device* device, 
	DX12DescriptorHeapManager* descriptorManager, 
	int width, 
	int height, 
	DXGI_FORMAT aFormat, 
	D3D12_RESOURCE_FLAGS flags, 
	LPCWSTR name, 
	int depth, 
	int mips, 
	D3D12_RESOURCE_STATES defaultState)
{
	width = width;
	height = height;
	this->depth = depth;
	format = aFormat;

	XMFLOAT4 clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	DXGI_FORMAT format = aFormat;

	// Describe and create a Texture2D/3D
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = mips;
	textureDesc.Format = format;
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.DepthOrArraySize = (depth > 0) ? depth : 1;
	textureDesc.Flags = flags;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = (depth > 0) ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = format;
	optimizedClearValue.Color[0] = clearColor.x;
	optimizedClearValue.Color[1] = clearColor.y;
	optimizedClearValue.Color[2] = clearColor.z;
	optimizedClearValue.Color[3] = clearColor.w;

	resource_state_current = defaultState;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		resource_state_current,
		&optimizedClearValue,
		IID_PPV_ARGS(&render_target)));

	render_target->SetName(name);

	D3D12_DESCRIPTOR_HEAP_FLAGS flagsHeap = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = aFormat;
	if (depth > 0) 
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	}
	else 
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = aFormat;
	if (depth > 0)
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		rtvDesc.Texture3D.MipSlice = mipLevel;
		rtvDesc.Texture3D.WSize = (depth >> mipLevel);
	}
	else
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = mipLevel;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = aFormat;
	if (depth > 0)
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.MipSlice = mipLevel;
		uavDesc.Texture3D.WSize = (depth >> mipLevel);
	}
	else
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = mipLevel;
	}

	descriptor_handle_block_per_mip_srv = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, mips);
	descriptor_handle_block_per_mip_uav = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, mips);
	descriptor_handle_block_per_mip_rtv = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, mips);

	
	for (int mipLevel = 0; mipLevel < mips; mipLevel++)
	{
		

		

		descriptor_handle_block_per_mip_rtv[mipLevel] = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		device->CreateRenderTargetView(render_target.Get(), &rtvDesc, descriptor_handle_block_per_mip_rtv[mipLevel].GetCPUHandle());

		if (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
		{
			descriptor_handle_block_per_mip_uav[mipLevel] = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			device->CreateUnorderedAccessView(render_target.Get(), nullptr, &uavDesc, descriptor_handle_block_per_mip_uav[mipLevel].GetCPUHandle());
		}

		device->CreateShaderResourceView(render_target.Get(), &srvDesc, mDescriptorSRV.GetCPUHandle());
	}
}

void DX12RenderTarget::TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, 
	ID3D12GraphicsCommandList* commandList, 
	D3D12_RESOURCE_STATES stateAfter,
	UINT subresource)
{
	if (stateAfter != resource_state_current)
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), resource_state_current, stateAfter, subresource));
		resource_state_current = stateAfter;
	}
}