#pragma once

#include "Graphics/DirectXTK12/Inc/DirectXHelpers.h"
#include "Graphics/DirectX/DXRSGraphics.h"

using namespace Microsoft::WRL;

class DX12DescriptorHandle
{
	public:
		DX12DescriptorHandle()
		{
			mCPUHandle.ptr = NULL;
			mGPUHandle.ptr = NULL;
			mHeapIndex = 0;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE& GetCPUHandle() { return mCPUHandle; }
		D3D12_GPU_DESCRIPTOR_HANDLE& GetGPUHandle() { return mGPUHandle; }
		UINT GetHeapIndex() { return mHeapIndex; }

		void SetCPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) { mCPUHandle = cpuHandle; }
		void SetGPUHandle(D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) { mGPUHandle = gpuHandle; }
		void SetHeapIndex(UINT heapIndex) { mHeapIndex = heapIndex; }

		bool IsValid() { return mCPUHandle.ptr != NULL; }
		bool IsReferencedByShader() { return mGPUHandle.ptr != NULL; }

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE mCPUHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE mGPUHandle;
		UINT mHeapIndex;
};

class DX12DescriptorHeapBase
{
	public:
		DX12DescriptorHeapBase(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool isReferencedByShader = false);
		virtual ~DX12DescriptorHeapBase();

		ID3D12DescriptorHeap* GetHeap() { return mDescriptorHeap.Get(); }
		D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() { return mHeapType; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetHeapCPUStart() { return mDescriptorHeapCPUStart; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetHeapGPUStart() { return mDescriptorHeapGPUStart; }
		UINT GetMaxNoofDescriptors() { return mMaxNumDescriptors; }
		UINT GetDescriptorSize() { return mDescriptorSize; }

		void AddToHandle(ID3D12Device* device, DX12DescriptorHandle& destCPUHandle, DX12DescriptorHandle& sourceCPUHandle)
		{
			device->CopyDescriptorsSimple(1, destCPUHandle.GetCPUHandle(), sourceCPUHandle.GetCPUHandle(), mHeapType);
			destCPUHandle.GetCPUHandle().ptr += mDescriptorSize;
		}

		void AddToHandle(ID3D12Device* device, DX12DescriptorHandle& destCPUHandle, D3D12_CPU_DESCRIPTOR_HANDLE& sourceCPUHandle)
		{
			device->CopyDescriptorsSimple(1, destCPUHandle.GetCPUHandle(), sourceCPUHandle, mHeapType);
			destCPUHandle.GetCPUHandle().ptr += mDescriptorSize;
		}

	protected:
		ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
		D3D12_DESCRIPTOR_HEAP_TYPE mHeapType;
		D3D12_CPU_DESCRIPTOR_HANDLE mDescriptorHeapCPUStart;
		D3D12_GPU_DESCRIPTOR_HANDLE mDescriptorHeapGPUStart;
		UINT mMaxNumDescriptors;
		UINT mDescriptorSize;
		bool mIsReferencedByShader;
};

class DX12DescriptorHeapCPU : public DX12DescriptorHeapBase
{
	public:
		DX12DescriptorHeapCPU(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
		~DX12DescriptorHeapCPU() final;

		DX12DescriptorHandle GetNewHandle();
		void FreeHandle(DX12DescriptorHandle handle);

	private:
		std::vector<UINT> mFreeDescriptors;
		UINT mCurrentDescriptorIndex;
		UINT mActiveHandleCount;
};

class DX12DescriptorHeapGPU : public DX12DescriptorHeapBase
{
	public:
		DX12DescriptorHeapGPU(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
		~DX12DescriptorHeapGPU() final {};

		void Reset();
		DX12DescriptorHandle GetHandleBlock(UINT count);

	private:
		UINT mCurrentDescriptorIndex;
};

class DX12DescriptorHeapManager
{
	public:
		DX12DescriptorHeapManager();
		~DX12DescriptorHeapManager();

		void Initialize(ID3D12Device* device);
		DX12DescriptorHandle CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType);
		DX12DescriptorHandle CreateGPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count);

		DX12DescriptorHeapGPU* GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
		{
			return mGPUDescriptorHeaps[DXRSGraphics::mBackBufferIndex][heapType];
		}

	private:
		DX12DescriptorHeapCPU* mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		DX12DescriptorHeapGPU* mGPUDescriptorHeaps[DXRSGraphics::MAX_BACK_BUFFER_COUNT][D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

};


