#pragma once

#include "Utility/Time/StepTimer.h"
#include "Graphics/SceneRenderers/3DSceneRenderer.h"



using namespace VoxelConeTracing;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;




namespace VoxelConeTracing
{
	class VoxelConeTracingMain
	{
		public:
			VoxelConeTracingMain();
			void CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources);
			void Update();
			bool Render();

			void OnWindowSizeChanged();
			void OnSuspending();
			void OnResuming();
			void OnDeviceRemoved();

		private:
			std::unique_ptr<Sample3DSceneRenderer> scene_renderer;
			// Rendering loop timer.
			DX::StepTimer step_timer;
			std::vector<Mesh> scene;
	};
}