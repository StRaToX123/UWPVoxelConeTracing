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





using namespace std;
using namespace DX;

class PresentThread
{
	public:
		static bool Initialize(const Microsoft::WRL::ComPtr<ID3D12Fence>& fence, 
								const HANDLE& eventWaitForGPUWorkload,
			                     const Microsoft::WRL::ComPtr<IDXGISwapChain3>& swapchain);
		static void Start();
		static void StartWaitingOnGPU(UINT64 fenceValueToWaitFor);
		static void ShutDown();


	private:
		PresentThread();
};

