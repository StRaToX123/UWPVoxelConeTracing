#include "VoxelConeTracingMain.h"




VoxelConeTracingMain::VoxelConeTracingMain() :
	show_imGui(true),
	camera_default_fov_degrees(70.0f),
	camera_controller_translation_multiplier(4.0f),
	camera_controller_rotation_multiplier(0.03f),
	camera_controller_backward(0.0f),
	camera_controller_forward(0.0f),
	camera_controller_right(0.0f),
	camera_controller_left(0.0f),
	camera_controller_up(0.0f),
	camera_controller_down(0.0f),
	camera_controller_pitch(0.0f),
	camera_controller_pitch_limit(89.99f),
	camera_controller_yaw(0.0f),
	show_voxel_debug_view(true)
{
	// Change the timer settings if you want something other than the default variable timestep mode.
	// example for 60 FPS fixed timestep update logic
	step_timer.SetFixedTimeStep(true);
	step_timer.SetTargetElapsedSeconds(1.0 / 60);

	// Setup the camera
	camera.SetFoV(camera_default_fov_degrees);
	camera.SetTranslation(XMVectorSet(0.0f, 0.0f, -2.0f, 1.0f));

	// Create the scene geometry
	scene.reserve(1);
	for (int i = 0; i < scene.capacity(); i++)
	{
		scene.emplace_back(Mesh(true));
	}

	// Initialize and setup the Cornell box
	scene[0].InitializeAsPlane(2.0f, 2.0f);
	scene[0].SetColor(XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f));
	/*
	scene[1].InitializeAsPlane(2.0f, 2.0f);
	scene[1].SetColor(XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
	scene[2].InitializeAsPlane(2.0f, 2.0f);
	scene[2].SetColor(XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f));
	scene[3].InitializeAsPlane(2.0f, 2.0f);
	scene[3].SetColor(XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f));
	scene[4].InitializeAsPlane(2.0f, 2.0f);
	scene[4].SetColor(XMVectorSet(0.0f, 1.0f, 1.0f, 1.0f));
	scene[5].InitializeAsPlane(2.0f, 2.0f);
	scene[5].SetColor(XMVectorSet(1.0f, 0.0f, 1.0f, 1.0f));
	scene[6].InitializeAsCube(0.5f);
	scene[6].SetColor(XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f));
	scene[7].InitializeAsSphere(0.4f);
	scene[7].SetColor(XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));

	scene[1].world_position.x = 1.0f;
	scene[1].world_position.y = 1.0f;
	scene[1].local_rotation.z = 90.0f;

	scene[2].world_position.x = -1.0f;
	scene[2].world_position.y = 1.0f;
	scene[2].local_rotation.z = -90.0f;

	scene[3].world_position.z = 1.0f;
	scene[3].world_position.y = 1.0f;
	scene[3].local_rotation.x = -90.0f;

	scene[4].world_position.z = -1.0f;
	scene[4].world_position.y = 1.0f;
	scene[4].local_rotation.x = 90.0f;

	scene[5].world_position.y = 2.0f;
	scene[5].local_rotation.x = 180.0f;

	scene[6].world_position.x = -0.3f;
	scene[6].world_position.y = 0.25f;
	scene[6].world_position.z = 0.3f;


	scene[7].world_position.x = 0.3f;
	scene[7].world_position.y = 0.2f;
	scene[7].world_position.z = -0.3f;
	*/

	// Setup the spot light
	/*
	spot_light.world_position.y = 1.99f;
	spot_light.direction_world_space.z = 0.0f;
	spot_light.direction_world_space.y = -1.0f;
	*/
}



// Creates and initializes the renderers.
void VoxelConeTracingMain::CreateRenderers(CoreWindow^ coreWindow, const std::shared_ptr<DeviceResources>& deviceResources)
{
	core_window = coreWindow;
	device_resources = deviceResources;

	ImGui_ImplDX12_Init(device_resources->GetD3DDevice(), c_frame_count, device_resources->GetBackBufferFormat());
	scene_renderer = std::unique_ptr<SceneRenderer3D>(new SceneRenderer3D(device_resources, camera));
	OnWindowSizeChanged();
}

