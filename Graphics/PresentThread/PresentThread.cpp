

#include "Graphics/PresentThread/PresentThread.h"



PresentThread::PresentThread()
{
	keep_looping = true;
	p_thread_handle = NULL;
	present_thread_back_buffer_index = 0;
	owner_thread_back_buffer_index = 0;
	fence_value_for_current_gpu_workload_being_waited_on = 1;
	fence_latest_unused_value = 1;
	event_wait_on_present_thread_to_exit = NULL;
	event_start_waiting_on_gpu = NULL;
	event_wait_for_gpu_workload = NULL;
	for (int i = 0; i < sc_frame_count; i++)
	{
		per_back_buffer_availability_events[i] = NULL;
	}

	skip_first_availability_flip = true;
}

PresentThread::~PresentThread()
{
	keep_looping = false;
	if (p_thread_handle != NULL)
	{
		keep_looping = false;
		WaitForSingleObjectEx(event_wait_on_present_thread_to_exit, INFINITE, FALSE);
		CloseHandle(event_wait_on_present_thread_to_exit);
		CloseHandle(event_start_waiting_on_gpu);
		CloseHandle(event_wait_for_gpu_workload);
		for (int i = 0; i < sc_frame_count; i++)
		{
			CloseHandle(per_back_buffer_availability_events[i]);
		}
	}
}


bool PresentThread::Initialize(const Microsoft::WRL::ComPtr<ID3D12Device>& d3d12Device,
							       const Microsoft::WRL::ComPtr<IDXGISwapChain3>& swapChain,
							           const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue)
{
	swap_chain = swapChain;
	command_queue = commandQueue;
	DX::ThrowIfFailed(d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	event_wait_on_present_thread_to_exit = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (event_wait_on_present_thread_to_exit == NULL)
	{
		string errorMessage = GetLastErrorAsString();
		DisplayDebugMessage("@@@@@@@@@@@@@@ FAILED TO CALL CreateEvent for event_wait_on_present_thread_to_exit, last error was : %s @@@@@@@@@@@@@@\n\0", errorMessage.c_str());
		return false;
	}

	event_start_waiting_on_gpu = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (event_start_waiting_on_gpu == NULL)
	{
		string errorMessage = GetLastErrorAsString();
		DisplayDebugMessage("@@@@@@@@@@@@@@ FAILED TO CALL CreateEvent for event_start_waiting_on_gpu, last error was : %s @@@@@@@@@@@@@@\n\0", errorMessage.c_str());
		return false;
	}

	event_wait_for_gpu_workload = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (event_wait_for_gpu_workload == NULL)
	{
		string errorMessage = GetLastErrorAsString();
		DisplayDebugMessage("@@@@@@@@@@@@@@ FAILED TO CALL CreateEvent for event_wait_for_gpu_workload, last error was : %s @@@@@@@@@@@@@@\n\0", errorMessage.c_str());
		return false;
	}

	for (int i = 0; i < sc_frame_count; i++)
	{
		per_back_buffer_availability_events[i] = CreateEvent(nullptr, TRUE, i == owner_thread_back_buffer_index ? FALSE : TRUE/, nullptr);
		if (per_back_buffer_availability_events[i] == NULL)
		{
			string errorMessage = GetLastErrorAsString();
			DisplayDebugMessage("@@@@@@@@@@@@@@ FAILED TO CALL CreateEvent for per_back_buffer_availability_events[%d], last error was : %s @@@@@@@@@@@@@@\n\0", i, errorMessage.c_str());
			return false;
		}
	}

	p_thread_handle = CreateThread(NULL, 0, PresentThreadMainFunction, this, CREATE_SUSPENDED, NULL);
	if (p_thread_handle == NULL)
	{
		string errorMessage = GetLastErrorAsString();
		DisplayDebugMessage("@@@@@@@@@@@@@@ FAILED TO CALL CreateThread, last error was : %s @@@@@@@@@@@@@@\n\0", errorMessage.c_str());
		return false;

	}
	
	return true;
}

void PresentThread::Start()
{
	if (p_thread_handle != NULL)
	{
		SetThreadPriority(p_thread_handle, THREAD_PRIORITY_HIGHEST);
		ResumeThread(p_thread_handle);
	}
}

void PresentThread::Present()
{
	// Schedule a Signal command in the queue.
	DX::ThrowIfFailed(command_queue->Signal(fence.Get(), fence_latest_unused_value));
	fence_latest_unused_value++;
	SetEvent(event_start_waiting_on_gpu);
}

// Called by the owner thread
UINT PresentThread::GetNextAvailableBackBufferIndex()
{
	owner_thread_back_buffer_index++;
	if (owner_thread_back_buffer_index == sc_frame_count)
	{
		owner_thread_back_buffer_index = 0;
	}

	WaitForSingleObjectEx(per_back_buffer_availability_events[owner_thread_back_buffer_index], INFINITE, FALSE);
	ResetEvent(per_back_buffer_availability_events[owner_thread_back_buffer_index]);
	return owner_thread_back_buffer_index;
}

DWORD __stdcall PresentThread::PresentThreadMainFunction(void* data)
{
	PresentThread* pPresentThread = (PresentThread*)data;
	while (pPresentThread->keep_looping)
	{
		// Wait until we're told to start waiting on the gpu workload
		WaitForSingleObjectEx(pPresentThread->event_start_waiting_on_gpu, INFINITE, FALSE);
		ResetEvent(pPresentThread->event_start_waiting_on_gpu);
		if (pPresentThread->fence->GetCompletedValue() < pPresentThread->fence_value_for_current_gpu_workload_being_waited_on)
		{
			WaitForSingleObjectEx(pPresentThread->event_wait_for_gpu_workload, INFINITE, FALSE);
		}

		pPresentThread->fence_value_for_current_gpu_workload_being_waited_on++;
		pPresentThread->swap_chain->Present(1, DXGI_PRESENT_DO_NOT_SEQUENCE);
		//if (pPresentThread->skip_first_availability_flip == true)
		//{
		//	pPresentThread->skip_first_availability_flip = false;
		//}
		//else
		//{
			int previousPresentThreadBackBufferIndex = pPresentThread->present_thread_back_buffer_index - 1;
			if (previousPresentThreadBackBufferIndex < 0)
			{
				previousPresentThreadBackBufferIndex = sc_frame_count - 1;
			}

			SetEvent(pPresentThread->per_back_buffer_availability_events[previousPresentThreadBackBufferIndex]);
		//}
		
		pPresentThread->present_thread_back_buffer_index++;
		if (pPresentThread->present_thread_back_buffer_index == sc_frame_count)
		{
			pPresentThread->present_thread_back_buffer_index = 0;
		}
	}

	SetEvent(pPresentThread->event_wait_on_present_thread_to_exit);
	return 0;
}

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
string PresentThread::GetLastErrorAsString()
{
	//Get the error message ID, if any.
	DWORD errorMessageID = GetLastError();
	if (errorMessageID == 0)
	{
		return string(); //No error message has been recorded
	}

	LPSTR messageBuffer = nullptr;
	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	//Copy the error message into a std::string.
	string message(messageBuffer, size);

	//Free the Win32's string's buffer.
	LocalFree(messageBuffer);

	return message;
}