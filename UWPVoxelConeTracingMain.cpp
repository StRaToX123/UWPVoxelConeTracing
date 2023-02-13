#include "UWPVoxelConeTracingMain.h"



// The DirectX 12 Application template is documented at https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409

// Loads and initializes application assets when the application is loaded.
UWPVoxelConeTracingMain::UWPVoxelConeTracingMain(Windows::UI::Core::CoreWindow^ coreWindow)
{
	
	/*
	timer.SetFixedTimeStep(true);
	timer.SetTargetElapsedSeconds(1.0 / 60);
	*/

	core_window = coreWindow;
	show_imGui = true;

	DX12DeviceResourcesSingleton::Initialize(coreWindow);
	descriptor_heap_manager.Initialize();
	scene_renderer.Initialize(coreWindow);

	#pragma region Initialize The Camera
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

	auto size = DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetOutputSize();
	float aspectRatio = float(size.right) / float(size.bottom);

	camera.SetProjection(60.0f, aspectRatio, 0.01f, 500.0f);
	camera.SetPositionWorldSpace(XMVectorSet(0.0f, 7.0f, 33.0f, 1.0f));
	camera.Initialize(&descriptor_heap_manager);
	#pragma endregion

	#pragma region Initialize The Scene
	Model* model = new Model(&descriptor_heap_manager, 
		GetFilePath("Assets\\Models\\room.fbx"),
		DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
		DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f), -XM_PIDIV2), 
		DirectX::XMFLOAT4(0.7, 0.7, 0.7, 0.0));
	scene.emplace_back(model);

	model = new Model(&descriptor_heap_manager, 
		GetFilePath("Assets\\Models\\dragon.fbx"), 
		DirectX::XMVectorSet(1.5f, 0.0f, -7.0f, 1.0f),
		DirectX::XMQuaternionIdentity(),
		DirectX::XMFLOAT4(0.044f, 0.627f, 0, 0.0));
	scene.emplace_back(model);

	model = new Model(&descriptor_heap_manager, 
		GetFilePath("Assets\\Models\\bunny.fbx"), 
		DirectX::XMVectorSet(21.0f, 13.9f, -19.0f, 1.0f),
		DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), -0.3752457f), 
		DirectX::XMFLOAT4(0.8f, 0.71f, 0, 0.0));
	scene.emplace_back(model);

	model = new Model(&descriptor_heap_manager, 
		GetFilePath("Assets\\Models\\torus.fbx"), 
		DirectX::XMVectorSet(21.0f, 4.0f, -9.6f, 1.0f),
		DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f), -XM_PIDIV2) * DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f), 1.099557f),
		DirectX::XMFLOAT4(0.329f, 0.26f, 0.8f, 0.8f));
	scene.emplace_back(model);

	model = new Model(&descriptor_heap_manager, 
		GetFilePath("Assets\\Models\\sphere_big.fbx"),
		DirectX::XMVectorSet(-17.25f, -1.15f, -24.15f, 1.0f),
		DirectX::XMQuaternionIdentity(),
		DirectX::XMFLOAT4(0.692f, 0.215f, 0.0f, 0.6f));
	scene.emplace_back(model);

	model = new Model(&descriptor_heap_manager,
		GetFilePath("Assets\\Models\\sphere_medium.fbx"), 
		DirectX::XMVectorSet(-21.0f, -0.95f, -13.20f, 1.0f),
		DirectX::XMQuaternionIdentity(),
		DirectX::XMFLOAT4(0.005, 0.8, 0.426, 0.7f));
	scene.emplace_back(model);

	model = new Model(&descriptor_heap_manager, 
		GetFilePath("Assets\\Models\\sphere_small.fbx"),
		DirectX::XMVectorSet(-11.25f, -0.45f, -16.20f, 1.0f),
		DirectX::XMQuaternionIdentity(),
		DirectX::XMFLOAT4(0.01, 0.0, 0.8, 0.75f));
	scene.emplace_back(model);

	model = new Model(&descriptor_heap_manager, 
		GetFilePath("Assets\\Models\\block.fbx"), 
		DirectX::XMVectorSet(3.0f, 8.0f, -30.0f, 1.0f),
		DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f), -XM_PIDIV2),
		DirectX::XMFLOAT4(0.9, 0.15, 1.0, 0.0));
	scene.emplace_back(model);

	model = new Model(&descriptor_heap_manager, 
		GetFilePath("Assets\\Models\\cube.fbx"), 
		DirectX::XMVectorSet(21.0f, 5.0f, -19.0f, 1.0f), 
		DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f), -XM_PIDIV2) * DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), -0.907571f),
		XMFLOAT4(0.1, 0.75, 0.8, 0.0));
	scene.emplace_back(model);

	model = new Model(&descriptor_heap_manager,
		GetFilePath("Assets\\Models\\sphere_medium.fbx"), 
		DirectX::XMVectorSet(RandomFloat(-35.0f, 35.0f), RandomFloat(5.0f, 30.0f), RandomFloat(-35.0f, 35.0f), 1.0f),
		DirectX::XMQuaternionIdentity(),
		XMFLOAT4(RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), 0.8),
		true,
		RandomFloat(-1.0f, 1.0f),
		RandomFloat(1.0f, 5.0f));
	scene.emplace_back(model);
	for (int i = 0; i < NUM_DYNAMIC_OBJECTS - 1; i++)
	{
		Model* modelCopy = new Model(&descriptor_heap_manager,
			DirectX::XMVectorSet(RandomFloat(-35.0f, 35.0f), RandomFloat(5.0f, 30.0f), RandomFloat(-35.0f, 35.0f), 1.0f),
			DirectX::XMQuaternionIdentity(), 
			XMFLOAT4(RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), 0.8),
			true,
			RandomFloat(-1.0f, 1.0f), 
			RandomFloat(1.0f, 5.0f));
		*modelCopy = *model;
		scene.emplace_back(modelCopy);
	}

	// Initialize the directional light
	directional_light.Initialize(&descriptor_heap_manager);
	#pragma endregion

	#pragma region Initialize imGUI
	//ImGui::CreateContext(coreWindow);
	//ImGui_ImplUWP_Init();
	//ImGui_ImplDX12_Init(DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice(), 
	//	DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetBackBufferCount(),
	//	DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetBackBufferFormat());
	#pragma endregion

	OnWindowSizeChanged();
}

