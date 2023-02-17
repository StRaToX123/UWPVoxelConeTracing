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
	DX12DeviceResourcesSingleton* _deviceResources = DX12DeviceResourcesSingleton::GetDX12DeviceResources();
	ID3D12Device* _d3dDevice = _deviceResources->GetD3DDevice();

	#pragma region Initialize Command Allocators And Lists
	{
		
		// Create a command allocator for each back buffer that will be rendered to.
		for (UINT i = 0; i < _deviceResources->GetBackBufferCount(); i++)
		{
			ThrowIfFailed(_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(command_allocators_direct[i].ReleaseAndGetAddressOf())));
		}

		for (UINT i = 0; i < _deviceResources->GetBackBufferCount(); i++)
		{
			ThrowIfFailed(_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(command_allocators_compute[i].ReleaseAndGetAddressOf())));
		}

		// Create the command lists
		ThrowIfFailed(_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators_direct[0].Get(), nullptr, IID_PPV_ARGS(command_list_direct.ReleaseAndGetAddressOf())));
		ThrowIfFailed(_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, command_allocators_compute[0].Get(), nullptr, IID_PPV_ARGS(command_list_compute.ReleaseAndGetAddressOf())));
		command_list_compute->Close();
	}
	#pragma endregion

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
	camera_controller_yaw = 180.0f;

	float aspectRatio = core_window->Bounds.Width / core_window->Bounds.Height;
	camera.SetProjection(60.0f, aspectRatio, 0.01f, 500.0f);
	camera.SetPositionWorldSpace(XMVectorSet(0.0f, 7.0f, 33.0f, 1.0f));
	camera.SetRotationLocalSpaceQuaternion(DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), XM_PI));
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

		scene_filtered.reserve(scene.size());
	}
	#pragma endregion

	// Initialize the directional light
	directional_light.Initialize(&descriptor_heap_manager);

	#pragma region Initialize Full ScreenQuad
	{
		// Create full screen quad
		struct FullscreenVertex
		{
			XMFLOAT4 position;
			XMFLOAT2 uv;
		};

		// Define the geometry for a fullscreen triangle.
		FullscreenVertex quadVertices[] =
		{
			{ { -1.0f, -1.0f, 0.0f, 1.0f },{ 0.0f, 1.0f } },       // Bottom left.
			{ { -1.0f, 1.0f, 0.0f, 1.0f },{ 0.0f, 0.0f } },        // Top left.
			{ { 1.0f, -1.0f, 0.0f, 1.0f },{ 1.0f, 1.0f } },        // Bottom right.
			{ { 1.0f, 1.0f, 0.0f, 1.0f },{ 1.0f, 0.0f } },         // Top right.
		};

		const UINT vertexBufferSize = sizeof(quadVertices);

		ThrowIfFailed(_d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT /*D3D12_HEAP_TYPE_UPLOAD*/),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			/*D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER*/ D3D12_RESOURCE_STATE_COPY_DEST /*D3D12_RESOURCE_STATE_GENERIC_READ*/,
			nullptr,
			IID_PPV_ARGS(&fullscreen_quad_vertex_buffer)));

		ThrowIfFailed(_d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&fullscreen_quad_vertex_upload_buffer)));

		// Copy data to the intermediate upload heap and then schedule a copy
		// from the upload heap to the vertex buffer.
		D3D12_SUBRESOURCE_DATA vertexData = {};
		vertexData.pData = reinterpret_cast<BYTE*>(quadVertices);
		vertexData.RowPitch = vertexBufferSize;
		vertexData.SlicePitch = vertexData.RowPitch;

		UpdateSubresources<1>(command_list_direct.Get(), fullscreen_quad_vertex_buffer.Get(), fullscreen_quad_vertex_upload_buffer.Get(), 0, 0, 1, &vertexData);
		command_list_direct.Get()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(fullscreen_quad_vertex_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// Initialize the vertex buffer view.
		fullscreen_quad_vertex_buffer_view.BufferLocation = fullscreen_quad_vertex_buffer->GetGPUVirtualAddress();
		fullscreen_quad_vertex_buffer_view.StrideInBytes = sizeof(FullscreenVertex);
		fullscreen_quad_vertex_buffer_view.SizeInBytes = sizeof(quadVertices);
	}
	#pragma endregion

	#pragma region Initialize imGUI
	ImGui::CreateContext(coreWindow);
	ImGui_ImplUWP_Init();
	ImGui_ImplDX12_Init(DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetD3DDevice(), 
		DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetBackBufferCount(),
		DX12DeviceResourcesSingleton::GetDX12DeviceResources()->GetBackBufferFormat());
	#pragma endregion

	command_list_direct->Close();
	ID3D12CommandList* __commandListsDirect[] = { command_list_direct.Get() };
	_deviceResources->GetCommandQueueDirect()->ExecuteCommandLists(1, __commandListsDirect);

	// This OnWindowSizeChanged will result in a DX12DeviceResourcesSingleton::CreateWindowResources call
	// which will create the swapchain on it's first ever call. This will also result in a call to the 
	// WaitForGPU function which is inside the CreateWindowResources function. We need to wait for the GPU here
	// since we just submited some work to it during this Initialization
	OnWindowSizeChanged(); 
}

