#include "DX12DescriptorHeap.h"

/////////////////////////////////////////////////
////////// DX12DescriptorHandleBlock ////////////
/////////////////////////////////////////////////

DX12DescriptorHandleBlock::DX12DescriptorHandleBlock()
{
	gpu_handle.ptr = NULL;
	owner_heap_index = 0;
	block_size = 0;
	unassigned_descriptor_block_index = 0;
}

DX12DescriptorHandleBlock::~DX12DescriptorHandleBlock()
{
	// We have to return the cpu handles back to the heap manager
}

void DX12DescriptorHandleBlock::Add(DX12DescriptorHandleBlock& sourceCPUHandleBlock)
{
	if (sourceCPUHandleBlock.GetBlockSize() > (block_size - unassigned_descriptor_block_index))
	{
		throw std::exception("Exceeded block size");
	}

	for (UINT i = 0; i < sourceCPUHandleBlock.GetBlockSize(); i++)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE destinationCPUHandle = _owner_heap->GetHeapCPUStart();
		destinationCPUHandle.Offset(owner_cpu_handle_indexes[unassigned_descriptor_block_index] * GetDescriptorSize());
		DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice()->CopyDescriptorsSimple(1, destinationCPUHandle, sourceCPUHandleBlock.GetCPUHandle(i), heap_type);
		unassigned_descriptor_block_index++;
	}
}

void DX12DescriptorHandleBlock::Add(D3D12_CPU_DESCRIPTOR_HANDLE& sourceCPUHandle)
{
	if (unassigned_descriptor_block_index == block_size)
	{
		throw std::exception("Exceeded block size");
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE destinationCPUHandle = _owner_heap->GetHeapCPUStart();
	destinationCPUHandle.Offset(owner_cpu_handle_indexes[unassigned_descriptor_block_index] * GetDescriptorSize());
	DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice()->CopyDescriptorsSimple(1, destinationCPUHandle, sourceCPUHandle, heap_type);
	unassigned_descriptor_block_index++;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHandleBlock::GetCPUHandle(UINT blockIndex)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE result = _owner_heap->GetHeapCPUStart();
	result.Offset(owner_cpu_handle_indexes[blockIndex] * GetDescriptorSize());
	return result;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHandleBlock::GetGPUHandle()
{
	return gpu_handle;
}

UINT DX12DescriptorHandleBlock::GetDescriptorSize()
{
	return _owner_heap->GetDescriptorSize();
}

UINT DX12DescriptorHandleBlock::GetHeapIndex() const
{
	return owner_heap_index;
};
UINT DX12DescriptorHandleBlock::GetBlockSize() const
{
	return block_size;
}

bool DX12DescriptorHandleBlock::IsReferencedByShader()
{
	return gpu_handle.ptr != NULL ? true : false;
}


//////////////////////////////////////////////
////////// DX12DescriptorHeapBase ////////////
//////////////////////////////////////////////

DX12DescriptorHeapBase::DX12DescriptorHeapBase(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool isReferencedByShader)
	: heap_type(heapType)
	, max_num_descriptors(numDescriptors)
	, is_referenced_by_shader(isReferencedByShader)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	heapDesc.NumDescriptors = max_num_descriptors;
	heapDesc.Type = heap_type;
	heapDesc.Flags = is_referenced_by_shader ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heapDesc.NodeMask = 0;

	ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptor_heap)));
	descriptor_heapCPUStart = descriptor_heap->GetCPUDescriptorHandleForHeapStart();
	if (is_referenced_by_shader)
	{
		descriptor_heapGPUStart = descriptor_heap->GetGPUDescriptorHandleForHeapStart();
	}

	descriptor_size = device->GetDescriptorHandleIncrementSize(heap_type);

	available_descriptor_indexes.reserve(max_num_descriptors);
	for (UINT i = 0; i < max_num_descriptors; i++)
	{
		available_descriptor_indexes.emplace_back(i);
	}
}

void DX12DescriptorHeapBase::Reset()
{
	mCurrentDescriptorIndex = 0;
}

/////////////////////////////////////////////
////////// DX12DescriptorHeapCPU ////////////
/////////////////////////////////////////////

