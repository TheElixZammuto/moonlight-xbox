#include "pch.h"
#include "StatsRenderer.h"
#include "Utils.hpp"

#include "Common/DirectXHelper.h"
#include <State\MoonlightClient.h>

using namespace moonlight_xbox_dx;
using namespace Microsoft::WRL;

// Initializes D2D resources used for text rendering.
StatsRenderer::StatsRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
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
void StatsRenderer::Update(DX::StepTimer const& timer)
{
	m_text = L"";
	if (Utils::showStats) {
		Windows::ApplicationModel::Package^ package = Windows::ApplicationModel::Package::Current;
		m_text += L"App Version: " + std::to_wstring(package->Id->Version.Major) + L"." + std::to_wstring(package->Id->Version.Minor) + L"." + std::to_wstring(package->Id->Version.Build) + L"." + std::to_wstring(package->Id->Version.Revision) + L"\n";
		m_text += L"Window Size: " + std::to_wstring(Utils::stats.outputW) + L" x " + std::to_wstring(Utils::stats.outputH) + L"\n";
		m_text += L"Composition Scale: " + std::to_wstring(Utils::stats.compositionScaleX) + L" - " + std::to_wstring(Utils::stats.compositionScaleY) + L" - " + std::to_wstring(Utils::stats.compositionScaleMultiplier) + L"\n";
		m_text += L"AVG FPS: " + std::to_wstring(Utils::stats.fps) + L"fps\n";
		m_text += L"AVG Rendering Time: " + std::to_wstring(Utils::stats.averageRenderingTime) + L"ms \n";
		m_text += L"AVG Network Time: " + std::to_wstring(Utils::stats.averageNetworkTime) + L"ms\n";
		m_text += L"AVG Total Time: " + std::to_wstring(Utils::stats.averageTotalTime) + L"ms \n";
		m_text += L"Queue Size: " + std::to_wstring(Utils::stats.queueSize) + L"\n";
	}
}

// Renders a frame to the screen.
void StatsRenderer::Render()
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

	Windows::Foundation::Size margin;
	margin.Width = 20;
	margin.Height = 10;

	// Position on the top left corner
	D2D1::Matrix3x2F screenTranslation = D2D1::Matrix3x2F::Translation(
		margin.Width,
		margin.Height
	);

	context->SetTransform(screenTranslation * m_deviceResources->GetOrientationTransform2D());

	DX::ThrowIfFailed(
		m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING)
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

void StatsRenderer::CreateDeviceDependentResources()
{
	DX::ThrowIfFailed(
		m_deviceResources->GetD2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), &m_whiteBrush)
	);
}
void StatsRenderer::ReleaseDeviceDependentResources()
{
	m_whiteBrush.Reset();
}