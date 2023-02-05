#include "UWPVoxelConeTracingMain.h"



// The DirectX 12 Application template is documented at https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409

// Loads and initializes application assets when the application is loaded.
UWPVoxelConeTracingMain::UWPVoxelConeTracingMain(Windows::UI::Core::CoreWindow^ coreWindow)
{
	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/


	core_window = coreWindow;
	show_imGui = true;
	scene_renderer.Initialize(coreWindow);

	camera_controller_translation_multiplier = 12.0f;
	camera_controller_rotation_multiplier = 0.03f;
	camera_controller_backward = 0.0f;
	camera_controller_forward = 0.0f;
	camera_controller_right = 0.0f;
	camera_controller_left = 0.0f;
	camera_controller_up = 0.0;
	camera_controller_down = 0.0f;
	camera_controller_pitch = 0.0f;
	camera_controller_pitch_limit = 89.99f;
	camera_controller_yaw = 0.0f;

	auto size = scene_renderer.mSandboxFramework->GetOutputSize();
	float aspectRatio = float(size.right) / float(size.bottom);

	//camera.Initialize(60.0f * XM_PI / 180.0f, aspectRatio, 0.01f, 500.0f);
	//camera.SetPosition(0.0f, 7.0f, 33.0f);

	camera.Initialize();
	camera.SetProjection(60.0f, aspectRatio, 0.01f, 500.0f);
	camera.SetTranslation(XMVectorSet(0.0f, 7.0f, 33.0f, 1.0f));

	OnWindowSizeChanged();
}

void UWPVoxelConeTracingMain::HandleKeyboardInput(Windows::System::VirtualKey vk, bool down)
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
					Windows::UI::Core::CoreWindow::GetForCurrentThread()->SetPointerCapture();
				}
				else
				{
					Windows::UI::Core::CoreWindow::GetForCurrentThread()->ReleasePointerCapture();
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

void UWPVoxelConeTracingMain::HandleMouseMovementCallback(float mouseDeltaX, float mouseDeltaY)
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

	mouse_delta.x = mouseDeltaX;
	mouse_delta.y = mouseDeltaY;

	UpdateCameraControllerPitchAndYaw(mouseDeltaX, mouseDeltaY);
}

void UWPVoxelConeTracingMain::UpdateCameraControllerPitchAndYaw(float mouseDeltaX, float mouseDeltaY)
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

void UWPVoxelConeTracingMain::OnGamepadConnectedDisconnectedCallback()
{
	/*auto gamepads = Windows::Gaming::Input::Gamepad::Gamepads;
	gamepad = nullptr;
	ImGui_ImplUWP_GamepadConnectedDisconnected_Callback(false);
	if (gamepads->Size != 0)
	{
		gamepad = gamepads->First()->Current;
		ImGui_ImplUWP_GamepadConnectedDisconnected_Callback(true);
	}*/
}

// Updates the application state once per frame.
void UWPVoxelConeTracingMain::Update()
{
	#pragma region Update The Camera
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
				1.0f) * camera_controller_translation_multiplier * static_cast<float>(mTimer.GetElapsedSeconds());
			camera.Translate(cameraTranslate, Space::Local);
		}
	}
	else
	{
		// Keyboard and mouse controlls (SPACE key cannot be handled by the HandleKeyboardInput callback, so we will have to check for it each frame, here in the update function)
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
				1.0f) * camera_controller_translation_multiplier * static_cast<float>(mTimer.GetElapsedSeconds());
			camera.Translate(cameraTranslate, Space::Local);

			/*camera.Update(XMFLOAT3(camera_controller_right - camera_controller_left,
				camera_controller_up - camera_controller_down,
				camera_controller_forward - camera_controller_backward),
				mouse_delta, 
				mTimer);

			mouse_delta.x = 0;
			mouse_delta.y = 0;*/
		}
	}

	//camera.UpdateGPUBuffers();
	#pragma endregion

	// Update scene objects.
	//m_timer.Tick([&]()
	//{
		// TODO: Replace this with your app's content update functions.
		//m_sceneRenderer->Update(m_timer);
	//});
	scene_renderer.Run(mTimer, camera);
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool UWPVoxelConeTracingMain::Render()
{
	// Don't try to render anything before the first Update.
	//if (m_timer.GetFrameCount() == 0)
	//{
		//return false;
	//}

	// Render the scene objects.
	// TODO: Replace this with your app's content rendering functions.
	//return m_sceneRenderer->Render();
	return true;
}

// Updates application state when the window's size changes (e.g. device orientation change)
void UWPVoxelConeTracingMain::OnWindowSizeChanged()
{
	// TODO: Replace this with the size-dependent initialization of your app's content.
	//m_sceneRenderer->CreateWindowSizeDependentResources();
	scene_renderer.OnWindowSizeChanged(camera);
}

// Notifies the app that it is being suspended.
void UWPVoxelConeTracingMain::OnSuspending()
{
	// TODO: Replace this with your app's suspending logic.

	// Process lifetime management may terminate suspended apps at any time, so it is
	// good practice to save any state that will allow the app to restart where it left off.

	//m_sceneRenderer->SaveState();

	// If your application uses video memory allocations that are easy to re-create,
	// consider releasing that memory to make it available to other applications.
}

// Notifes the app that it is no longer suspended.
void UWPVoxelConeTracingMain::OnResuming()
{
	// TODO: Replace this with your app's resuming logic.
}

// Notifies renderers that device resources need to be released.
void UWPVoxelConeTracingMain::OnDeviceRemoved()
{
	// TODO: Save any necessary application or renderer state and release the renderer
	// and its resources which are no longer valid.
	//m_sceneRenderer->SaveState();
}
