#pragma once

#include "Graphics/SceneRenderers/DXRSExampleGIScene.h"
#include <wrl.h>
//#include "Common\DirectXHelper.h"
//#include "Common\StepTimer.h"
//#include "Common\DeviceResources.h"
//#include "Content\Sample3DSceneRenderer.h"


#define GAMEPAD_TRIGGER_THRESHOLD 0.2

using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;




class UWPVoxelConeTracingMain
{
	public:
		UWPVoxelConeTracingMain(Windows::UI::Core::CoreWindow^ coreWindow);

		void Update();
		bool Render();
		void OnWindowSizeChanged();
		void OnSuspending();
		void OnResuming();
		void OnDeviceRemoved();
		void HandleKeyboardInput(Windows::System::VirtualKey vk, bool down);
		void HandleMouseMovementCallback(float mouseDeltaX, float mouseDeltaY);
		void UpdateCameraControllerPitchAndYaw(float mouseDeltaX, float mouseDeltaY);
		void OnGamepadConnectedDisconnectedCallback();

		bool show_imGui;

	private:
		Windows::UI::Core::CoreWindow^ core_window;
		DXRSTimer mTimer;
		Windows::Gaming::Input::Gamepad^ gamepad;
		Camera camera;
		XMFLOAT2 mouse_delta;
		float camera_controller_translation_multiplier;
		float camera_controller_rotation_multiplier;
		float camera_controller_forward;
		float camera_controller_backward;
		float camera_controller_left;
		float camera_controller_right;
		float camera_controller_up;
		float camera_controller_down;
		float camera_controller_pitch;
		float camera_controller_pitch_limit;
		float camera_controller_yaw;
		DXRSExampleGIScene scene_renderer;
};