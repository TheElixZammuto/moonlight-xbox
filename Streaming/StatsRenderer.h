#pragma once

#include <memory>
#include <mutex>
#include <string>
#include "..\Common\StepTimer.h"
#include "..\Common\TextConsole.h"
#include "..\State\Stats.h"

namespace moonlight_xbox_dx
{
	class StatsRenderer
	{
	public:
		StatsRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, const std::shared_ptr<Stats>& stats);
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::StepTimer const& timer);
		void Render();
		void SetVisible(bool visible);

	private:
		std::mutex                                m_mutex;
		std::shared_ptr<DX::DeviceResources>      m_deviceResources;
		std::unique_ptr<DX::TextConsole>          m_console;
		std::shared_ptr<Stats>                    m_stats;
		bool                                      m_visible;
		uint32_t                                  m_displayWidth;
		uint32_t                                  m_displayHeight;
	};
}
