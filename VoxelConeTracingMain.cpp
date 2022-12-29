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
}



// Creates and initializes the renderers.
void VoxelConeTracingMain::Initialize(CoreWindow^ coreWindow, const std::shared_ptr<DeviceResources>& deviceResources)
{
	core_window = coreWindow;
	device_resources = deviceResources;

	ImGui_ImplDX12_Init(device_resources->GetD3DDevice(), c_frame_count, device_resources->GetBackBufferFormat());
	imgui_voxel_grid_selected_allowed_resolution_current_index = 3;
	imgui_voxel_grid_selected_allowed_resolution_previous_index = 3;
	scene_renderer = std::unique_ptr<SceneRenderer3D>(new SceneRenderer3D(device_resources, camera, voxel_grid_allowed_resolutions[imgui_voxel_grid_selected_allowed_resolution_current_index]));
	imgui_voxel_grid_data.UpdateRes(voxel_grid_allowed_resolutions[imgui_voxel_grid_selected_allowed_resolution_current_index]);
	#pragma region Root Signature
	CD3DX12_DESCRIPTOR_RANGE rangeCBV;

	CD3DX12_ROOT_PARAMETER parameterMVP;

	rangeCBV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	parameterMVP.InitAsDescriptorTable(1, &rangeCBV, D3D12_SHADER_VISIBILITY_VERTEX);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // Only the input assembler stage needs access to the constant buffer.
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;

	CD3DX12_ROOT_PARAMETER parameterArray[1] = {
		parameterMVP
	};

	descRootSignature.Init(1, parameterArray, 0, nullptr, rootSignatureFlags);

	ComPtr<ID3DBlob> pSignature;
	ComPtr<ID3DBlob> pError;
	ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
	root_signature->SetName(L"VoxelConeTracingMain root_signature");
	#pragma endregion

	#pragma region Default Pipeline
	char* pVertexShaderByteCode;
	int vertexShaderByteCodeLength;
	char* pPixelShaderByteCode;
	int pixelShaderByteCodeLength;
	wstring wStringInstallPath(Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data());
	string standardStringInstallPath(wStringInstallPath.begin(), wStringInstallPath.end());
	LoadShaderByteCode((standardStringInstallPath + "\\Sample_VS.cso").c_str(), pVertexShaderByteCode, vertexShaderByteCodeLength);
	LoadShaderByteCode((standardStringInstallPath + "\\Sample_PS.cso").c_str(), pPixelShaderByteCode, pixelShaderByteCodeLength);


	D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
	state.InputLayout = { ShaderStructureCPUVertexPositionNormalTextureColor::input_elements, ShaderStructureCPUVertexPositionNormalTextureColor::input_element_count };
	state.pRootSignature = root_signature.Get();
	state.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderByteCode, vertexShaderByteCodeLength);
	state.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderByteCode, pixelShaderByteCodeLength);
	state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	state.SampleMask = UINT_MAX;
	state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	state.NumRenderTargets = 1;
	state.RTVFormats[0] = device_resources->GetBackBufferFormat();
	state.DSVFormat = device_resources->GetDepthBufferFormat();
	state.SampleDesc.Count = 1;

	ThrowIfFailed(device_resources->GetD3DDevice()->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipeline_state_default)));
	#pragma endregion

	// Create command list
	// Create direct command list
	ThrowIfFailed(device_resources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, device_resources->GetCommandAllocatorDirect(), pipeline_state_default.Get(), IID_PPV_ARGS(&command_list_direct)));
	command_list_direct->SetName(L"VoxelConeTracing command_list_direct");
	ThrowIfFailed(command_list_direct->Close());

	// Initialize the rest of the class
	// We have to do it here since this is when we get a reference to the device_resources
	// Setup the camera
	camera.Initialize(device_resources);
	camera.SetFoV(camera_default_fov_degrees);
	camera.SetTranslation(XMVectorSet(0.0f, 0.0f, -2.0f, 1.0f));

	// Create the scene geometry
	scene.reserve(2);
	for (int i = 0; i < scene.capacity(); i++)
	{
		scene.emplace_back(Mesh(true));
	}

	// Initialize and setup the Cornell box
	//scene[0].InitializeAsPlane(2.0f, 2.0f);
	scene[0].InitializeAsVerticalPlane(2.0f, 2.0f);
	scene[0].name = "Plane 01";
	//scene[0].SetColor(XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f));

	scene[1].InitializeAsCube();
	scene[1].name = "CUBE 02";
	scene[1].SetColor(XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
	//scene[1].world_position.y = 0.98f;

	/*
	scene[2].InitializeAsPlane(2.0f, 2.0f);
	scene[2].name = "Plane 03";
	scene[2].SetColor(XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f));

	scene[3].InitializeAsPlane(2.0f, 2.0f);
	scene[3].name = "Plane 04";
	scene[3].SetColor(XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f));

	scene[4].InitializeAsPlane(2.0f, 2.0f);
	scene[4].name = "Plane 05";
	scene[4].SetColor(XMVectorSet(0.0f, 1.0f, 1.0f, 1.0f));

	scene[5].InitializeAsPlane(2.0f, 2.0f);
	scene[5].name = "Plane 06";
	scene[5].SetColor(XMVectorSet(1.0f, 0.0f, 1.0f, 1.0f));

	scene[6].InitializeAsCube(0.5f);
	scene[6].name = "Cube 01";
	scene[6].SetColor(XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f));

	scene[7].InitializeAsSphere(0.4f);
	scene[7].name = "Sphere 01";
	scene[7].SetColor(XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));


	scene[0].world_position.y = -0.990f;

	scene[1].world_position.x = 0.99f;
	scene[1].local_rotation.z = 90.0f;

	scene[2].world_position.x = -0.99f;
	scene[2].local_rotation.z = -90.0f;

	scene[3].world_position.z = 0.99f;
	scene[3].local_rotation.x = -90.0f;

	scene[4].world_position.z = -0.99f;
	scene[4].local_rotation.x = 90.0f;

	scene[5].world_position.y = 0.99f;
	scene[5].local_rotation.x = 180.0f;

	scene[6].world_position.x = -0.3f;
	scene[6].world_position.y = -0.75f;
	scene[6].world_position.z = 0.3f;


	scene[7].world_position.x = 0.3f;
	scene[7].world_position.y = -0.8f;
	scene[7].world_position.z = -0.3f;
	*/

	// Setup the spot light
	
	//spot_light.Initialize(L"SpotLight01", scene_renderer->voxel_grid_data.res, scene_renderer->voxel_grid_data.res, device_resources);
	spot_light.Initialize(L"SpotLight01", 
		voxel_grid_allowed_resolutions[imgui_voxel_grid_selected_allowed_resolution_current_index],
		voxel_grid_allowed_resolutions[imgui_voxel_grid_selected_allowed_resolution_current_index],
		device_resources);
	spot_light.constant_buffer_data.position_world_space.y = 0.8f;
	spot_light.constant_buffer_data.position_world_space.z = 0.0f;
	spot_light.constant_buffer_data.direction_world_space.y = -1.0f;
	spot_light.constant_buffer_data.direction_world_space.z = 0.0f;
	XMStoreFloat4(&spot_light.constant_buffer_data.color, XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f));
	spot_light.constant_buffer_data.UpdatePositionViewSpace(camera);
	spot_light.constant_buffer_data.UpdateDirectionViewSpace(camera);
	spot_light.constant_buffer_data.UpdateSpotLightViewMatrix();
	spot_light.constant_buffer_data.UpdateSpotLightProjectionMatrix();
	//imgui_spot_light_data = spot_light.constant_buffer_data;
	spot_light.UpdateConstantBuffers();

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
				1.0f) * camera_controller_translation_multiplier * static_cast<float>(step_timer.GetElapsedSeconds());
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
				1.0f) * camera_controller_translation_multiplier * static_cast<float>(step_timer.GetElapsedSeconds());
			camera.Translate(cameraTranslate, Space::Local);
		}
	}

	camera.UpdateGPUBuffers();
	#pragma endregion

	#pragma region Update The Lights
	spot_light.constant_buffer_data.UpdatePositionViewSpace(camera);
	spot_light.constant_buffer_data.UpdateDirectionViewSpace(camera);
	spot_light.constant_buffer_data.UpdateSpotLightViewMatrix();
	spot_light.UpdateConstantBuffers();
	#pragma endregion

	static float angle = 0.0f;
	static float offset = 0.6f;
	static float convToRad = 3.14f / 180.0f;
	// Update scene objects.
	step_timer.Tick([&]()
	{	
		//angle += static_cast<float>(step_timer.GetElapsedSeconds()) * 45.0f;
		//if (angle >= 360.0f)
		//{
		//	angle -= 360.0f;
		//}

		//scene[1].world_position.x = offset * cos(angle * convToRad);
	});
}