UWPVoxelConeTracingMain::~UWPVoxelConeTracingMain()
{
	DX12DeviceResourcesSingleton::GetDX12DeviceResources()->WaitForGPU();
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplUWP_Shutdown();
	ImGui::DestroyContext();
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
	auto gamepads = Windows::Gaming::Input::Gamepad::Gamepads;
	gamepad = nullptr;
	ImGui_ImplUWP_GamepadConnectedDisconnected_Callback(false);
	if (gamepads->Size != 0)
	{
		gamepad = gamepads->First()->Current;
		ImGui_ImplUWP_GamepadConnectedDisconnected_Callback(true);
	}
}

void UWPVoxelConeTracingMain::OnPointerMovedCallback(float x, float y)
{
	if (show_imGui == true)
	{
		ImGui_ImplUWP_PointerMoved_Callback(x, y);
	}
}

void UWPVoxelConeTracingMain::OnPointerEnteredCallback()
{
	if (show_imGui)
	{
		ImGui_ImplUWP_PointerEntered_Callback();
	}
}

void UWPVoxelConeTracingMain::OnPointerExitedCallback()
{
	if (show_imGui == true)
	{
		ImGui_ImplUWP_PointerExited_Callback();
	}
}

void UWPVoxelConeTracingMain::OnPointerPressedCallback(Windows::UI::Input::PointerPointProperties^ pointProperties)
{
	if (show_imGui == true)
	{
		ImGui_ImplUWP_PointerButton_Callback(pointProperties);
	}
}

void UWPVoxelConeTracingMain::OnPointerReleasedCallback(Windows::UI::Input::PointerPointProperties^ pointProperties)
{
	if (show_imGui == true)
	{
		ImGui_ImplUWP_PointerButton_Callback(pointProperties);
	}
}

void UWPVoxelConeTracingMain::OnPointerWheelChangedCallback(int wheelDelta)
{
	if (show_imGui == true)
	{
		ImGui_ImplUWP_PointerWheelChanged_Callback(wheelDelta);
	}
}

