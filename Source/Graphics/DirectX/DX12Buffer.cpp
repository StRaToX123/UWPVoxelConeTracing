#include "DX12Buffer.h"

//////////////////////////////////////
////////// DX12Buffer ////////////////
//////////////////////////////////////


DX12Buffer::DX12Buffer(DX12DescriptorHeapManager* _descriptorHeapManager,
	Description& description,
	UINT numberOfCopys,
	LPCWSTR name)

	: mDescription(description),
	mCBVMappedData(nullptr),
	number_of_copys(numberOfCopys)
{
	mBufferSize = mDescription.mNumElements * mDescription.mElementSize;

	if (mDescription.mDescriptorType & DescriptorType::CBV)
	{
		mBufferSize = Align(mBufferSize, 256);
	}

	CreateResources(_descriptorHeapManager, _commandList);

	mBuffer->SetName(name);
}

DX12Buffer::~DX12Buffer()
{
	if (mCBVMappedData)
	{
		mBuffer->Unmap(0, nullptr);
	}

	mCBVMappedData = nullptr;
}

void DX12Buffer::CreateResources(DX12DescriptorHeapManager* _descriptorHeapManager)
{
	ID3D12Device* _d3dDevice = DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice();
	D3D12_DESCRIPTOR_HEAP_FLAGS flagsHeap = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (mDescription.mDescriptorType & DescriptorType::SRV)
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Alignment = mDescription.mAlignment;
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Flags = mDescription.mResourceFlags;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Height = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Width = (UINT64)(mBufferSize * number_of_copys);

		D3D12_HEAP_PROPERTIES heapProperties;
		heapProperties.Type = (mDescription.mDescriptorType & DescriptorType::CBV) ? D3D12_HEAP_TYPE_UPLOAD : mDescription.mHeapType;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;

		ThrowIfFailed(
			_d3dDevice->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				mDescription.mState,
				nullptr,
				IID_PPV_ARGS(&mBuffer)
			)
		);

		mDescriptorSRV = _descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, number_of_copys);
		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = mDescription.mFormat;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.NumElements = mDescription.mNumElements;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		if (mDescription.mDescriptorType & DX12Buffer::DescriptorType::Structured)
		{
			srvDesc.Buffer.StructureByteStride = mDescription.mElementSize;
		}
		else 
		{
			if (mDescription.mDescriptorType & DX12Buffer::DescriptorType::Raw)
			{
				srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			}
		}

		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle = mDescriptorSRV.GetCPUHandle();
		D3D12_GPU_VIRTUAL_ADDRESS resourceGPUAddress = mBuffer->GetGPUVirtualAddress();
		auto descriptorIncrementSize = DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for(UINT i = 0; i < mDescriptorSRV.GetBlockSize(); i++)
		{
			srvDesc.Buffer.FirstElement = (UINT64)(i * mDescription.mNumElements);
			_d3dDevice->CreateShaderResourceView(mBuffer.Get(), &srvDesc, mDescriptorSRV.GetCPUHandle());
			cpuDescriptorHandle.ptr += descriptorIncrementSize;
		}
	}

	if (mDescription.mDescriptorType & DescriptorType::CBV)
	{
		CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(number_of_copys * mBufferSize);
		ThrowIfFailed(_d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&constantBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mBuffer)));

		// Create constant buffer views to access the upload buffer.
		D3D12_GPU_VIRTUAL_ADDRESS cbvGpuAddress = mBuffer->GetGPUVirtualAddress();
		mDescriptorCBV = _descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, number_of_copys);
		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle = mDescriptorCBV.GetCPUHandle();
		for (int i = 0; i < mDescriptorCBV.GetBlockSize(); i++)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
			desc.BufferLocation = cbvGpuAddress;
			desc.SizeInBytes = mBufferSize;
			_d3dDevice->CreateConstantBufferView(&desc, cpuDescriptorHandle);

			cbvGpuAddress += desc.SizeInBytes;
			cpuDescriptorHandle.Offset(mDescriptorCBV.GetDescriptorSize());
		}

		
		//D3D12_GPU_VIRTUAL_ADDRESS resourceGPUAddress = mBuffer->GetGPUVirtualAddress();
		//auto descriptorIncrementSize = DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		//// Describe and create a constant buffer view.
		//D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		//cbvDesc.SizeInBytes = mBufferSize; // CB size is required to be 256-byte aligned.
		//for (int i = 0; i < mDescriptorCBV.GetBlockSize(); i++)
		//{
		//	cbvDesc.BufferLocation = resourceGPUAddress;
		//	_d3dDevice->CreateConstantBufferView(&cbvDesc, cpuDescriptorHandle);
		//	cpuDescriptorHandle.ptr += descriptorIncrementSize;
		//	resourceGPUAddress += cbvDesc.SizeInBytes;
		//}

		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(mBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mCBVMappedData)));
	}
}

unsigned char* DX12Buffer::GetMappedData(UINT copyIndex)
{

	return mCBVMappedData + (copyIndex * mDescription.mElementSize);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12Buffer::GetSRV(UINT copyIndex)
{ 
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle = mDescriptorSRV.GetCPUHandle();
	cpuDescriptorHandle.Offset(copyIndex * mDescriptorSRV.GetDescriptorSize());
	return cpuDescriptorHandle;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12Buffer::GetCBV(UINT copyIndex)
{ 

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle = mDescriptorCBV.GetCPUHandle();
	cpuDescriptorHandle.Offset(copyIndex * mDescriptorCBV.GetDescriptorSize());
	return cpuDescriptorHandle;
}