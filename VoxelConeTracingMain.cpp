#include "VoxelConeTracingMain.h"



// The DirectX 12 Application template is documented at https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409

// Loads and initializes application assets when the application is loaded.
VoxelConeTracingMain::VoxelConeTracingMain()
{
	// Change the timer settings if you want something other than the default variable timestep mode.
	// example for 60 FPS fixed timestep update logic, call:
	/*
	step_timer.SetFixedTimeStep(true);
	step_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/

	// Create the scene geometry
	scene.reserve(10);
	for (int i = 0; i < scene.capacity(); i++)
	{
		scene.emplace_back(Mesh());
	}

	




	



	
}


// Creates and initializes the renderers.
void VoxelConeTracingMain::CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	// TODO: Replace this with your app's content initialization.
	scene_renderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(deviceResources));

	OnWindowSizeChanged();
}

// Updates the application state once per frame.
void VoxelConeTracingMain::Update()
{
	// Update scene objects.
	step_timer.Tick([&]()
	{
		scene_renderer->Update(step_timer);
	});
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool VoxelConeTracingMain::Render()
{
	// Don't try to render anything before the first Update.
	if (step_timer.GetFrameCount() == 0)
	{
		return false;
	}

	// Render the scene objects.
	// TODO: Replace this with your app's content rendering functions.
	return scene_renderer->Render();
}

// Updates application state when the window's size changes (e.g. device orientation change)
void VoxelConeTracingMain::OnWindowSizeChanged()
{
	// TODO: Replace this with the size-dependent initialization of your app's content.
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