DX12DescriptorHeapCPU::DX12DescriptorHeapCPU(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
	: DX12DescriptorHeapBase(device, heapType, numDescriptors, false)
{
	mCurrentDescriptorIndex = 0;
}



DX12DescriptorHandleBlock DX12DescriptorHeapCPU::GetHandleBlock(UINT count)
{
	UINT newHandleID = 0;
	UINT blockEnd = mCurrentDescriptorIndex + count;
	if (blockEnd < max_num_descriptors)
	{
		newHandleID = mCurrentDescriptorIndex;
		mCurrentDescriptorIndex = blockEnd;
	}
	else
	{
		std::exception("Ran out of dynamic descriptor heap handles, need to increase heap size.");
	}

	DX12DescriptorHandleBlock newHandleBlock;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descriptor_heapCPUStart;
	cpuHandle.ptr += newHandleID * descriptor_size;
	newHandleBlock.SetCPUHandle(cpuHandle);
	newHandleBlock.owner_heap_index = newHandleID;
	newHandleBlock.block_size = count;
	newHandleBlock.heap_type = heap_type;
	newHandleBlock._owner_heap = this;

	return newHandleBlock;
}


/////////////////////////////////////////////
////////// DX12DescriptorHeapGPU ////////////
/////////////////////////////////////////////

DX12DescriptorHeapGPU::DX12DescriptorHeapGPU(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
	: DX12DescriptorHeapBase(device, heapType, numDescriptors, true)
{
	mCurrentDescriptorIndex = 0;
}

DX12DescriptorHandleBlock DX12DescriptorHeapGPU::GetHandleBlock(UINT count)
{
	UINT newHandleID = 0;
	UINT blockEnd = mCurrentDescriptorIndex + count;

	if (blockEnd < max_num_descriptors)
	{
		newHandleID = mCurrentDescriptorIndex;
		mCurrentDescriptorIndex = blockEnd;
	}
	else
	{
		std::exception("Ran out of GPU descriptor heap handles, need to increase heap size.");
	}

	DX12DescriptorHandleBlock newHandleBlock;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descriptor_heapCPUStart;
	cpuHandle.ptr += newHandleID * descriptor_size;
	newHandleBlock.SetCPUHandle(cpuHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = descriptor_heapGPUStart;
	gpuHandle.ptr += newHandleID * descriptor_size;
	newHandleBlock.SetGPUHandle(gpuHandle);

	newHandleBlock.SetHeapIndex(newHandleID);
	newHandleBlock.SetBlockSize(count);
	newHandleBlock.SetDescriptorSize(descriptor_size);
	newHandleBlock.SetHeapType(heap_type);

	return newHandleBlock;
}


/////////////////////////////////////////////////
////////// DX12DescriptorHeapManager ////////////
/////////////////////////////////////////////////

DX12DescriptorHeapManager::DX12DescriptorHeapManager()
{
	
}

void DX12DescriptorHeapManager::Initialize()
{
	ID3D12Device* _d3dDevice = DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice();
	ZeroMemory(mCPUDescriptorHeaps, sizeof(mCPUDescriptorHeaps));

	static const int MaxNoofSRVDescriptors = 4 * 4096;

	mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new DX12DescriptorHeapCPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxNoofSRVDescriptors);
	mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = new DX12DescriptorHeapCPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 128);
	mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = new DX12DescriptorHeapCPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128);
	mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = new DX12DescriptorHeapCPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16);

	for (UINT i = 0; i < DX12DeviceResourcesSingleton::MAX_BACK_BUFFER_COUNT; i++)
	{
		ZeroMemory(mGPUDescriptorHeaps[i], sizeof(mGPUDescriptorHeaps[i]));
		mGPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new DX12DescriptorHeapGPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxNoofSRVDescriptors);
		mGPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = new DX12DescriptorHeapGPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16);
	}
}

DX12DescriptorHeapManager::~DX12DescriptorHeapManager()
{
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
	{
		if (mCPUDescriptorHeaps[i])
		{
			delete mCPUDescriptorHeaps[i];

		}

		for (UINT j = 0; j < DX12DeviceResourcesSingleton::MAX_BACK_BUFFER_COUNT; j++)
		{
			if (mGPUDescriptorHeaps[j][i])
			{
				delete mGPUDescriptorHeaps[j][i];
			}
		}
	}
}

DX12DescriptorHandleBlock DX12DescriptorHeapManager::GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count)
{
	return mCPUDescriptorHeaps[DX12DeviceResourcesSingleton::back_buffer_index]->GetHandleBlock(count);
}

DX12DescriptorHandleBlock DX12DescriptorHeapManager::GetGPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count)
{
	return mGPUDescriptorHeaps[DX12DeviceResourcesSingleton::back_buffer_index][heapType]->GetHandleBlock(count);
}
