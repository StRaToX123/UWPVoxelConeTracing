#include "DX12DescriptorHeap.h"


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

DX12DescriptorHeapCPU::DX12DescriptorHeapCPU(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
	: DX12DescriptorHeapBase(device, heapType, numDescriptors, false)
{
	mCurrentDescriptorIndex = 0;
	mActiveHandleCount = 0;
}

DX12DescriptorHeapCPU::~DX12DescriptorHeapCPU()
{
	mFreeDescriptors.clear();
}

DX12DescriptorHandle DX12DescriptorHeapCPU::GetNewHandle()
{
	UINT newHandleID = 0;

	if (mCurrentDescriptorIndex < mMaxNumDescriptors)
	{
		newHandleID = mCurrentDescriptorIndex;
		mCurrentDescriptorIndex++;
	}
	else if (mFreeDescriptors.size() > 0)
	{
		newHandleID = mFreeDescriptors.back();
		mFreeDescriptors.pop_back();
	}
	else
	{
		std::exception("Ran out of dynamic descriptor heap handles, need to increase heap size.");
	}

	DX12DescriptorHandle newHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mDescriptorHeapCPUStart;
	cpuHandle.ptr += newHandleID * mDescriptorSize;
	newHandle.SetCPUHandle(cpuHandle);
	newHandle.SetHeapIndex(newHandleID);
	mActiveHandleCount++;

	return newHandle;
}

void DX12DescriptorHeapCPU::FreeHandle(DX12DescriptorHandle handle)
{
	mFreeDescriptors.push_back(handle.GetHeapIndex());

	if (mActiveHandleCount == 0)
	{
		std::exception("Freeing heap handles when there should be none left");
	}
	mActiveHandleCount--;
}

DX12DescriptorHeapGPU::DX12DescriptorHeapGPU(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
	: DX12DescriptorHeapBase(device, heapType, numDescriptors, true)
{
	mCurrentDescriptorIndex = 0;
}

DX12DescriptorHandle DX12DescriptorHeapGPU::GetHandleBlock(UINT count)
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

	DX12DescriptorHandle newHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mDescriptorHeapCPUStart;
	cpuHandle.ptr += newHandleID * mDescriptorSize;
	newHandle.SetCPUHandle(cpuHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = mDescriptorHeapGPUStart;
	gpuHandle.ptr += newHandleID * mDescriptorSize;
	newHandle.SetGPUHandle(gpuHandle);

	newHandle.SetHeapIndex(newHandleID);

	return newHandle;
}

void DX12DescriptorHeapGPU::Reset()
{
	mCurrentDescriptorIndex = 0;
}

DX12DescriptorHeapManager::DX12DescriptorHeapManager()
{
	
}

void DX12DescriptorHeapManager::Initialize(ID3D12Device* device)
{
	ZeroMemory(mCPUDescriptorHeaps, sizeof(mCPUDescriptorHeaps));

	static const int MaxNoofSRVDescriptors = 4 * 4096;

	mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new DX12DescriptorHeapCPU(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxNoofSRVDescriptors);
	mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = new DX12DescriptorHeapCPU(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 128);
	mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = new DX12DescriptorHeapCPU(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128);
	mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = new DX12DescriptorHeapCPU(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16);

	for (UINT i = 0; i < DXRSGraphics::MAX_BACK_BUFFER_COUNT; i++)
	{
		ZeroMemory(mGPUDescriptorHeaps[i], sizeof(mGPUDescriptorHeaps[i]));
		mGPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new DX12DescriptorHeapGPU(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxNoofSRVDescriptors);
		mGPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = new DX12DescriptorHeapGPU(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16);
	}
}

DX12DescriptorHeapManager::~DX12DescriptorHeapManager()
{
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
	{
		if (mCPUDescriptorHeaps[i])
			delete mCPUDescriptorHeaps[i];

		for (UINT j = 0; j < DXRSGraphics::MAX_BACK_BUFFER_COUNT; j++)
		{
			if (mGPUDescriptorHeaps[j][i])
				delete mGPUDescriptorHeaps[j][i];
		}
	}
}

DX12DescriptorHandle DX12DescriptorHeapManager::CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
	const UINT currentFrame = DXRSGraphics::mBackBufferIndex;

	return mCPUDescriptorHeaps[heapType]->GetNewHandle();
}

DX12DescriptorHandle DX12DescriptorHeapManager::CreateGPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count)
{
	const UINT currentFrame = DXRSGraphics::mBackBufferIndex;

	return mGPUDescriptorHeaps[currentFrame][heapType]->GetHandleBlock(count);
}
