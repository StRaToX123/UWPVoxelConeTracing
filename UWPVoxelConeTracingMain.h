#pragma once

#include <wrl.h>
#include "Scene\Lights.h"
#include "Graphics\SceneRenderers\SceneRendererDirectLightingVoxelGIandAO.h"
#include "Utility\Debug\DebugMessage.h"
#include "imgui.h"
#include "imgui_impl_UWP.h"
#include "imgui_impl_dx12.h"

#define GAMEPAD_TRIGGER_THRESHOLD 0.2
#define NUM_DYNAMIC_OBJECTS 40

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
		void OnGamepadConnectedDisconnectedCallback();
		void OnPointerMovedCallback(float x, float y);
		void OnPointerEnteredCallback();
		void OnPointerExitedCallback();
		void OnPointerPressedCallback(Windows::UI::Input::PointerPointProperties^ pointProperties);
		void OnPointerReleasedCallback(Windows::UI::Input::PointerPointProperties^ pointProperties);
		void OnPointerWheelChangedCallback(int wheelDelta);

	private:
		void UpdateCameraControllerPitchAndYaw(float mouseDeltaX, float mouseDeltaY);
		ComPtr<ID3D12GraphicsCommandList>   command_list_compute;
		ComPtr<ID3D12GraphicsCommandList> command_list_direct;
		ComPtr<ID3D12CommandAllocator> command_allocators_direct[DX12DeviceResourcesSingleton::MAX_BACK_BUFFER_COUNT];
		ComPtr<ID3D12CommandAllocator> command_allocators_compute[DX12DeviceResourcesSingleton::MAX_BACK_BUFFER_COUNT];

		// Fullscreen Quad
		ComPtr<ID3D12Resource>              fullscreen_quad_vertex_buffer;
		ComPtr<ID3D12Resource>              fullscreen_quad_vertex_upload_buffer;
		D3D12_VERTEX_BUFFER_VIEW            fullscreen_quad_vertex_buffer_view;

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
		std::vector<Model*> scene_filtered;

		DirectionalLight directional_light;
		float directional_light_animation_angle = 0.0f;
		float directional_light_animation_speed = 1.0f;

		// ImGui variables
		bool show_imGui;
		bool imgui_render_dynamic_objects = false;
		bool imgui_dont_update_dynamic_objects = false;
		bool imgui_update_directional_light_buffers = false;
		bool imgui_update_scene_renderer_illumination_flags_buffer = false;
		bool imgui_update_scene_renderer_vct_main_buffer = false;
		bool imgui_update_voxelizer_data_buffer = false;
};