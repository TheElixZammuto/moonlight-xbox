﻿#pragma once

#include <string>
#include "..\Common\StepTimer.h"
#include "..\Common\TextConsole.h"

namespace moonlight_xbox_dx
{
	class LogRenderer
	{
	public:
		LogRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::StepTimer const& timer);
		void Render();
		void SetVisible(bool visible);

	private:
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::unique_ptr<DX::TextConsole>     m_console;
		bool                                 m_visible;
		uint32_t                             m_displayWidth;
		uint32_t                             m_displayHeight;
	};
}
