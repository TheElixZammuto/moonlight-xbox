#include "pch.h"
#include "StatsRenderer.h"
#include "FFMpegDecoder.h"
#include "Utils.hpp"

using namespace DirectX;
using namespace moonlight_xbox_dx;
using namespace Microsoft::WRL;
using namespace Windows::UI::Core;

StatsRenderer::StatsRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, const std::shared_ptr<Stats>& stats) :
	m_console(std::make_unique<DX::TextConsole>()),
	m_deviceResources(deviceResources),
	m_visible(false),
	m_stats(stats)
{
	m_console->SetForegroundColor(Colors::Yellow);
	//m_console->SetDebugOutput(true);

	CreateDeviceDependentResources();
}

void StatsRenderer::Update(DX::StepTimer const& timer)
{
	// We let the Stats class always process even if not visible. Most of the time
	// it will simply accumulate stats during its 1-second window period. Each second,
	// when it determines the user-visible text should be updated, it will update outputStr and return true.

	char outputStr[1024]; // char is used so we can share more of the formatting code with moonlight-qt
	wchar_t wideStr[2048];

	if (m_stats->ShouldUpdateDisplay(timer, m_visible, outputStr, sizeof(outputStr))) {
		size_t numChars = mbstowcs(wideStr, outputStr, 1024);
		if (numChars != -1) {
			m_console->Clear();
			m_console->Write(wideStr);
		}
	}
}

// Renders a frame to the screen.
void StatsRenderer::Render()
{
	if (m_visible) {
		m_console->Render();
	}
}

void StatsRenderer::CreateDeviceDependentResources()
{
	m_deviceResources->GetUWPPixelDimensions(&m_displayWidth, &m_displayHeight);

	const wchar_t* font = L"Assets\\Font\\ModeSeven-24.spritefont"; // sized for 4K
	if (m_displayHeight <= 1440) {
		font = L"Assets\\Font\\ModeSeven-12.spritefont"; // for 1080p & 1440p
	}

	m_console->RestoreDevice(m_deviceResources->GetD3DDeviceContext(), font);

	// use much faster font rendering
	m_console->SetFixedWidthFont(true);
}

void StatsRenderer::CreateWindowSizeDependentResources()
{
	// The size of our text area (left, top, right, bottom)
	RECT size = {20, 0, m_displayWidth * 0.5, m_displayHeight * 0.2};

	m_console->SetWindow(size);
}

void StatsRenderer::ReleaseDeviceDependentResources()
{
	m_console->ReleaseDevice();
}

void StatsRenderer::SetVisible(bool visible) {
	m_visible = visible;
}
