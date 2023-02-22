#pragma once

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
			UINT num_of_elements;
			union
			{
				UINT element_size;
				UINT size;
			};

			UINT64 alignment;
			DXGI_FORMAT format;
			UINT descriptor_type;
			D3D12_RESOURCE_FLAGS resource_flags;
			D3D12_RESOURCE_STATES state;
			D3D12_HEAP_TYPE heap_type;
			UINT64 counter_offset;

			Description() :
				num_of_elements(1),
				element_size(0),
				alignment(0),
				descriptor_type(DX12Buffer::DescriptorType::SRV),
				format(DXGI_FORMAT_UNKNOWN),
				resource_flags(D3D12_RESOURCE_FLAG_NONE),
				state(D3D12_RESOURCE_STATE_COMMON),
				heap_type(D3D12_HEAP_TYPE_DEFAULT),
				counter_offset(UINT64_MAX)

			{

			}
		};

		DX12Buffer(DX12DescriptorHeapManager* _descriptorHeapManager,
			Description& aDescription,
			UINT numberOfCopys = 1,
			LPCWSTR name = nullptr,
			void* _data = nullptr,
			LONG_PTR dataSize = 0,
			ID3D12GraphicsCommandList* _commandList = nullptr);
		DX12Buffer() {}
		virtual ~DX12Buffer();

		ID3D12Resource* GetResource() { return buffer.Get(); }
		Description& GetDescription() { return description; }
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetSRV(UINT copyIndex = 0);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetUAV(UINT copyIndex = 0);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCBV(UINT copyIndex = 0);
		unsigned char* GetMappedData(UINT copyIndex = 0);
		void TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers,
			ID3D12GraphicsCommandList* commandList,
			D3D12_RESOURCE_STATES stateAfter);

	protected:
		Description description;

		D3D12_RESOURCE_STATES resource_state_current;

		UINT buffer_size_in_bytes;
		UINT8* _buffer_mapped_data;

		ComPtr<ID3D12Resource> buffer;
		ComPtr<ID3D12Resource> buffer_upload;

		DX12DescriptorHandleBlock descriptor_handle_block_cbv;
		DX12DescriptorHandleBlock descriptor_handle_block_srv;
		DX12DescriptorHandleBlock descriptor_handle_block_uav;

		UINT number_of_copys;

		virtual void CreateResources(DX12DescriptorHeapManager* _descriptorHeapManager, 
			void* _data = nullptr,
			LONG_PTR dataSize = 0,
			ID3D12GraphicsCommandList* _commandList = nullptr);
};

