#pragma once

#include "Graphics/DirectXTK12/Inc/DirectXHelpers.h"
#include "Graphics/DirectX/DX12DeviceResourcesSingleton.h"

using namespace Microsoft::WRL;

class DX12DescriptorHeapBase;

class DX12DescriptorHandleBlock
{
	public:
		DX12DescriptorHandleBlock();
		~DX12DescriptorHandleBlock();
		// Returns the CPU handle from the block, CPU handles held by this block may not be linearly laid out
		// from inside the heap. GPU handles are always laid out linear inside the heap
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT blockIndex = 0);
		// Returns the start of the heap, GPU descriptor handles inside a block are linear, CPU ones may not be
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUHandle();
		UINT GetHeapIndex() const;
		UINT GetBlockSize() const;
		UINT GetDescriptorSize();
		bool IsReferencedByShader();

		void Add(DX12DescriptorHandleBlock& sourceCPUHandleBlock);
		void Add(D3D12_CPU_DESCRIPTOR_HANDLE& sourceCPUHandle);

	private:
		friend class DX12DescriptorHeapBase;
		friend class DX12DescriptorHeapCPU;
		friend class DX12DescriptorHeapGPU;

		D3D12_DESCRIPTOR_HEAP_TYPE heap_type;
		// Indexes referencing the _owner_heap's cpu descriptor heap
		std::vector<UINT> owner_cpu_handle_indexes;
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_handle;
		UINT owner_heap_index;
		UINT block_size;
		UINT unassigned_descriptor_block_index;
		DX12DescriptorHeapBase* _owner_heap;

};

class DX12DescriptorHeapBase
{
	public:
		DX12DescriptorHeapBase(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool isReferencedByShader = false);

		ID3D12DescriptorHeap* GetHeap() { return descriptor_heap.Get(); }
		D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() { return heap_type; }
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetHeapCPUStart() { return mDescriptorHeapCPUStart; }
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetHeapGPUStart() { return mDescriptorHeapGPUStart; }
		UINT GetMaxNoofDescriptors() { return max_num_descriptors; }
		UINT GetDescriptorSize() { return descriptor_size; }
		virtual DX12DescriptorHandleBlock GetHandleBlock(UINT count = 1) = 0;
		void Reset();

	protected:
		ComPtr<ID3D12DescriptorHeap> descriptor_heap;
		D3D12_DESCRIPTOR_HEAP_TYPE heap_type;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mDescriptorHeapCPUStart;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mDescriptorHeapGPUStart;
		// Which descriptors from the heap are available
		std::vector<UINT> available_descriptor_indexes;
		UINT max_num_descriptors;
		UINT descriptor_size;
		bool is_referenced_by_shader;
};

// The difference between the DX12DescriptorHeapCPU and the DX12DescriptorHeapGPU are the way they fill out 
// the DX12DescriptorHnadleBlocks they hand out
class DX12DescriptorHeapCPU : public DX12DescriptorHeapBase
{
	public:
		DX12DescriptorHeapCPU(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
		DX12DescriptorHandleBlock GetHandleBlock(UINT count = 1) override;
};

class DX12DescriptorHeapGPU : public DX12DescriptorHeapBase
{
	public:
		DX12DescriptorHeapGPU(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
		DX12DescriptorHandleBlock GetHandleBlock(UINT count = 1) override;
};

class DX12DescriptorHeapManager
{
	public:
		DX12DescriptorHeapManager();
		~DX12DescriptorHeapManager();

		void Initialize();
		DX12DescriptorHandleBlock GetCPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count = 1);
		DX12DescriptorHandleBlock GetGPUHandleBlock(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count = 1);

		DX12DescriptorHeapGPU* GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
		{
			return mGPUDescriptorHeaps[DX12DeviceResourcesSingleton::back_buffer_index][heapType];
		}

	private:
		DX12DescriptorHeapCPU* mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		DX12DescriptorHeapGPU* mGPUDescriptorHeaps[DX12DeviceResourcesSingleton::MAX_BACK_BUFFER_COUNT][D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

};


