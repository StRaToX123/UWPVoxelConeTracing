#pragma once


#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <processthreadsapi.h>
#include <handleapi.h>
#include <synchapi.h>
#include <condition_variable>
#include <WinBase.h>
#include <string>
#include "Utility/Debugging/DebugMessage.h"
#include "Graphics/DeviceResources/DeviceResourcesCommons.h"
#include "Graphics/DirectX/DirectXHelper.h"





using namespace std;
using namespace DX;

class PresentThread
{
	public:
		PresentThread();
		~PresentThread();
		bool Initialize(const Microsoft::WRL::ComPtr<ID3D12Device>& d3d12Device,
				            const Microsoft::WRL::ComPtr<IDXGISwapChain3>& swapChain,
			                    const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue);
		void Start();
		void Present();
		// This function is blocking
		UINT GetNextAvailableBackBufferIndex();
		


	private:
		string GetLastErrorAsString();
		static DWORD __stdcall PresentThreadMainFunction(void* data);
		bool keep_looping = false;
		void* p_thread_handle;
		Microsoft::WRL::ComPtr<ID3D12Fence> fence;
		UINT64 fence_value_for_current_gpu_workload_being_waited_on;
		UINT64 fence_latest_unused_value;
		Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue;
		

		
		HANDLE event_wait_on_present_thread_to_exit;
		HANDLE event_start_waiting_on_gpu;
		HANDLE event_wait_for_gpu_workload;
		HANDLE per_back_buffer_availability_events[sc_frame_count];
		bool skip_first_availability_flip;
		int present_thread_back_buffer_index;
		int owner_thread_back_buffer_index;
		
};

