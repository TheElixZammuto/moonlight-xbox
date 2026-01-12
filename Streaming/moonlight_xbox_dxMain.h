#pragma once

#include "Common\StepTimer.h"
#include "Streaming\VideoRenderer.h"
#include "Streaming\LogRenderer.h"
#include "Streaming\StatsRenderer.h"
#include "Pages\StreamPage.xaml.h"

// Renders Direct2D and 3D content on the screen.
namespace moonlight_xbox_dx
{
	class moonlight_xbox_dxMain : public DX::IDeviceNotify
	{
	public:
		moonlight_xbox_dxMain(const std::shared_ptr<DX::DeviceResources>& deviceResources, StreamPage^ streamPage, MoonlightClient* client, StreamConfiguration^ configuration);
		~moonlight_xbox_dxMain();
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void TrackingUpdate(float positionX) { m_pointerLocationX = positionX; }
		void StartRenderLoop();
		void StopRenderLoop();
		void SetFlyoutOpened(bool value);
		Concurrency::critical_section& GetCriticalSection() { return m_criticalSection; }
		bool keyboardMode = false;
		void OnKeyDown(unsigned short virtualKey, char modifiers);
		void OnKeyUp(unsigned short virtualKey, char modifiers);
		// IDeviceNotify
		virtual void OnDeviceLost();
		virtual void OnDeviceRestored();
		void Disconnect();
		void CloseApp();
		void SendGuideButton(int duration);
		void SendWinAltB();
		bool ToggleLogs();
		bool ToggleStats();

		bool mouseMode = false;
		
		MoonlightClient* moonlightClient;
	private:
		void ProcessInput();
		void Update();
		bool Render();
		void RenderImGui();

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		std::shared_ptr<VideoRenderer> m_sceneRenderer;
		std::unique_ptr<LogRenderer>   m_LogRenderer;
		std::unique_ptr<StatsRenderer> m_statsTextRenderer;

		std::shared_ptr<Stats>         m_stats;

		Windows::Foundation::IAsyncAction^ m_renderLoopWorker;
		Windows::Foundation::IAsyncAction^ m_inputLoopWorker;
		Concurrency::critical_section m_criticalSection;

		// Rendering loop timer.
		DX::StepTimer m_timer;

		// Track current input pointer position.
		float m_pointerLocationX;
		bool insideFlyout = false;
		Windows::Gaming::Input::GamepadReading previousReading[8];
		StreamPage^ m_streamPage;
	};
	void usleep(unsigned int usec);
}
