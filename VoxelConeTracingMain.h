#pragma once

#include "Utility/Time/StepTimer.h"
#include "Graphics/SceneRenderers/3DSceneRenderer.h"
#include "Graphics/Shaders/ShaderGlobalsCPU.h"
#include "ImGUI/imgui_impl_UWP.h"
#include "ImGUI/imgui_impl_dx12.h"



using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

#define GAMEPAD_TRIGGER_THRESHOLD 0.2


class VoxelConeTracingMain
{
	private:
		friend ref class App;
		VoxelConeTracingMain();
		void Initialize(CoreWindow^ coreWindow, const std::shared_ptr<DeviceResources>& deviceResources);
		void Update();
		void Render();
		void OnWindowSizeChanged();
		void OnSuspending();
		void OnResuming();
		void OnDeviceRemoved();
		void HandleKeyboardInput(Windows::System::VirtualKey vk, bool down);
		void HandleMouseMovementCallback(float mouseDeltaX, float mouseDeltaY);
		void UpdateCameraControllerPitchAndYaw(float mouseDeltaX, float mouseDeltaY);
		void OnGamepadConnectedDisconnectedCallback();


		CoreWindow^ core_window;
		// Cached pointer to device resources.
		std::shared_ptr<DeviceResources> device_resources;
		std::unique_ptr<SceneRenderer3D> scene_renderer;
		// Rendering loop timer.
		DX::StepTimer step_timer;
		std::vector<Mesh> scene;
		float cube_spin_angle;
		bool show_imGui;
		Windows::Gaming::Input::Gamepad^ gamepad;
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
		
		SpotLight spot_light;
		ShaderStructureCPUSpotLight imgui_spot_light_data;
		XMFLOAT4 imgui_spot_light_default_color;

		bool show_voxel_debug_view;
		SceneRenderer3D::ShaderStructureCPUVoxelGridData imgui_voxel_grid_data;
		int voxel_grid_allowed_resolutions[8] = { 4, 16, 32, 64, 128, 256 };
		int imgui_voxel_grid_selected_allowed_resolution_current_index = 0;
		int imgui_voxel_grid_selected_allowed_resolution_previous_index = 0;
		const char* imgui_combo_box_string_voxel_grid_allowed_resolution = " 16\0 32\0 64\0 128\0 256";

		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_default;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list_direct;

};