void VoxelConeTracingMain::HandleKeyboardInput(Windows::System::VirtualKey vk, bool down)
{
	if (gamepad != nullptr)
	{
		return;
	}

	switch (vk)
	{
		case Windows::System::VirtualKey::Q:
		{
			if (down == true)
			{
				show_imGui = !show_imGui;
				if (show_imGui == false)
				{
					CoreWindow::GetForCurrentThread()->SetPointerCapture();
				}
				else
				{
					CoreWindow::GetForCurrentThread()->ReleasePointerCapture();
				}
			}

			break;
		}

		case Windows::System::VirtualKey::D:
		{
			if (down == true)
			{
				camera_controller_right = 1.0f;
			}
			else
			{
				camera_controller_right = 0.0f;
			}

			break;
		}

		case Windows::System::VirtualKey::A:
		{
			if (down == true)
			{
				camera_controller_left = 1.0f;
			}
			else
			{
				camera_controller_left = 0.0f;
			}

			break;
		}

		case Windows::System::VirtualKey::W:
		{
			if (down == true)
			{
				camera_controller_forward = 1.0f;
			}
			else
			{
				camera_controller_forward = 0.0f;
			}

			break;
		}

		case Windows::System::VirtualKey::S:
		{
			if (down == true)
			{
				camera_controller_backward = 1.0f;
			}
			else
			{
				camera_controller_backward = 0.0f;
			}

			break;
		}

		case Windows::System::VirtualKey::Control:
		{
			if (down == true)
			{
				camera_controller_down = 1.0f;
			}
			else
			{
				camera_controller_down = 0.0f;
			}

			break;
		}
	}
}

void VoxelConeTracingMain::HandleMouseMovementCallback(float mouseDeltaX, float mouseDeltaY)
{
	// Camera rotation controll via mouse
	if (gamepad != nullptr)
	{
		return;
	}

	if (show_imGui == true)
	{
		return;
	}

	UpdateCameraControllerPitchAndYaw(mouseDeltaX, mouseDeltaY);
}

void VoxelConeTracingMain::UpdateCameraControllerPitchAndYaw(float mouseDeltaX, float mouseDeltaY)
{
	camera_controller_pitch += mouseDeltaY * camera_controller_rotation_multiplier;
	camera_controller_yaw += mouseDeltaX * camera_controller_rotation_multiplier;

	camera_controller_pitch = (float)__max(-camera_controller_pitch_limit, camera_controller_pitch);
	camera_controller_pitch = (float)__min(+camera_controller_pitch_limit, camera_controller_pitch);

	// keep longitude in useful range by wrapping
	if (camera_controller_yaw > 360.0f)
	{
		camera_controller_yaw -= 360.0f;
	}
	else
	{
		if (camera_controller_yaw < -360.0f)
		{
			camera_controller_yaw += 360.0f;
		}
	}
}

