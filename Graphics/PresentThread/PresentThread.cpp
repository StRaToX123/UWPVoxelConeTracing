

#include "Graphics/PresentThread/PresentThread.h"



static bool gs_shutdown_present_thread = false;
static HANDLE gs_event_wait_on_present_thread_to_exit = NULL;
static HANDLE gs_event_start_waiting_on_gpu = NULL;
static HANDLE _gs_event_wait_for_gpu_worldload = NULL;
static void* gsp_thread_handle = NULL;
static Microsoft::WRL::ComPtr<ID3D12Fence> gs_fence;
static Microsoft::WRL::ComPtr<IDXGISwapChain3> gs_swapchain;
static UINT64 gs_per_frame_fence_values_to_wait_for[gsc_frame_count];


bool PresentThread::Initialize(const Microsoft::WRL::ComPtr<ID3D12Fence>& fence,
								const HANDLE& eventWaitForGPUWorkload,
								 const Microsoft::WRL::ComPtr<IDXGISwapChain3>& swapchain)
{
	gs_fence = fence;
	gs_swapchain = swapchain;
	gs_event_wait_on_present_thread_to_exit = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (gs_event_wait_on_present_thread_to_exit == nullptr)
	{
		string errorMessage = GetLastErrorAsString();
		DisplayDebugMessage("@@@@@@@@@@@@@@ FAILED TO CALL CreateEvent for gs_event_wait_on_present_thread_to_exit, last error was : %s @@@@@@@@@@@@@@\n\0", errorMessage.c_str());
		return false;
	}

	gs_event_start_waiting_on_gpu = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (gs_event_start_waiting_on_gpu == nullptr)
	{
		string errorMessage = GetLastErrorAsString();
		DisplayDebugMessage("@@@@@@@@@@@@@@ FAILED TO CALL CreateEvent for gs_event_start_waiting_on_gpu, last error was : %s @@@@@@@@@@@@@@\n\0", errorMessage.c_str());
		return false;
	}

	gsp_thread_handle = CreateThread(NULL, 0, GSPresentThreadMainFunction, nullptr, CREATE_SUSPENDED, NULL);
	if (gsp_thread_handle == NULL)
	{
		string errorMessage = GetLastErrorAsString();
		DisplayDebugMessage("@@@@@@@@@@@@@@ FAILED TO CALL CreateThread, last error was : %s @@@@@@@@@@@@@@\n\0", errorMessage.c_str());
		return false;

	}
	
	return true;
}

void PresentThread::ShutDown()
{
	if (gsp_thread_handle != NULL)
	{
		gs_shutdown_present_thread = true;
		WaitForSingleObjectEx(gs_event_wait_on_present_thread_to_exit, INFINITE, FALSE);
		CloseHandle(gsp_thread_handle);
	}

	if (gs_event_start_waiting_on_gpu != NULL)
	{
		CloseHandle(gs_event_start_waiting_on_gpu);
	}

	if (gs_event_wait_on_present_thread_to_exit != NULL)
	{
		CloseHandle(gs_event_wait_on_present_thread_to_exit);
	}

	// No need to close the _gs_event_wait_for_gpu_worldload
	// that event is being handled by the MainThread which owns the DeviceResources instance
}

void PresentThread::Start()
{
	if (gsp_thread_handle != NULL)
	{
		SetThreadPriority(gsp_thread_handle, THREAD_PRIORITY_HIGHEST);
		ResumeThread(gsp_thread_handle);
	}
}

void PresentThread::StartWaitingOnGPU(UINT64 fenceValueToWaitFor)
{
	gs_per_frame_fence_values_to_wait_for[gs_current_back_buffer_index] = fenceValueToWaitFor;
	SetEvent(gs_event_start_waiting_on_gpu);
}


static DWORD WINAPI GSPresentThreadMainFunction(void* data)
{
	while (gs_shutdown_present_thread == false)
	{
		// Wait util we're told to start waiting on the gpu
		WaitForSingleObjectEx(gs_event_start_waiting_on_gpu, INFINITE, FALSE);
		if (gs_fence->GetCompletedValue() < gs_per_frame_fence_values_to_wait_for[gs_current_back_buffer_index])
		{
			WaitForSingleObjectEx(_gs_event_wait_for_gpu_worldload, INFINITE, FALSE);
		}

		gs_swapchain->Present(1, 0);
		// Mark the previously presented frame as available for rendering now
		sadadasdasd
		gs_current_back_buffer_index++;
		if (gs_current_back_buffer_index > gsc_frame_count)
		{
			gs_current_back_buffer_index = 0;
		}
	}

	SetEvent(gs_event_wait_on_present_thread_to_exit);
	return 0;
}