UWPVoxelConeTracingMain::~UWPVoxelConeTracingMain()
{
	DX12DeviceResourcesSingleton::GetDX12DeviceResources()->WaitForGPU();
	//ImGui_ImplDX12_Shutdown();
	//ImGui_ImplUWP_Shutdown();
	//ImGui::DestroyContext();
}

bool UWPVoxelConeTracingMain::HandleKeyboardInput(Windows::System::VirtualKey vk, bool down)
{
	bool result = false;
	if (gamepad != nullptr)
	{
		return false;
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

			result = true;
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

			result = true;
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

			result = true;
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

			result = true;
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

			result = true;
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

			result = true;
			break;
		}

		case Windows::System::VirtualKey::Space:
		{
			result = !show_imGui;
			break;
		}
	}

	return result;
}

bool UWPVoxelConeTracingMain::HandleMouseMovementCallback(float mouseDeltaX, float mouseDeltaY)
{
	// Camera rotation controll via mouse
	if (gamepad != nullptr)
	{
		return false;
	}

	if (show_imGui == true)
	{
		return false;
	}

	UpdateCameraControllerPitchAndYaw(mouseDeltaX, mouseDeltaY);
	return true;
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
	timer.Tick([&]()
	{
		if (show_imGui == false)
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
					camera.SetRotationLocalSpaceQuaternion(cameraRotation);
					XMVECTOR cameraTranslate = XMVectorSet(camera_controller_left,
						camera_controller_up - camera_controller_down,
						camera_controller_forward,
						1.0f) * camera_controller_translation_multiplier * static_cast<float>(timer.GetElapsedSeconds());
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
					camera.SetRotationLocalSpaceQuaternion(cameraRotation);
					XMVECTOR cameraTranslate = XMVectorSet(camera_controller_right - camera_controller_left,
						camera_controller_up - camera_controller_down,
						camera_controller_forward - camera_controller_backward,
						1.0f) * camera_controller_translation_multiplier * static_cast<float>(timer.GetElapsedSeconds());
					camera.Translate(cameraTranslate, Space::Local);
				}
			}
			#pragma endregion
		}

		// Update the scene
		for (auto& model : scene)
		{
			if (model->GetIsDynamic())
			{	
				model->position_world_space = DirectX::XMVectorSetY(model->position_world_space, DirectX::XMVectorGetY(model->position_world_space) + sin(timer.GetTotalSeconds() * model->GetAmplitude()) * model->GetSpeed());
			}
		}

		// Update the directional light
		if (directional_light.is_static == false)
		{
			directional_light_animation_angle += static_cast<float>(timer.GetElapsedSeconds()) * 45.0f;
			if (directional_light_animation_angle >= 360.0f)
			{
				directional_light_animation_angle -= 360.0f;
			}

			directional_light.direction_world_space.x = 0.6f * cos(directional_light_animation_angle * (3.14f / 180.0f));
		}
	});	

	// Update all the buffers once
	if (show_imGui == false)
	{
		camera.UpdateBuffers();
	}

	for (auto& model : scene)
	{
		if (model->GetIsDynamic())
		{
			model->UpdateBuffers();
		}
	}

	if (directional_light.is_static == false)
	{
		directional_light.UpdateBuffers();
	}
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool UWPVoxelConeTracingMain::Render()
{
	scene_renderer.Render(scene, directional_light, timer, camera);
	return true;
}

// Updates application state when the window's size changes (e.g. device orientation change)
void UWPVoxelConeTracingMain::OnWindowSizeChanged()
{
	DX12DeviceResourcesSingleton* _dx12DeviceResources = DX12DeviceResourcesSingleton::GetDX12DeviceResources();
	if (!_dx12DeviceResources->OnWindowSizeChanged())
	{
		return;
	}

	float aspectRatio = _dx12DeviceResources->mAppWindow->Bounds.Width / _dx12DeviceResources->mAppWindow->Bounds.Height;
	// This is a simple example of a change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		camera.SetFoVYDeggrees(60.0f * 2.0f);
	}
	else
	{
		camera.SetFoVYDeggrees(60.0f);
	}

	camera.SetProjection(camera.GetFoVYDegrees(), aspectRatio, 0.01f, 500.0f);
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
