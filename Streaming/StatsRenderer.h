#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include "..\Common\StepTimer.h"
#include "..\Common\TextConsole.h"
#include "..\State\Stats.h"

namespace moonlight_xbox_dx {
class StatsRenderer
{
  public:
	StatsRenderer(const std::shared_ptr<DX::DeviceResources> &deviceResources);
	void CreateDeviceDependentResources();
	void CreateWindowSizeDependentResources();
	void ReleaseDeviceDependentResources();
	void Update(DX::StepTimer const &timer);
	void Render(bool showImGui);
	void RenderGraphs();
	bool GetVisible();
	void SetVisible(bool visible);
	void ToggleVisible();

  private:
	std::mutex m_mutex;
	std::shared_ptr<DX::DeviceResources> m_deviceResources;
	std::unique_ptr<DX::TextConsole> m_console;
	bool m_visible;
	uint32_t m_displayWidth;
	uint32_t m_displayHeight;
};
} // namespace moonlight_xbox_dx
