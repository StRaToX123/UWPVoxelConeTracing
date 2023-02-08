#pragma once

#include "Graphics/DirectXTK12/Inc/DirectXHelpers.h"
#include "Graphics/DirectX/DX12DeviceResourcesSingleton.h"

using namespace Microsoft::WRL;

class DX12DescriptorHandleBlock
{
	public:
		DX12DescriptorHandleBlock()
		{
			mCPUHandle.ptr = NULL;
			mGPUHandle.ptr = NULL;
			mHeapIndex = 0;
			block_size = 0;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE& GetCPUHandle() { return mCPUHandle; }
		D3D12_GPU_DESCRIPTOR_HANDLE& GetGPUHandle() { return mGPUHandle; }
		UINT GetHeapIndex() const { return mHeapIndex; }
		UINT GetBlockSize() const { return block_size; }

		void SetCPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) { mCPUHandle = cpuHandle; }
		void SetGPUHandle(D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) { mGPUHandle = gpuHandle; }
		void SetHeapIndex(UINT heapIndex) { mHeapIndex = heapIndex; }
		void SetBlockSize(UINT blockSize) { block_size = blockSize; };
		 
		bool IsValid() { return mCPUHandle.ptr != NULL; }
		bool IsReferencedByShader() { return mGPUHandle.ptr != NULL; }

	private:
		friend class DX12DescriptorHeapBase;
		D3D12_CPU_DESCRIPTOR_HANDLE mCPUHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE mGPUHandle;
		UINT mHeapIndex;
		UINT block_size;
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
		virtual DX12DescriptorHandleBlock GetHandleBlock(UINT count = 1) = 0;


		void AddToHandle(ID3D12Device* device, DX12DescriptorHandleBlock& destCPUHandle, DX12DescriptorHandleBlock& sourceCPUHandle)
		{
			device->CopyDescriptorsSimple(1, destCPUHandle.GetCPUHandle(), sourceCPUHandle.GetCPUHandle(), mHeapType);
			destCPUHandle.GetCPUHandle().ptr += mDescriptorSize;
		}

		void AddToHandle(ID3D12Device* device, DX12DescriptorHandleBlock& destCPUHandle, D3D12_CPU_DESCRIPTOR_HANDLE& sourceCPUHandle)
		{
			device->CopyDescriptorsSimple(1, destCPUHandle.GetCPUHandle(), sourceCPUHandle, mHeapType);
			destCPUHandle.GetCPUHandle().ptr += mDescriptorSize;
		}

		void Reset();

	protected:
		ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
		D3D12_DESCRIPTOR_HEAP_TYPE mHeapType;
		D3D12_CPU_DESCRIPTOR_HANDLE mDescriptorHeapCPUStart;
		D3D12_GPU_DESCRIPTOR_HANDLE mDescriptorHeapGPUStart;
		UINT mMaxNumDescriptors;
		UINT mDescriptorSize;
		bool mIsReferencedByShader;
		UINT mCurrentDescriptorIndex;
};

class DX12DescriptorHeapCPU : public DX12DescriptorHeapBase
{
	public:
		DX12DescriptorHeapCPU(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
		~DX12DescriptorHeapCPU() final;

		DX12DescriptorHandleBlock GetHandleBlock(UINT count = 1) override;
	private:
		std::vector<UINT> mFreeDescriptors; // Descriptor deletion not implemented
		
};

class DX12DescriptorHeapGPU : public DX12DescriptorHeapBase
{
	public:
		DX12DescriptorHeapGPU(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
		~DX12DescriptorHeapGPU() final {};

		DX12DescriptorHandleBlock GetHandleBlock(UINT count = 1) override;
};

class DX12DescriptorHeapManager
{
	public:
		DX12DescriptorHeapManager();
		~DX12DescriptorHeapManager();

		void Initialize(ID3D12Device* device);
		DX12DescriptorHandleBlock CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count = 1);
		DX12DescriptorHandleBlock CreateGPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count = 1);

		DX12DescriptorHeapGPU* GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
		{
			return mGPUDescriptorHeaps[DX12DeviceResourcesSingleton::mBackBufferIndex][heapType];
		}

	private:
		DX12DescriptorHeapCPU* mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		DX12DescriptorHeapGPU* mGPUDescriptorHeaps[DX12DeviceResourcesSingleton::MAX_BACK_BUFFER_COUNT][D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

};


