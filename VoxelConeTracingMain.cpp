#include "VoxelConeTracingMain.h"



// The DirectX 12 Application template is documented at https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409

// Loads and initializes application assets when the application is loaded.
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
	space_key_pressed_workaround(false)
{
	// Change the timer settings if you want something other than the default variable timestep mode.
	// example for 60 FPS fixed timestep update logic
	step_timer.SetFixedTimeStep(true);
	step_timer.SetTargetElapsedSeconds(1.0 / 60);

	// Setup the camera
	camera.SetFoV(camera_default_fov_degrees);
	camera.SetTranslation(XMVectorSet(0.0f, 0.0f, -2.0f, 1.0f));

	// Create the scene geometry
	/*
	scene.reserve(10);
	for (int i = 0; i < scene.capacity(); i++)
	{
		scene.emplace_back(Mesh());
	}

	scene[0].InitializeAsPlane(5, 5);
	scene[1].InitializeAsPlane(5, 5);
	scene[2].InitializeAsPlane(5, 5);
	scene[3].InitializeAsPlane(5, 5);
	scene[4].InitializeAsPlane(5, 5);
	scene[5].InitializeAsCube(1);
	scene[6].InitializeAsSphere();
	scene[7].InitializeAsCone();
	*/

	
	



	
}



// Creates and initializes the renderers.
void VoxelConeTracingMain::CreateRenderers(CoreWindow^ coreWindow, const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	core_window = coreWindow;
	device_resources = deviceResources;
	ImGui_ImplDX12_Init(device_resources->GetD3DDevice(), DX::c_frameCount, device_resources->GetBackBufferFormat());
	scene_renderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(device_resources));
	OnWindowSizeChanged();
}

void VoxelConeTracingMain::HandleKeyboardInput(Windows::System::VirtualKey vk, bool down)
{
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
	if (show_imGui == true)
	{
		return;
	}

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
	if (show_imGui == false)
	{
		// Sheck space key status
		// This needs to be an async call otherwise the Control key and Space key end up conflicting with one another for some reason.
		// The Control key if held down before the Space key, creates a delay in the Space keys keydown press for some reason.
		if ((core_window->GetAsyncKeyState(Windows::System::VirtualKey::Space) & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down)
		{
			if (space_key_pressed_workaround == false)
			{
				space_key_pressed_workaround = true;
				camera_controller_up = 1.0f;
			}
		}
		else
		{
			if (space_key_pressed_workaround == true)
			{
				space_key_pressed_workaround = false;
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
	
	// Update scene objects.
	step_timer.Tick([&]()
	{
		
	});
}


void VoxelConeTracingMain::Render()
{
	if (show_imGui == true)
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplUWP_NewFrame();
		ImGui::NewFrame();
	}

	

	ID3D12GraphicsCommandList* pCommandList = scene_renderer->Render(camera, step_timer, show_imGui);
	if (show_imGui == true)
	{
		ImGui::ShowDemoWindow();
		ImGuiIO& io = ImGui::GetIO();

		ImGui::Begin("Hello, world!");                          
		ImGui::Text("ImGui Mouse Pos:");               
		ImGui::InputFloat("MouseX", &io.MousePos.x);
		ImGui::InputFloat("MouseY", &io.MousePos.y);
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
		pCommandList->SetDescriptorHeaps(1, imGuiHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList);
	}
	// Finish up the rendering 
	// Indicate that the render target will now be used to present when the command list is done executing.
	CD3DX12_RESOURCE_BARRIER presentResourceBarrier =
		CD3DX12_RESOURCE_BARRIER::Transition(device_resources->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	pCommandList->ResourceBarrier(1, &presentResourceBarrier);

	DX::ThrowIfFailed(pCommandList->Close());

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { pCommandList };
	device_resources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
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
	scene_renderer->CreateWindowSizeDependentResources();
}

// Notifies the app that it is being suspended.
void VoxelConeTracingMain::OnSuspending()
{
	// TODO: Replace this with your app's suspending logic.

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
