#pragma once

#include "Utility/Time/StepTimer.h"
#include "Graphics/SceneRenderers/3DSceneRenderer.h"



using namespace VoxelConeTracing;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;



// Renders Direct3D content on the screen.
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
			// TODO: Replace with your own content renderers.
			std::unique_ptr<Sample3DSceneRenderer> m_sceneRenderer;

			// Rendering loop timer.
			DX::StepTimer m_timer;
	};
}