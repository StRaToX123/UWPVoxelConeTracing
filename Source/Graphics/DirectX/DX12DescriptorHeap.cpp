#include "DX12DescriptorHeap.h"

/////////////////////////////////////////////////
////////// DX12DescriptorHandleBlock ////////////
/////////////////////////////////////////////////

DX12DescriptorHandleBlock::DX12DescriptorHandleBlock()
{
	gpu_handle.ptr = NULL;
	block_size = 0;
	unassigned_descriptor_block_index = 0;
	do_not_run_destructor = false;
}

DX12DescriptorHandleBlock::DX12DescriptorHandleBlock(DX12DescriptorHandleBlock& other)
{
	gpu_handle.ptr = other.gpu_handle.ptr;
	block_size = other.block_size;
	unassigned_descriptor_block_index = 0;
	heap_type = other.heap_type;
	owner_cpu_handle_indexes = std::move(other.owner_cpu_handle_indexes);
	_owner_heap = other._owner_heap;
	do_not_run_destructor = false;
	other.do_not_run_destructor = true;
}

DX12DescriptorHandleBlock::DX12DescriptorHandleBlock(DX12DescriptorHandleBlock&& other)
{
	gpu_handle.ptr = other.gpu_handle.ptr;
	block_size = other.block_size;
	unassigned_descriptor_block_index = 0;
	heap_type = other.heap_type;
	owner_cpu_handle_indexes = std::move(other.owner_cpu_handle_indexes);
	_owner_heap = other._owner_heap;
	do_not_run_destructor = false;
	other.do_not_run_destructor = true;
}

DX12DescriptorHandleBlock& DX12DescriptorHandleBlock::operator=(DX12DescriptorHandleBlock&& other)
{
	gpu_handle.ptr = other.gpu_handle.ptr;
	block_size = other.block_size;
	unassigned_descriptor_block_index = 0;
	heap_type = other.heap_type;
	owner_cpu_handle_indexes = std::move(other.owner_cpu_handle_indexes);
	_owner_heap = other._owner_heap;
	do_not_run_destructor = false;
	other.do_not_run_destructor = true;
	return *this;
}

DX12DescriptorHandleBlock::~DX12DescriptorHandleBlock()
{
	if (do_not_run_destructor == false)
	{
		// We have to return the cpu handles back to the heap manager
		for (UINT i = 0; i < block_size; i++)
		{
			_owner_heap->available_descriptor_indexes.push(owner_cpu_handle_indexes[i]);
		}
	}
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
	descriptor_heap_cpu_start = descriptor_heap->GetCPUDescriptorHandleForHeapStart();
	if (is_referenced_by_shader)
	{
		descriptor_heap_gpu_start = descriptor_heap->GetGPUDescriptorHandleForHeapStart();
	}

	descriptor_size = device->GetDescriptorHandleIncrementSize(heap_type);

	
	for (UINT i = 0; i < max_num_descriptors; i++)
	{
		available_descriptor_indexes_default_value.push(i);
	}

	available_descriptor_indexes = available_descriptor_indexes_default_value;
}

void DX12DescriptorHeapBase::Reset()
{
	available_descriptor_indexes = available_descriptor_indexes_default_value;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapBase::GetHeapCPUStart()
{ 
	return descriptor_heap_cpu_start; 
}
CD3DX12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeapBase::GetHeapGPUStart()
{ 
	return descriptor_heap_gpu_start; 
}

/////////////////////////////////////////////
////////// DX12DescriptorHeapCPU ////////////
/////////////////////////////////////////////

DX12DescriptorHeapCPU::DX12DescriptorHeapCPU(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
	: DX12DescriptorHeapBase(device, heapType, numDescriptors, false)
{
	
}



DX12DescriptorHandleBlock DX12DescriptorHeapCPU::GetHandleBlock(UINT count)
{
	if (count > available_descriptor_indexes.size())
	{
		std::exception("Ran out of dynamic descriptor heap handles, need to increase heap size.");
	}

	DX12DescriptorHandleBlock newHandleBlock;
	newHandleBlock.owner_cpu_handle_indexes.reserve(count);
	for (UINT i = 0; i < count; i++)
	{
		newHandleBlock.owner_cpu_handle_indexes.emplace_back(available_descriptor_indexes.front());
		available_descriptor_indexes.pop();
	}
	
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
	
}

DX12DescriptorHandleBlock DX12DescriptorHeapGPU::GetHandleBlock(UINT count)
{
	if (count > available_descriptor_indexes.size())
	{
		std::exception("Ran out of GPU descriptor heap handles, need to increase heap size.");

	}

	DX12DescriptorHandleBlock newHandleBlock;
	newHandleBlock.owner_cpu_handle_indexes.reserve(count);
	for (UINT i = 0; i < count; i++)
	{
		newHandleBlock.owner_cpu_handle_indexes.emplace_back(available_descriptor_indexes.front());
		available_descriptor_indexes.pop();
	}

	newHandleBlock.block_size = count;
	newHandleBlock.heap_type = heap_type;
	newHandleBlock._owner_heap = this;

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = descriptor_heap_gpu_start;
	gpuHandle.ptr += newHandleBlock.owner_cpu_handle_indexes[0] * descriptor_size;
	newHandleBlock.gpu_handle = gpuHandle;

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
	ZeroMemory(P_descriptor_heaps_cpu, sizeof(P_descriptor_heaps_cpu));

	static const int MaxNoofSRVDescriptors = 4 * 4096;

	P_descriptor_heaps_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new DX12DescriptorHeapCPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxNoofSRVDescriptors);
	P_descriptor_heaps_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = new DX12DescriptorHeapCPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 128);
	P_descriptor_heaps_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = new DX12DescriptorHeapCPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128);
	P_descriptor_heaps_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = new DX12DescriptorHeapCPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16);

	for (UINT i = 0; i < DX12DeviceResourcesSingleton::MAX_BACK_BUFFER_COUNT; i++)
	{
		ZeroMemory(P_descriptor_heaps_gpu[i], sizeof(P_descriptor_heaps_gpu[i]));
		P_descriptor_heaps_gpu[i][D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new DX12DescriptorHeapGPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxNoofSRVDescriptors);
		P_descriptor_heaps_gpu[i][D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = new DX12DescriptorHeapGPU(_d3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16);
	}
}

DX12DescriptorHeapManager::~DX12DescriptorHeapManager()
{
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
	{
		if (P_descriptor_heaps_cpu[i])
		{
			delete P_descriptor_heaps_cpu[i];

		}

		for (UINT j = 0; j < DX12DeviceResourcesSingleton::MAX_BACK_BUFFER_COUNT; j++)
		{
			if (P_descriptor_heaps_gpu[j][i])
			{
				delete P_descriptor_heaps_gpu[j][i];
			}
		}
	}
}

DX12DescriptorHandleBlock DX12DescriptorHeapManager::GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count)
{
	return P_descriptor_heaps_cpu[heapType]->GetHandleBlock(count);
}

DX12DescriptorHandleBlock DX12DescriptorHeapManager::GetGPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count)
{
	return P_descriptor_heaps_gpu[DX12DeviceResourcesSingleton::back_buffer_index][heapType]->GetHandleBlock(count);
}
