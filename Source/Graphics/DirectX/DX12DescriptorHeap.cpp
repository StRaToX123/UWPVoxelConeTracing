#include "DX12DescriptorHeap.h"

/////////////////////////////////////////////////
////////// DX12DescriptorHandleBlock ////////////
/////////////////////////////////////////////////

void DX12DescriptorHandleBlock::Add(DX12DescriptorHandleBlock& sourceCPUHandleBlock)
{
	if (sourceCPUHandleBlock.GetBlockSize() > (block_size - unassigned_descriptor_block_index))
	{
		throw std::exception("Exceeded block size");
	}

	DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice()->CopyDescriptorsSimple(sourceCPUHandleBlock.GetBlockSize(), CD3DX12_CPU_DESCRIPTOR_HANDLE(mCPUHandle, unassigned_descriptor_block_index, descriptor_size), sourceCPUHandleBlock.GetCPUHandle(), heap_type);
	unassigned_descriptor_block_index += sourceCPUHandleBlock.GetBlockSize();
}

void DX12DescriptorHandleBlock::Add(D3D12_CPU_DESCRIPTOR_HANDLE& sourceCPUHandle)
{
	if (unassigned_descriptor_block_index == block_size)
	{
		throw std::exception("Exceeded block size");
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCPUHandle, unassigned_descriptor_block_index, descriptor_size);
	DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice()->CopyDescriptorsSimple(1, CD3DX12_CPU_DESCRIPTOR_HANDLE(mCPUHandle, unassigned_descriptor_block_index, descriptor_size), sourceCPUHandle, heap_type);
	unassigned_descriptor_block_index += 1;
}


//////////////////////////////////////////////
////////// DX12DescriptorHeapBase ////////////
//////////////////////////////////////////////

DX12DescriptorHeapBase::DX12DescriptorHeapBase(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool isReferencedByShader)
	: mHeapType(heapType)
	, mMaxNumDescriptors(numDescriptors)
	, mIsReferencedByShader(isReferencedByShader)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	heapDesc.NumDescriptors = mMaxNumDescriptors;
	heapDesc.Type = mHeapType;
	heapDesc.Flags = mIsReferencedByShader ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heapDesc.NodeMask = 0;

	ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDescriptorHeap)));

	mDescriptorHeapCPUStart = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	if (mIsReferencedByShader)
	{
		mDescriptorHeapGPUStart = mDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	}

	mDescriptorSize = device->GetDescriptorHandleIncrementSize(mHeapType);
}

DX12DescriptorHeapBase::~DX12DescriptorHeapBase()
{

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

DX12DescriptorHeapCPU::~DX12DescriptorHeapCPU()
{
	mFreeDescriptors.clear();
}

DX12DescriptorHandleBlock DX12DescriptorHeapCPU::GetHandleBlock(UINT count)
{
	UINT newHandleID = 0;
	UINT blockEnd = mCurrentDescriptorIndex + count;
	if (blockEnd < mMaxNumDescriptors)
	{
		newHandleID = mCurrentDescriptorIndex;
		mCurrentDescriptorIndex = blockEnd;
	}
	/*else if (mFreeDescriptors.size() >= count)
	{
		newHandleID = mFreeDescriptors.back();
		mFreeDescriptors.pop_back();
	}*/
	else
	{
		std::exception("Ran out of dynamic descriptor heap handles, need to increase heap size.");
	}

	DX12DescriptorHandleBlock newHandleBlock;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mDescriptorHeapCPUStart;
	cpuHandle.ptr += newHandleID * mDescriptorSize;
	newHandleBlock.SetCPUHandle(cpuHandle);
	newHandleBlock.SetHeapIndex(newHandleID);
	newHandleBlock.SetBlockSize(count);
	newHandleBlock.SetDescriptorSize(mDescriptorSize);
	newHandleBlock.SetHeapType(mHeapType);

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

	if (blockEnd < mMaxNumDescriptors)
	{
		newHandleID = mCurrentDescriptorIndex;
		mCurrentDescriptorIndex = blockEnd;
	}
	else
	{
		std::exception("Ran out of GPU descriptor heap handles, need to increase heap size.");
	}

	DX12DescriptorHandleBlock newHandleBlock;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mDescriptorHeapCPUStart;
	cpuHandle.ptr += newHandleID * mDescriptorSize;
	newHandleBlock.SetCPUHandle(cpuHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = mDescriptorHeapGPUStart;
	gpuHandle.ptr += newHandleID * mDescriptorSize;
	newHandleBlock.SetGPUHandle(gpuHandle);

	newHandleBlock.SetHeapIndex(newHandleID);
	newHandleBlock.SetBlockSize(count);
	newHandleBlock.SetDescriptorSize(mDescriptorSize);
	newHandleBlock.SetHeapType(mHeapType);

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
			delete mCPUDescriptorHeaps[i];

		for (UINT j = 0; j < DX12DeviceResourcesSingleton::MAX_BACK_BUFFER_COUNT; j++)
		{
			if (mGPUDescriptorHeaps[j][i])
				delete mGPUDescriptorHeaps[j][i];
		}
	}
}

DX12DescriptorHandleBlock DX12DescriptorHeapManager::CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count)
{
	const UINT currentFrame = DX12DeviceResourcesSingleton::mBackBufferIndex;
	return mCPUDescriptorHeaps[heapType]->GetHandleBlock(count);
}

DX12DescriptorHandleBlock DX12DescriptorHeapManager::CreateGPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count)
{
	const UINT currentFrame = DX12DeviceResourcesSingleton::mBackBufferIndex;
	return mGPUDescriptorHeaps[currentFrame][heapType]->GetHandleBlock(count);
}
