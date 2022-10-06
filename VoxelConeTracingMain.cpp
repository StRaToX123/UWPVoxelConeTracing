#include "VoxelConeTracingMain.h"



// The DirectX 12 Application template is documented at https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409

// Loads and initializes application assets when the application is loaded.
VoxelConeTracingMain::VoxelConeTracingMain() :
	show_imGui(true),
	camera_fov_angle_y(70.0f * XM_PI / 180.0f),
	camera_movement_multiplier(4.0f),
	camera_controller_backward(0.0f),
	camera_controller_forward(0.0f),
	camera_controller_right(0.0f),
	camera_controller_left(0.0f),
	camera_controller_up(0.0f),
	camera_controller_down(0.0f),
	camera_controller_pitch(0.0f),
	camera_controller_yaw(0.0f)
{
	// Change the timer settings if you want something other than the default variable timestep mode.
	// example for 60 FPS fixed timestep update logic
	step_timer.SetFixedTimeStep(true);
	step_timer.SetTargetElapsedSeconds(1.0 / 60);
	
	

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
void VoxelConeTracingMain::CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
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
		case Windows::System::VirtualKey::Space:
		{
			if (down == true)
			{
				camera_controller_up = 1.0f;
			}
			else
			{
				camera_controller_up = 0.0f;
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

// Updates the application state once per frame.
void VoxelConeTracingMain::Update()
{
	// Update scene objects.
	step_timer.Tick([&]()
	{
		XMVECTOR cameraTranslate = XMVectorSet(camera_controller_right - camera_controller_left, 
												camera_controller_up - camera_controller_down, 
			                                     camera_controller_forward - camera_controller_backward, 
			                                      1.0f) * camera_movement_multiplier * static_cast<float>(step_timer.GetElapsedSeconds());
		//XMVECTOR cameraPan = XMVectorSet(0.0f, m_Up - m_Down, 0.0f, 1.0f) * speedMultipler * static_cast<float>(e.ElapsedTime);
		camera.Translate(cameraTranslate, Space::Local);
		//m_Camera.Translate(cameraPan, Space::Local);

		//XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(m_Pitch), XMConvertToRadians(m_Yaw), 0.0f);
		//m_Camera.set_Rotation(cameraRotation);











		
		scene_renderer->Update(step_timer);
	});
}


void VoxelConeTracingMain::Render()
{
	// Don't try to render anything before the first Update.
	/*
	if (step_timer.GetFrameCount() == 0)
	{
		return;
	}
	*/

	ID3D12GraphicsCommandList* pCommandList = scene_renderer->Render(camera, show_imGui);
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
		camera_fov_angle_y *= 2.0f;
	}
	else
	{

	}

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