// Updates the application state once per frame.
void VoxelConeTracingMain::Update()
{
#pragma region UpdateTheCamera
	// Update the camera
	// Check to see if we should update the camera using the gamepad or using the keyboard and mouse
	if (gamepad != nullptr)
	{

		// Update the camera using the gamepad
		Windows::Gaming::Input::GamepadReading gamepadReading = gamepad->GetCurrentReading();
		// Toggle ImGui on the gamepad menu key press
		// We need to guard this against key spam which occurs when the key is held down,
		// otherwise the show_ImGui boolean will toggle rapidly and flash the UI on and off
		static bool gamepadMenuKeySpamGuard = false;
		if (static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::Menu))
		{
			if (gamepadMenuKeySpamGuard == false)
			{
				gamepadMenuKeySpamGuard = true;
				show_imGui = !show_imGui;
			}
		}
		else
		{
			gamepadMenuKeySpamGuard = false;
		}

		if (show_imGui == true)
		{
			ImGui_ImplUWP_UpdateGamepads_Callback(gamepadReading);
		}
		else
		{
			camera_controller_rotation_multiplier = 1.0f;
			// Update the camera controller variables		
			// We use camera_controller_left to store the leftThumbStickX value
			// and the camera_controller_forward to store the leftThumbStickY value
			// this makes camera_controller_right and camera_controller_down obsolete
			camera_controller_right = 0.0f;
			camera_controller_down = 0.0f;
			if ((gamepadReading.LeftThumbstickX <= -GAMEPAD_TRIGGER_THRESHOLD)
				|| (gamepadReading.LeftThumbstickX >= GAMEPAD_TRIGGER_THRESHOLD))
			{
				camera_controller_left = gamepadReading.LeftThumbstickX;
			}
			else
			{
				camera_controller_left = 0.0f;
			}

			if ((gamepadReading.LeftThumbstickY <= -GAMEPAD_TRIGGER_THRESHOLD)
				|| (gamepadReading.LeftThumbstickY >= GAMEPAD_TRIGGER_THRESHOLD))
			{
				camera_controller_forward = gamepadReading.LeftThumbstickY;
			}
			else
			{
				camera_controller_forward = 0.0f;
			}

			UpdateCameraControllerPitchAndYaw(((gamepadReading.RightThumbstickX <= -GAMEPAD_TRIGGER_THRESHOLD) || (gamepadReading.RightThumbstickX >= GAMEPAD_TRIGGER_THRESHOLD)) ? gamepadReading.RightThumbstickX : 0.0f,
				((gamepadReading.RightThumbstickY <= -GAMEPAD_TRIGGER_THRESHOLD) || (gamepadReading.RightThumbstickY >= GAMEPAD_TRIGGER_THRESHOLD)) ? -gamepadReading.RightThumbstickY : 0.0f);

			if (static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::A))
			{
				camera_controller_up = 1.0f;
			}
			else
			{
				camera_controller_up = 0.0f;
			}

			if (static_cast<int>(gamepadReading.Buttons) & static_cast<int>(Windows::Gaming::Input::GamepadButtons::B))
			{
				camera_controller_down = 1.0f;
			}
			else
			{
				camera_controller_down = 0.0f;
			}

			// Update the camera 
			XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(camera_controller_pitch), XMConvertToRadians(camera_controller_yaw), 0.0f);
			camera.SetRotation(cameraRotation);
			XMVECTOR cameraTranslate = XMVectorSet(camera_controller_left,
				camera_controller_up - camera_controller_down,
				camera_controller_forward,
				1.0f) * camera_controller_translation_multiplier * static_cast<float>(step_timer.GetElapsedSeconds());
			camera.Translate(cameraTranslate, Space::Local);
		}
	}
	else
	{
		// Keyboard and mouse controlls
		if (show_imGui == false)
		{
			camera_controller_rotation_multiplier = 0.03f;
			static bool spaceKeySpamGuard = false;
			// Sheck space key status
			// This needs to be an async call otherwise the Control key and Space key end up conflicting with one another for some reason.
			// The Control key if held down before the Space key, creates a delay in the Space keys keydown press for some reason.
			if ((core_window->GetAsyncKeyState(Windows::System::VirtualKey::Space) & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down)
			{
				if (spaceKeySpamGuard == false)
				{
					spaceKeySpamGuard = true;
					camera_controller_up = 1.0f;
				}
			}
			else
			{
				if (spaceKeySpamGuard == true)
				{
					spaceKeySpamGuard = false;
					camera_controller_up = 0.0f;
				}
			}

			// Update the camera 
			XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(camera_controller_pitch), XMConvertToRadians(camera_controller_yaw), 0.0f);
			camera.SetRotation(cameraRotation);
			XMVECTOR cameraTranslate = XMVectorSet(camera_controller_right - camera_controller_left,
				camera_controller_up - camera_controller_down,
				camera_controller_forward - camera_controller_backward,
				1.0f) * camera_controller_translation_multiplier * static_cast<float>(step_timer.GetElapsedSeconds());
			camera.Translate(cameraTranslate, Space::Local);
		}
	}
