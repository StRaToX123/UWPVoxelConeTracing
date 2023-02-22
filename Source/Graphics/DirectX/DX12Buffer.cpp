#include "DX12Buffer.h"

//////////////////////////////////////
////////// DX12Buffer ////////////////
//////////////////////////////////////


DX12Buffer::DX12Buffer(DX12DescriptorHeapManager* _descriptorHeapManager,
	Description& aDescription,
	UINT numberOfCopys,
	LPCWSTR name,
	void* _data,
	LONG_PTR dataSize,
	ID3D12GraphicsCommandList* _commandList)

	: description(aDescription),
	_buffer_mapped_data(nullptr),
	number_of_copys(numberOfCopys)
{
	//if (description.descriptor_type & DescriptorType::CBV)
	//{
		//buffer_size_in_bytes = Align(buffer_size_in_bytes, 256);
	//}

	CreateResources(_descriptorHeapManager, _data, dataSize, _commandList);
	buffer->SetName(name);
}

DX12Buffer::~DX12Buffer()
{
	if (_buffer_mapped_data != nullptr)
	{
		buffer->Unmap(0, nullptr);
	}
}

void DX12Buffer::CreateResources(DX12DescriptorHeapManager* _descriptorHeapManager,
	void* _data,
	LONG_PTR dataSize,
	ID3D12GraphicsCommandList* _commandList)
{
	ID3D12Device* _d3dDevice = DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice();
	D3D12_DESCRIPTOR_HEAP_FLAGS flagsHeap = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	D3D12_RESOURCE_DESC desc = {};

	buffer_size_in_bytes = (description.num_of_elements * description.element_size) + ((description.counter_offset != UINT64_MAX) ? sizeof(UINT) : 0);
	buffer_size_in_bytes = Align(buffer_size_in_bytes, 256);

	desc.Alignment = description.alignment;
	desc.DepthOrArraySize = 1;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Flags = description.resource_flags;
	if (description.descriptor_type & DescriptorType::UAV)
	{
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.Height = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Width = buffer_size_in_bytes * number_of_copys;

	D3D12_HEAP_PROPERTIES heapProperties;
	heapProperties.Type = (description.descriptor_type & DescriptorType::CBV) ? D3D12_HEAP_TYPE_UPLOAD : description.heap_type;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	ThrowIfFailed(
		_d3dDevice->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			(_data != nullptr) ? D3D12_RESOURCE_STATE_COPY_DEST : description.state,
			nullptr,
			IID_PPV_ARGS(&buffer)
		)
	);

	if (_data != nullptr)
	{
		// Create the GPU upload buffer.
		ThrowIfFailed(_d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(buffer_size_in_bytes),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer_upload)));

		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = _data;
		data.RowPitch = dataSize;
		data.SlicePitch = 0;

		UpdateSubresources(_commandList, buffer.Get(), buffer_upload.Get(), 0, 0, 1, &data);
		if (description.state != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, description.state));
		}
	}
	
	resource_state_current = description.state;
	if (description.descriptor_type & DescriptorType::SRV)
	{
		descriptor_handle_block_srv = _descriptorHeapManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, number_of_copys);
		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = description.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.NumElements = description.num_of_elements;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		if (description.descriptor_type & DX12Buffer::DescriptorType::Structured)
		{
			srvDesc.Buffer.StructureByteStride = description.element_size;
		}
		else 
		{
			if (description.descriptor_type & DX12Buffer::DescriptorType::Raw)
			{
				srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			}
		}

		for(UINT i = 0; i < descriptor_handle_block_srv.GetBlockSize(); i++)
		{
			srvDesc.Buffer.FirstElement = (UINT64)(i * description.num_of_elements);
			_d3dDevice->CreateShaderResourceView(buffer.Get(), &srvDesc, descriptor_handle_block_srv.GetCPUHandle(i));
		}
	}

	if (description.descriptor_type & DescriptorType::UAV)
	{
		descriptor_handle_block_uav = _descriptorHeapManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, number_of_copys);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = description.format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.StructureByteStride = 0;
		if (description.counter_offset != UINT64_MAX)
		{
			uavDesc.Buffer.CounterOffsetInBytes = description.counter_offset;
		}
		
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		if (description.descriptor_type & DX12Buffer::DescriptorType::Structured)
		{
			uavDesc.Buffer.StructureByteStride = description.element_size;
			uavDesc.Buffer.NumElements = description.num_of_elements;
		}
		else
		{
			if (description.descriptor_type & DX12Buffer::DescriptorType::Raw)
			{
				uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
				uavDesc.Buffer.NumElements = 1;
			}
		}

		D3D12_GPU_VIRTUAL_ADDRESS resourceGPUAddress = buffer->GetGPUVirtualAddress();
		for (UINT i = 0; i < descriptor_handle_block_uav.GetBlockSize(); i++)
		{
			uavDesc.Buffer.FirstElement = (UINT64)(i * description.num_of_elements);
			_d3dDevice->CreateUnorderedAccessView(buffer.Get(), 
				(description.counter_offset != UINT64_MAX) ? buffer.Get() : nullptr, 
				&uavDesc,
				descriptor_handle_block_uav.GetCPUHandle(i));
		}
	}

	if (description.descriptor_type & DescriptorType::CBV)
	{
		// Create constant buffer views to access the upload buffer.
		descriptor_handle_block_cbv = _descriptorHeapManager->GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, number_of_copys);
		D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = buffer->GetGPUVirtualAddress();
		for (int i = 0; i < descriptor_handle_block_cbv.GetBlockSize(); i++)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
			desc.BufferLocation = cbvGpuAddress;
			desc.SizeInBytes = buffer_size_in_bytes;
			_d3dDevice->CreateConstantBufferView(&desc, descriptor_handle_block_cbv.GetCPUHandle(i));

			cbvGpuAddress += desc.SizeInBytes;
		}
	}
}

unsigned char* DX12Buffer::GetMappedData(UINT copyIndex)
{
	if (_buffer_mapped_data == nullptr)
	{
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(buffer->Map(0, &readRange, reinterpret_cast<void**>(&_buffer_mapped_data)));
	}

	return _buffer_mapped_data + (copyIndex * description.element_size);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12Buffer::GetSRV(UINT copyIndex)
{ 
	return descriptor_handle_block_srv.GetCPUHandle(copyIndex);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12Buffer::GetUAV(UINT copyIndex)
{
	return descriptor_handle_block_uav.GetCPUHandle(copyIndex);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12Buffer::GetCBV(UINT copyIndex)
{ 

	return descriptor_handle_block_cbv.GetCPUHandle(copyIndex);
}

void DX12Buffer::TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers,
	ID3D12GraphicsCommandList* commandList,
	D3D12_RESOURCE_STATES stateAfter)
{
	if (stateAfter != resource_state_current)
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), resource_state_current, stateAfter, 0));
		resource_state_current = stateAfter;
	}
}