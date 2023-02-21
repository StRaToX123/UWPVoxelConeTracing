#pragma once
#include "Common.h"
#include "Graphics\DirectX\DX12DescriptorHeap.h"
#include <string>

class DX12Buffer
{
	public:
		struct DescriptorType
		{
			enum Enum
			{
				SRV = 1 << 0,
				CBV = 1 << 1,
				UAV = 1 << 2,
				Structured = 1 << 3,
				Raw = 1 << 4,
			};
		};

		struct Description
		{
			UINT mNumElements;
			union
			{
				UINT mElementSize;
				UINT mSize;
			};
			UINT64 mAlignment;
			DXGI_FORMAT mFormat;
			UINT mDescriptorType;
			D3D12_RESOURCE_FLAGS mResourceFlags;
			D3D12_RESOURCE_STATES mState;
			D3D12_HEAP_TYPE mHeapType;

			Description() :
				mNumElements(1)
				, mElementSize(0)
				, mAlignment(0)
				, mDescriptorType(DX12Buffer::DescriptorType::SRV)
				, mFormat(DXGI_FORMAT_UNKNOWN)
				, mResourceFlags(D3D12_RESOURCE_FLAG_NONE)
				, mState(D3D12_RESOURCE_STATE_COMMON)
				, mHeapType(D3D12_HEAP_TYPE_DEFAULT)
			{}
		};

		DX12Buffer(DX12DescriptorHeapManager* _descriptorHeapManager,
			Description& description,
			UINT numberOfCopys = 1,
			LPCWSTR name = nullptr);
		DX12Buffer() {}
		virtual ~DX12Buffer();

		ID3D12Resource* GetResource() { return buffer.Get(); }
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetSRV(UINT copyIndex = 0);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCBV(UINT copyIndex = 0);
		unsigned char* GetMappedData(UINT copyIndex = 0);

	protected:
		Description mDescription;

		UINT buffer_size_in_bytes;
		UINT8* _buffer_mapped_data;

		ComPtr<ID3D12Resource> buffer;
		ComPtr<ID3D12Resource> buffer_upload;

		DX12DescriptorHandleBlock descriptor_handle_block_cbv;
		DX12DescriptorHandleBlock descriptor_handle_block_srv;

		UINT number_of_copys;

		virtual void CreateResources(DX12DescriptorHeapManager* _descriptorHeapManager);
};