#pragma endregion

	// Update scene objects.
	step_timer.Tick([&]()
	{	
			
		//scene[0].local_rotation.y += static_cast<float>(step_timer.GetElapsedSeconds()) * 45.0f;
			/*
		if (scene[1].is_static == false)
		{
			scene[1].local_rotation.y -= static_cast<float>(step_timer.GetElapsedSeconds()) * 45.0f;

		}
		*/
		
	});
}


void VoxelConeTracingMain::Render()
{
	// Render out the scene
	ID3D12GraphicsCommandList* _commandList = scene_renderer->Render(scene, camera, show_voxel_debug_view);
	if (show_imGui == true)
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplUWP_NewFrame();
		ImGui::NewFrame();
		
		ImGui::ShowDemoWindow();
		ImGuiIO& io = ImGui::GetIO();

		ImGui::Begin("Hello, world!");                          
		ImGui::Text("ImGui Mouse Pos:");               
		ImGui::InputFloat("MouseX", &io.MousePos.x);
		ImGui::InputFloat("MouseY", &io.MousePos.y);
		ImGui::Checkbox("Voxel Debug Visualization", &show_voxel_debug_view);
		ImGui::End();

		/*
		ImGui::Begin("Hello, world!");                         
		ImGui::Text("Screen mouse pos:");               
		ImGui::InputFloat("MouseX", &camera_controller_mouse_position_x);
		ImGui::InputFloat("MouseY", &camera_controller_mouse_position_y);
		ImGui::End();
		*/


		ImGui::Render();
		ID3D12DescriptorHeap* imGuiHeaps[] = { ImGui_ImplDX12_GetDescriptorHeap() };
		_commandList->SetDescriptorHeaps(1, imGuiHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), _commandList);
	}

	// Finish up the rendering 
	// Indicate that the render target will now be used to present when the command list is done executing.
	CD3DX12_RESOURCE_BARRIER presentResourceBarrier =
		CD3DX12_RESOURCE_BARRIER::Transition(device_resources->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	_commandList->ResourceBarrier(1, &presentResourceBarrier);

	ThrowIfFailed(_commandList->Close());

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { _commandList };
	device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

// Updates application state when the window's size changes (e.g. device orientation change)
void VoxelConeTracingMain::OnWindowSizeChanged()
{
	Size outputSize = device_resources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		camera.SetFoV(camera_default_fov_degrees * 2.0f);
	}
	else
	{
		camera.SetFoV(camera_default_fov_degrees);
	}

	camera.SetProjection(camera.GetFoV(), aspectRatio, 0.01f, 100.0f);
	scene_renderer->CreateWindowSizeDependentResources(camera);
}

// Notifies the app that it is being suspended.
void VoxelConeTracingMain::OnSuspending()
{
	// Process lifetime management may terminate suspended apps at any time, so it is
	// good practice to save any state that will allow the app to restart where it left off.

	scene_renderer->SaveState();

	// If your application uses video memory allocations that are easy to re-create,
	// consider releasing that memory to make it available to other applications.
}

// Notifes the app that it is no longer suspended.
void VoxelConeTracingMain::OnResuming()
{
	// TODO: Replace this with your app's resuming logic.
}

// Notifies renderers that device resources need to be released.
void VoxelConeTracingMain::OnDeviceRemoved()
{
	// TODO: Save any necessary application or renderer state and release the renderer
	// and its resources which are no longer valid.
	scene_renderer->SaveState();
	scene_renderer = nullptr;
}

void VoxelConeTracingMain::OnGamepadConnectedDisconnectedCallback()
{
	auto gamepads = Windows::Gaming::Input::Gamepad::Gamepads;
	gamepad = nullptr;
	ImGui_ImplUWP_GamepadConnectedDisconnected_Callback(false);
	if (gamepads->Size != 0)
	{
		gamepad = gamepads->First()->Current;
		ImGui_ImplUWP_GamepadConnectedDisconnected_Callback(true);
	}
}