// Updates the application state once per frame.
void UWPVoxelConeTracingMain::Update()
{
	timer.Tick([&]()
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

		// Update the scene
		if (imgui_dont_update_dynamic_objects == false)
		{
			for (auto& model : scene)
			{
				if (model->GetIsDynamic())
				{
					model->position_world_space = DirectX::XMVectorSetY(model->position_world_space, DirectX::XMVectorGetY(model->position_world_space) + sin(timer.GetTotalSeconds() * model->GetAmplitude()) * model->GetSpeed());
				}
			}
		}

		// Update the directional light
		if (directional_light.is_dynamic == true)
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

	if (imgui_dont_update_dynamic_objects == false)
	{
		for (auto& model : scene)
		{
			if (model->GetIsDynamic())
			{
				model->UpdateBuffers();
			}
		}
	}

	if ((directional_light.is_dynamic == true) || (imgui_update_directional_light_buffers == true))
	{
		directional_light.UpdateBuffers();
	}

	if (imgui_update_scene_renderer_illumination_flags_buffer == true)
	{
		scene_renderer.UpdateBuffers(SceneRendererDirectLightingVoxelGIandAO::UpdatableBuffers::ILLUMINATION_FLAGS_DATA_BUFFER);
	}

	if (imgui_update_scene_renderer_vct_main_buffer == true)
	{
		scene_renderer.UpdateBuffers(SceneRendererDirectLightingVoxelGIandAO::UpdatableBuffers::VCT_MAIN_DATA_BUFFER);
	}

	if (imgui_update_voxelizer_data_buffer == true)
	{
		scene_renderer.UpdateBuffers(SceneRendererDirectLightingVoxelGIandAO::UpdatableBuffers::VOXELIZATION_DATA_BUFFER);
	}

	// Reset all the imGui update flags
	imgui_update_directional_light_buffers = false;
	imgui_update_scene_renderer_illumination_flags_buffer = false;
	imgui_update_scene_renderer_vct_main_buffer = false;
	imgui_update_voxelizer_data_buffer = false;
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed
bool UWPVoxelConeTracingMain::Render()
{
	DX12DeviceResourcesSingleton* _deviceResources = DX12DeviceResourcesSingleton::GetDX12DeviceResources();
	command_allocators_direct[_deviceResources->back_buffer_index]->Reset();
	command_allocators_compute[_deviceResources->back_buffer_index]->Reset();
	command_list_direct->Reset(command_allocators_direct[_deviceResources->back_buffer_index].Get(), nullptr);
	command_list_compute->Reset(command_allocators_compute[_deviceResources->back_buffer_index].Get(), nullptr);
	// Transition the backbuffer state
	CD3DX12_RESOURCE_BARRIER resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	command_list_direct->ResourceBarrier(1, &resourceBarrier);

	// Create an array of all the Models that will get rendered
	for (UINT i = 0; i < scene.size(); i++)
	{
		if (imgui_render_dynamic_objects == true)
		{
			scene_filtered.push_back(scene[i]);
		}
		else // render only static objects
		{
			if (scene[i]->GetIsDynamic() == false)
			{
				scene_filtered.push_back(scene[i]);
			}
		}
	}

	// The scene_renderer.Render exptects to be handed an already reset direct command list and compute command list 
	// it returns a reset direct command list and an unreset compute command list
	scene_renderer.Render(scene_filtered, 
		directional_light, 
		timer, 
		camera,
		fullscreen_quad_vertex_buffer_view,
		command_list_direct.Get(),
		command_list_compute.Get(),
		command_allocators_direct[_deviceResources->back_buffer_index].Get());

	scene_filtered.clear();
	if (show_imGui == true)
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplUWP_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("UWPVoxelConeTracing");
		ImGui::TextColored(ImVec4(0.95f, 0.5f, 0.0f, 1), "FPS: (%.1f FPS), %.3f ms/frame", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
		ImGui::Separator();
		ImGui::Text("Scene controls:");
		ImGui::Checkbox("Dynamic Light", &directional_light.is_dynamic);
		imgui_update_directional_light_buffers |= ImGui::SliderFloat3("Light Color", &directional_light.shader_structure_cpu_directional_light_data.color.x, 0.0f, 1.0f);
		if (directional_light.is_dynamic == false)
		{
			imgui_update_directional_light_buffers |= ImGui::SliderFloat3("Light Direction", &directional_light.direction_world_space.x, -1.0f, 1.0f);
		}
		
		imgui_update_directional_light_buffers |= ImGui::SliderFloat("Light Intensity", &directional_light.shader_structure_cpu_directional_light_data.intensity, 0.0f, 5.0f);
		ImGui::Checkbox("Dynamic objects", &imgui_render_dynamic_objects);
		if (imgui_render_dynamic_objects == true) 
		{
			ImGui::SameLine();
			ImGui::Checkbox("Stop", &imgui_dont_update_dynamic_objects);
		}

		ImGui::Separator();
		ImGui::Text("Rendering:");
		imgui_update_scene_renderer_illumination_flags_buffer |= ImGui::Checkbox("Direct Light", (bool*)&scene_renderer.shader_structure_cpu_illumination_flags_data.use_direct);
		imgui_update_scene_renderer_illumination_flags_buffer |= ImGui::Checkbox("Direct Shadows", (bool*)&scene_renderer.shader_structure_cpu_illumination_flags_data.use_shadows);
		imgui_update_scene_renderer_illumination_flags_buffer |= ImGui::Checkbox("Voxel Cone Tracing", (bool*)&scene_renderer.shader_structure_cpu_illumination_flags_data.use_vct);
		imgui_update_scene_renderer_illumination_flags_buffer |= ImGui::Checkbox("Show Only AO", (bool*)&scene_renderer.shader_structure_cpu_illumination_flags_data.show_only_ao);
		if (ImGui::CollapsingHeader("Global Illumination Config"))
		{
			//ImGui::Checkbox("Render voxels for debug", &mVCTRenderDebug);
			imgui_update_voxelizer_data_buffer |= ImGui::SliderFloat("Voxel Grid Extent", &scene_renderer.shader_structure_cpu_voxelization_data.voxel_grid_extent_world_space, 1.0f, 400.0f);
			imgui_update_scene_renderer_illumination_flags_buffer |= ImGui::SliderFloat("GI Intensity", &scene_renderer.shader_structure_cpu_illumination_flags_data.vct_gi_power, 0.0f, 15.0f);
			imgui_update_scene_renderer_vct_main_buffer |= ImGui::SliderFloat("Diffuse Strength", &scene_renderer.shader_structure_cpu_vct_main_data.indirect_diffuse_strength, 0.0f, 1.0f);
			imgui_update_scene_renderer_vct_main_buffer |= ImGui::SliderFloat("Specular Strength", &scene_renderer.shader_structure_cpu_vct_main_data.indirect_specular_strength, 0.0f, 1.0f);
			imgui_update_scene_renderer_vct_main_buffer |= ImGui::SliderFloat("Max Cone Trace Dist", &scene_renderer.shader_structure_cpu_vct_main_data.max_cone_trace_distance, 0.0f, 2500.0f);
			imgui_update_scene_renderer_vct_main_buffer |= ImGui::SliderFloat("AO Falloff", &scene_renderer.shader_structure_cpu_vct_main_data.ao_falloff, 0.0f, 2.0f);
			imgui_update_scene_renderer_vct_main_buffer |= ImGui::SliderFloat("Sampling Factor", &scene_renderer.shader_structure_cpu_vct_main_data.sampling_factor, 0.01f, 3.0f);
			imgui_update_scene_renderer_vct_main_buffer |= ImGui::SliderFloat("Sample Offset", &scene_renderer.shader_structure_cpu_vct_main_data.voxel_sample_offset, -0.1f, 0.1f);
		}

		ImGui::End();
		ImGui::Render();
		// Execute the command list
		ID3D12DescriptorHeap* imGuiHeaps[] = { ImGui_ImplDX12_GetDescriptorHeap() };
		command_list_direct->SetDescriptorHeaps(1, imGuiHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), command_list_direct.Get() );
		
	}

	// Transition the back buffer back to a present resource state
	resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	command_list_direct->ResourceBarrier(1, &resourceBarrier);
	command_list_direct->Close();
	ID3D12CommandList* __commandListsDirect[] = { command_list_direct.Get() };
	_deviceResources->GetCommandQueueDirect()->ExecuteCommandLists(1, __commandListsDirect);
	_deviceResources->Present();
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

	float aspectRatio = core_window->Bounds.Width / core_window->Bounds.Height;
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
