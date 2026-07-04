#pragma once

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
	StatsRenderer(const std::shared_ptr<DX::DeviceResources> &deviceResources, const std::shared_ptr<Stats> &stats, Platform::String^ fontFamily, Platform::String^ colorName);
	void CreateDeviceDependentResources();
	void CreateWindowSizeDependentResources();
	void ReleaseDeviceDependentResources();
	void Update(DX::StepTimer const &timer);
	void Render(bool showImGui);
	void RenderGraphs();
	bool GetVisible()
	{
		return m_visible;
	}
	void SetVisible(bool visible)
	{
		m_visible = visible;
	}
	void ToggleVisible();
	// Sizes the text box for a single compact line (Lite) vs. the tall multi-line full-mode box.
	void SetLiteMode(bool lite) { m_liteMode = lite; }

  private:
	std::mutex m_mutex;
	std::shared_ptr<DX::DeviceResources> m_deviceResources;
	std::unique_ptr<DX::TextConsole> m_console;
	std::shared_ptr<Stats> m_stats;
	bool m_visible;
	bool m_liteMode = false;
	std::wstring m_fontFamily;
	std::wstring m_colorName;
	uint32_t m_displayWidth;
	uint32_t m_displayHeight;
};
} // namespace moonlight_xbox_dx
