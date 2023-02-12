#pragma once

#include <wrl.h>
#include "Scene/Lights.h"
#include "Graphics\SceneRenderers\SceneRendererDirectLightingVoxelGIandAO.h"
#include "Utility/Debug/DebugMessage.h"

#define GAMEPAD_TRIGGER_THRESHOLD 0.2

using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;


class UWPVoxelConeTracingMain
{
	public:
		UWPVoxelConeTracingMain(Windows::UI::Core::CoreWindow^ coreWindow);
		~UWPVoxelConeTracingMain();

		void Update();
		bool Render();
		void OnWindowSizeChanged();
		void OnSuspending();
		void OnResuming();
		void OnDeviceRemoved();
		bool HandleKeyboardInput(Windows::System::VirtualKey vk, bool down);
		bool HandleMouseMovementCallback(float mouseDeltaX, float mouseDeltaY);
		void UpdateCameraControllerPitchAndYaw(float mouseDeltaX, float mouseDeltaY);
		void OnGamepadConnectedDisconnectedCallback();

	private:
		Windows::UI::Core::CoreWindow^ core_window;
		DXRSTimer timer;
		Windows::Gaming::Input::Gamepad^ gamepad;
		Camera camera;
		float camera_controller_translation_multiplier;
		float camera_controller_rotation_multiplier;
		float camera_controller_forward;
		float camera_controller_backward;
		float camera_controller_left;
		float camera_controller_right;
		float camera_controller_up;
		float camera_controller_down;
		float camera_controller_pitch; // X axis rotation
		float camera_controller_pitch_limit;
		float camera_controller_yaw; // Y axis rotation

		SceneRendererDirectLightingVoxelGIandAO scene_renderer;

		DX12DescriptorHeapManager descriptor_heap_manager;
		std::vector<Model*> scene;

		DirectionalLight directional_light;
		float directional_light_animation_angle = 0.0f;
		float directional_light_animation_speed = 1.0f;


		bool show_imGui;
};