void VoxelConeTracingMain::Render()
{
	// Transition the back buffer from present to render target
	ThrowIfFailed(device_resources->GetCommandAllocatorDirect()->Reset());
	ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_default.Get()));
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(device_resources->GetRenderTarget(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	command_list_direct->ResourceBarrier(1, &barrier);
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = device_resources->GetRenderTargetView();
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = device_resources->GetDepthStencilView();
	command_list_direct->ClearRenderTargetView(renderTargetView, DirectX::Colors::CornflowerBlue, 0, nullptr);
	command_list_direct->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	ThrowIfFailed(command_list_direct->Close());
	ID3D12CommandList* ppCommandLists[] = { command_list_direct.Get() };
	device_resources->GetCommandQueueDirect()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Render out the scene
	scene_renderer->Render(scene, spot_light, camera, show_voxel_debug_view);

	if (show_imGui == true)
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplUWP_NewFrame();
		ImGui::NewFrame();

		ImGui::ShowDemoWindow();

		// Scene controls
		ImGui::SetNextWindowSize(ImVec2(400, 220), ImGuiCond_FirstUseEver);
		ImGui::Begin("Scene");
		if (ImGui::CollapsingHeader("SpotLight01"))
		{
			ImGui::SliderFloat3("World Position", &spot_light.constant_buffer_data.position_world_space.x, -5.0f, 5.0f);
			ImGui::SliderFloat3("Direction", &spot_light.constant_buffer_data.direction_world_space.x, -1.0f, 1.0f);
			ImGui::ColorPicker4("Color", &spot_light.constant_buffer_data.color.x, 0, &imgui_spot_light_default_color.x);
			ImGui::SliderFloat("Intensity", &spot_light.constant_buffer_data.intensity, 1.0f, 5.0f);
			ImGui::SliderFloat("Angle", &spot_light.constant_buffer_data.spot_angle_degrees, 0.0f, 170.0f);
			ImGui::SliderFloat("Attenuation", &spot_light.constant_buffer_data.attenuation, 0.0f, 1.0f);
		}

		for (int i = 0; i < scene.size(); i++)
		{
			if (ImGui::CollapsingHeader(scene[i].name.c_str()))
			{
				string iAsString = to_string(i);
				ImGui::Checkbox((string("Is Static##") + iAsString).c_str(), &scene[i].is_static);
				/*
				ImGui::SliderFloat3((string("World Position##") + iAsString).c_str(), &scene[i].world_position.x, -5.0f, 5.0f);
				ImGui::SliderFloat3((string("Local Rotation##") + iAsString).c_str(), &scene[i].local_rotation.x, 0.0f, 360.0f);
				*/
				ImGui::InputFloat3((string("World Position##") + iAsString).c_str(), &scene[i].world_position.x);
				ImGui::InputFloat3((string("Local Rotation##") + iAsString).c_str(), &scene[i].local_rotation.x);
			}
		}

		ImGui::End();

		// Render controlls
		ImGui::SetNextWindowPos(ImVec2(80, 380), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_FirstUseEver);
		ImGui::Begin("Render Controls");
		if (ImGui::CollapsingHeader("Voxelizer Pass"))
		{
			ImGui::Checkbox("Show", &show_voxel_debug_view);
			ImGui::Combo("Grid Resolution", &imgui_voxel_grid_selected_allowed_resolution_current_index, imgui_combo_box_string_voxel_grid_allowed_resolution);
			//ImGui::SliderFloat("Grid Extent", &scene_renderer->voxel_grid_data.grid_extent, 0.5f, 10.0f);
			ImGui::InputFloat("Grid Extent", &scene_renderer->voxel_grid_data.grid_extent, 0.5f, 10.0f);
			ImGui::SliderInt("Number of Cones", &scene_renderer->voxel_grid_data.num_cones, 2, 10);
			ImGui::SliderFloat("Ray Step Size", &scene_renderer->voxel_grid_data.ray_step_size, 0.0058593f, 0.5f);
			ImGui::SliderFloat("Max Distance", &scene_renderer->voxel_grid_data.max_distance, 0.5f, 10.0f);
			ImGui::SliderInt("Enable Secondary Bounce", &scene_renderer->voxel_grid_data.secondary_bounce_enabled, 0, 1);
		}

		ImGui::End();

		bool callUpdateBuffers = false;
#pragma region Voxelizer Pass Callbacks
		// If the voxel grid resolution changed, we will have to update the voxel grid data structure and all of the voxelizer buffers
		bool updateVoxelizerBuffers = false;
		if (imgui_voxel_grid_selected_allowed_resolution_current_index != imgui_voxel_grid_selected_allowed_resolution_previous_index)
		{
			callUpdateBuffers = true;
			updateVoxelizerBuffers = true;
			imgui_voxel_grid_selected_allowed_resolution_previous_index = imgui_voxel_grid_selected_allowed_resolution_current_index;
			scene_renderer->voxel_grid_data.UpdateRes(voxel_grid_allowed_resolutions[imgui_voxel_grid_selected_allowed_resolution_current_index]);
			imgui_voxel_grid_data.UpdateRes(voxel_grid_allowed_resolutions[imgui_voxel_grid_selected_allowed_resolution_current_index]);
		}

		// If the voxel grid extent changed, we will have to call the grid's UpdateGirdExtent function 
		// and also update the voxel grid data buffer
		bool updateVoxelGridDataBuffers = false;
		if (scene_renderer->voxel_grid_data.grid_extent != imgui_voxel_grid_data.grid_extent)
		{
			callUpdateBuffers = true;
			updateVoxelGridDataBuffers = true;
			scene_renderer->voxel_grid_data.UpdateGirdExtent(scene_renderer->voxel_grid_data.grid_extent);
			imgui_voxel_grid_data.UpdateGirdExtent(scene_renderer->voxel_grid_data.grid_extent);
		}

		// If anything else changed for the voxel grid, we only need to update the voxel grid data buffer
		if (memcmp(&scene_renderer->voxel_grid_data, &imgui_voxel_grid_data, sizeof(SceneRenderer3D::ShaderStructureCPUVoxelGridData)) != 0)
		{
			callUpdateBuffers = true;
			updateVoxelGridDataBuffers = true;
			imgui_voxel_grid_data = scene_renderer->voxel_grid_data;
		}
#pragma endregion

#pragma region Spot Light Callbacks
		if (spot_light.constant_buffer_data.spot_angle_degrees != imgui_spot_light_data.spot_angle_degrees)
		{
			spot_light.constant_buffer_data.UpdateSpotLightProjectionMatrix();
			imgui_spot_light_data.spot_angle_degrees = spot_light.constant_buffer_data.spot_angle_degrees;
		}
#pragma endregion

		if (callUpdateBuffers == true)
		{
			device_resources->WaitForGpu();
			if (updateVoxelizerBuffers == true)
			{
				// !!!!!!!!!!!!!!!!! ALSO UPDATE THE SHADOW MAP TEXTURE OF THE SPOT LIGHT !!!!!!!!!!!!!!!!!
				spot_light.UpdateShadowMapBuffers(scene_renderer->voxel_grid_data.res, scene_renderer->voxel_grid_data.res);

			}

			scene_renderer->UpdateBuffers(updateVoxelizerBuffers, updateVoxelGridDataBuffers);
		}

		ImGui::Render();
		ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_default.Get()));
		command_list_direct->RSSetViewports(1, &device_resources->GetScreenViewport());
		command_list_direct->RSSetScissorRects(1, &device_resources->GetScissorRect());
		// Bind the render target and depth buffer
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = device_resources->GetRenderTargetView();
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = device_resources->GetDepthStencilView();
		command_list_direct->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);
		ID3D12DescriptorHeap* imGuiHeaps[] = { ImGui_ImplDX12_GetDescriptorHeap() };
		command_list_direct->SetDescriptorHeaps(1, imGuiHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), command_list_direct.Get());
	}
	else
	{
		ThrowIfFailed(command_list_direct->Reset(device_resources->GetCommandAllocatorDirect(), pipeline_state_default.Get()));
	}

	// Finish up the rendering 
	// Indicate that the render target will now be used to present when the command list is done executing.
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(device_resources->GetRenderTarget(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);
	command_list_direct->ResourceBarrier(1, &barrier);
	ThrowIfFailed(command_list_direct->Close());
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

	//XMFLOAT4X4 orientation = device_resources->GetOrientationTransform3D();
	//XMMATRIX orientationMatrix = DirectX::XMLoadFloat4x4(&orientation);
	//DirectX::XMStoreFloat4x4(&camera.view_projection_constant_buffer_data.projection, XMMatrixTranspose(camera.GetProjectionMatrix() * orientationMatrix));

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
