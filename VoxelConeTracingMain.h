#pragma once

#include "Utility/Time/StepTimer.h"
#include "Graphics/SceneRenderers/3DSceneRenderer.h"
#include "Scene/Mesh.h"
#include "Scene/Camera.h"
#include "ImGUI/imgui_impl_UWP.h"
#include "ImGUI/imgui_impl_dx12.h"



using namespace VoxelConeTracing;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;




namespace VoxelConeTracing
{
	class VoxelConeTracingMain
	{
		private:
			friend ref class App;
			VoxelConeTracingMain();
			void CreateRenderers(CoreWindow^ coreWindow, const std::shared_ptr<DX::DeviceResources>& deviceResources);
			void Update();
			void Render();
			void OnWindowSizeChanged();
			void OnSuspending();
			void OnResuming();
			void OnDeviceRemoved();
			void HandleKeyboardInput(Windows::System::VirtualKey vk, bool down);
			void HandleMouseMovementCallback(float mouseDeltaX, float mouseDeltaY);


			CoreWindow^ core_window;
			// Cached pointer to device resources.
			std::shared_ptr<DX::DeviceResources> device_resources;
			std::unique_ptr<Sample3DSceneRenderer> scene_renderer;
			// Rendering loop timer.
			DX::StepTimer step_timer;
			std::vector<Mesh> scene;
			float cube_spin_angle;
			bool show_imGui;
			bool space_key_pressed_workaround;
			bool control_key_pressed_workaround;
			// Camera
			Camera camera;
			float camera_default_fov_degrees;
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
	};
}