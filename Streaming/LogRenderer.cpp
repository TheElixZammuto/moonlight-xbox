﻿#include "pch.h"
#include "LogRenderer.h"
#include "Utils.hpp"

#include "Common/DirectXHelper.h"
#include <State\MoonlightClient.h>

using namespace moonlight_xbox_dx;
using namespace Microsoft::WRL;

// Initializes D2D resources used for text rendering.
LogRenderer::LogRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) : 
	m_text(L""),
	m_deviceResources(deviceResources)
{
	ZeroMemory(&m_textMetrics, sizeof(DWRITE_TEXT_METRICS));

	// Create device independent resources
	ComPtr<IDWriteTextFormat> textFormat;
	DX::ThrowIfFailed(
		m_deviceResources->GetDWriteFactory()->CreateTextFormat(
			L"Segoe UI",
			nullptr,
			DWRITE_FONT_WEIGHT_LIGHT,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			14.0f,
			L"en-US",
			&textFormat
			)
		);

	DX::ThrowIfFailed(
		textFormat.As(&m_textFormat)
		);

	DX::ThrowIfFailed(
		m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR)
		);

	DX::ThrowIfFailed(
		m_deviceResources->GetD2DFactory()->CreateDrawingStateBlock(&m_stateBlock)
		);

	CreateDeviceDependentResources();
}

// Updates the text to be displayed.
void LogRenderer::Update(DX::StepTimer const& timer)
{
	m_text = L"";
	if (Utils::showLogs) {
		Utils::logMutex.lock();
		std::vector<std::wstring> lines = Utils::GetLogLines();
		for (std::wstring line : lines) {
			m_text += line;
		}
		Utils::logMutex.unlock();
	}
	
}

// Renders a frame to the screen.
void LogRenderer::Render(FLOAT width, FLOAT height)
{
	ComPtr<IDWriteTextLayout> textLayout;
	DX::ThrowIfFailed(
		m_deviceResources->GetDWriteFactory()->CreateTextLayout(
			m_text.c_str(),
			(uint32)m_text.length(),
			m_textFormat.Get(),
			500.0f, // Max width of the input text.
			16.0f * 64.0f, // Max height of the input text.
			&textLayout
		)
	);

	DX::ThrowIfFailed(
		textLayout.As(&m_textLayout)
	);

	DX::ThrowIfFailed(
		m_textLayout->GetMetrics(&m_textMetrics)
	);
	ID2D1DeviceContext* context = m_deviceResources->GetD2DDeviceContext();
	Windows::Foundation::Size logicalSize = m_deviceResources->GetLogicalSize();

	context->SaveDrawingState(m_stateBlock.Get());
	context->BeginDraw();

	Windows::Foundation::Size screen;
	screen.Width = (logicalSize.Width / 1920) * width;
	screen.Height = (logicalSize.Height / 1080) * height;

	Windows::Foundation::Size margin;
	margin.Width = 30;
	margin.Height = 10;

	// Position on the bottom right corner
	D2D1::Matrix3x2F screenTranslation = D2D1::Matrix3x2F::Translation(
		screen.Width - m_textMetrics.layoutWidth - margin.Width,
		screen.Height - m_textMetrics.height - margin.Height
		);

	context->SetTransform(screenTranslation * m_deviceResources->GetOrientationTransform2D());

	DX::ThrowIfFailed(
		m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING)
		);

	context->DrawTextLayout(
		D2D1::Point2F(0.f, 0.f),
		m_textLayout.Get(),
		m_whiteBrush.Get()
		);

	// Ignore D2DERR_RECREATE_TARGET here. This error indicates that the device
	// is lost. It will be handled during the next call to Present.
	HRESULT hr = context->EndDraw();
	if (hr != D2DERR_RECREATE_TARGET)
	{
		DX::ThrowIfFailed(hr);
	}

	context->RestoreDrawingState(m_stateBlock.Get());
}

void LogRenderer::CreateDeviceDependentResources()
{
	DX::ThrowIfFailed(
		m_deviceResources->GetD2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), &m_whiteBrush)
		);
}
void LogRenderer::ReleaseDeviceDependentResources()
{
	m_whiteBrush.Reset();
}