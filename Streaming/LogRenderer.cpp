﻿#include "pch.h"
#include "LogRenderer.h"
#include "Utils.hpp"

#include "Common/DirectXHelper.h"
#include <State\MoonlightClient.h>

using namespace DirectX;
using namespace moonlight_xbox_dx;
using namespace Microsoft::WRL;

LogRenderer::LogRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_console(std::make_unique<DX::TextConsole>()),
	m_deviceResources(deviceResources),
	m_visible(false)
{
	m_console->SetForegroundColor(Colors::Yellow);
	//m_console->SetDebugOutput(true);

	CreateDeviceDependentResources();
}

// Updates the text to be displayed.
void LogRenderer::Update(DX::StepTimer const& timer)
{
	// Only update the console once per second
	static double lastUpdateSeconds = 0.0;
	if (m_visible && timer.GetTotalSeconds() - lastUpdateSeconds >= 1.0) {
		m_console->Clear();

		Utils::logMutex.lock();
		std::vector<std::wstring> lines = Utils::GetLogLines();
		for (std::wstring line : lines) {
			m_console->Write(line.c_str());
		}
		Utils::logMutex.unlock();

		lastUpdateSeconds = timer.GetTotalSeconds();
	}
}

// Renders a frame to the screen.
void LogRenderer::Render()
{
	if (m_visible) {
		m_console->Render();
	}
}

void LogRenderer::CreateDeviceDependentResources()
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

void LogRenderer::CreateWindowSizeDependentResources()
{
	// The size of our text area (left, top, right, bottom)
	RECT size = {m_displayWidth * 0.5, 0, m_displayWidth - 20, m_displayHeight - 20};

	m_console->SetWindow(size);
}

void LogRenderer::ReleaseDeviceDependentResources()
{
	m_console->ReleaseDevice();
}

void LogRenderer::SetVisible(bool visible) {
	m_visible = visible;
}
