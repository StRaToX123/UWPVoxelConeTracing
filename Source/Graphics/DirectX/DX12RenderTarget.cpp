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
	this->width = width;
	this->height = height;
	this->depth = depth;
	format = aFormat;

	XMFLOAT4 clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	DXGI_FORMAT format = aFormat;

	resource_state_current_per_mip.reserve(mips);
	for (UINT i = 0; i < mips; i++)
	{
		resource_state_current_per_mip.emplace_back(defaultState);
	}

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

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		defaultState,
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
	}
	else
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = aFormat;
	if (depth > 0)
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	}
	else
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	}

	descriptor_handle_block_per_mip_rtv = descriptorManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, mips);
	descriptor_handle_block_per_mip_srv = descriptorManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, mips);
	if (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		descriptor_handle_block_per_mip_uav = descriptorManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, mips);

	}

	for (int mipLevel = 0; mipLevel < mips; mipLevel++)
	{
		// RTV
		if (depth > 0)
		{
			rtvDesc.Texture3D.MipSlice = mipLevel;
			rtvDesc.Texture3D.WSize = (depth >> mipLevel);
		}
		else
		{
			rtvDesc.Texture2D.MipSlice = mipLevel;
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtcCPUDescriptorHandle = descriptor_handle_block_per_mip_rtv.GetCPUHandle(mipLevel);
		device->CreateRenderTargetView(render_target.Get(), &rtvDesc, rtcCPUDescriptorHandle);

		// SRV
		if (mipLevel == 0)
		{
			if (depth > 0)
			{
				srvDesc.Texture3D.MipLevels = mips;
			}
			else
			{
				srvDesc.Texture2D.MipLevels = mips;
			}
		}
		else
		{
			if (depth > 0)
			{
				srvDesc.Texture3D.MipLevels = 1;
			}
			else
			{
				srvDesc.Texture2D.MipLevels = 1;
			}
		}

		if (depth > 0)
		{
			srvDesc.Texture3D.MostDetailedMip = mipLevel;
		}
		else
		{
			srvDesc.Texture2D.MostDetailedMip = mipLevel;
		}
		
		device->CreateShaderResourceView(render_target.Get(), &srvDesc, descriptor_handle_block_per_mip_srv.GetCPUHandle(mipLevel));

		// UAV
		if (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
		{
			if (depth > 0)
			{
				uavDesc.Texture3D.MipSlice = mipLevel;
				uavDesc.Texture3D.WSize = (depth >> mipLevel);
			}
			else
			{
				uavDesc.Texture2D.MipSlice = mipLevel;
			}

			device->CreateUnorderedAccessView(render_target.Get(), nullptr, &uavDesc, descriptor_handle_block_per_mip_uav.GetCPUHandle(mipLevel));
		}
	}
}

void DX12RenderTarget::TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, 
	ID3D12GraphicsCommandList* commandList, 
	D3D12_RESOURCE_STATES stateAfter,
	UINT subresource)
{
	if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		if (resource_state_current_per_mip[0] == stateAfter)
		{
			return;
		}

		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), resource_state_current_per_mip[0], stateAfter, subresource));
		for (UINT i = 0; i < resource_state_current_per_mip.size(); i++)
		{
			resource_state_current_per_mip[i] = stateAfter;
		}

		return;
	}

	if (stateAfter != resource_state_current_per_mip[subresource])
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), resource_state_current_per_mip[subresource], stateAfter, subresource));
		resource_state_current_per_mip[subresource] = stateAfter;
	}
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12RenderTarget::GetRTV(UINT mip)
{
	return descriptor_handle_block_per_mip_rtv.GetCPUHandle(mip);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12RenderTarget::GetSRV(UINT mip)
{
	return descriptor_handle_block_per_mip_srv.GetCPUHandle(mip);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12RenderTarget::GetUAV(UINT mip)
{
	return descriptor_handle_block_per_mip_uav.GetCPUHandle(mip);
}