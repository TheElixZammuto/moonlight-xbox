// clang-format off
#include "pch.h"
// clang-format on
#include "StatsRenderer.h"
#include "../Plot/ImGuiPlots.h"
#include "../Plot/PlotDesc.h"
#include "FFMpegDecoder.h"
#include "Utils.hpp"

using namespace DirectX;
using namespace moonlight_xbox_dx;
using namespace Microsoft::WRL;
using namespace Windows::UI::Core;

StatsRenderer::StatsRenderer(const std::shared_ptr<DX::DeviceResources> &deviceResources, const std::shared_ptr<Stats> &stats) :
    m_console(std::make_unique<DX::TextConsole>()),
    m_deviceResources(deviceResources),
    m_mutex(),
    m_visible(false),
    m_stats(stats) {
	m_console->SetForegroundColor(Colors::Yellow);
	// m_console->SetDebugOutput(true);

	CreateDeviceDependentResources();
}

void StatsRenderer::Update(DX::StepTimer const &timer) {
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
void StatsRenderer::Render(bool showImGui) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_visible) {
		m_console->Render();
		if (showImGui) {
			RenderGraphs();
		}
	}
}

struct clampData {
	const float *values;
	float maxVal;
};

static inline float clampGetter(void *data, int idx) {
	const clampData *c = static_cast<const clampData *>(data);
	return fminf(c->values[idx], c->maxVal);
}

void StatsRenderer::RenderGraphs() {
	// Track graph CPU overhead per frame
	int64_t plotStartQpc = QpcNow();

	// we malloc a buffer for each stat only once and reuse it each frame
	assert(PlotCount == 8);
	static float *buffers[8] = {
	    (float *)malloc(sizeof(float) * 512),
	    (float *)malloc(sizeof(float) * 512),
	    (float *)malloc(sizeof(float) * 512),
	    (float *)malloc(sizeof(float) * 512),
	    (float *)malloc(sizeof(float) * 512),
	    (float *)malloc(sizeof(float) * 512),
	    (float *)malloc(sizeof(float) * 512),
	    (float *)malloc(sizeof(float) * 512)};

	float graphW = 850.0f * (m_displayWidth / 3840.0f);
	float graphH = 120.0f * (m_displayHeight / 2160.0f);
	float opacity = 0.8f;

	LogOnce("Drawing graphs of size %.1f x %.1f in viewport %d x %d using opacity %.2f\n",
	        graphW, graphH, m_displayWidth, m_displayHeight, opacity);

	// Row 1: 3 graphs
	// Row 2: 2 graphs left-aligned
	float itemSpacingX = ImGui::GetStyle().ItemSpacing.x;
	float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
	float row1Width = (3 * graphW) + (2 * itemSpacingX);
	float totalHeight = (3 * graphH) + (2 * itemSpacingY) + 50;

	// Anchor window to top-right
	ImVec2 windowPos(m_displayWidth - 10.0f, 10.0f); // 10px margin
	ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(row1Width, totalHeight), ImGuiCond_Always);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
	                         ImGuiWindowFlags_NoMove |
	                         ImGuiWindowFlags_NoNavFocus |
	                         ImGuiWindowFlags_NoBackground |
	                         ImGuiWindowFlags_NoSavedSettings;
	ImGui::Begin("##Stats", nullptr, flags);

	auto draw_plot = [&](int i, float width, float height) {
		Plot &plot = ImGuiPlots::instance().get(i);

		float minY = 0.0f;
		float maxY = 0.0f;
		std::size_t countF = plot.buffer.copyInto(buffers[i], 512, minY, maxY);
		float avgF = plot.buffer.average();
		if (!countF) {
			return;
		}

		char label[64];
		switch (plot.desc.labelType) {
		case PLOT_LABEL_MIN_MAX_AVG:
			snprintf(label, sizeof(label), "%s  %.1f / %.1f / %.1f %s", plot.desc.title, minY, maxY, avgF, plot.desc.unit);
			break;
		case PLOT_LABEL_MIN_MAX_AVG_INT:
			snprintf(label, sizeof(label), "%s  %d / %d / %.1f %s", plot.desc.title, (int)minY, (int)maxY, avgF, plot.desc.unit);
			break;
		case PLOT_LABEL_TOTAL_INT:
			snprintf(label, sizeof(label), "%s  %d %s", plot.desc.title, (int)plot.buffer.sum(), plot.desc.unit);
			break;
		}
		float scaleMin = FLT_MAX;
		float scaleMax = FLT_MAX;
		if (plot.desc.scaleTarget) {
			// optionally center the graph on a target such as the ideal frametime
			float ideal = (float)plot.desc.scaleTarget;
			scaleMin = ideal - (2 * ideal);
			scaleMax = ideal + (2 * ideal);
		}
		if (plot.desc.scaleMin)
			scaleMin = plot.desc.scaleMin;
		if (plot.desc.scaleMax)
			scaleMax = plot.desc.scaleMax;

		ImGui::PushID(i);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.19f, 0.19f, 0.19f, opacity)); // dark
		ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));     // green
		if (plot.desc.clampMax > 0.0) {
			clampData ctx{buffers[i], plot.desc.clampMax};
			ImGui::PlotLines("##xx", clampGetter, &ctx, countF, 0, (countF > 0 ? label : "no data"), scaleMin, scaleMax, ImVec2(width, height));
		} else {
			ImGui::PlotLines("##xx", buffers[i], countF, 0, (countF > 0 ? label : "no data"), scaleMin, scaleMax, ImVec2(width, height));
		}
		ImGui::PopStyleColor(2);
		ImGui::PopID();
	};

	const int row1[3] = {PLOT_FRAMETIME, PLOT_VSYNC_INTERVAL, PLOT_DROPPED_NETWORK};
	for (int c = 0; c < 3; ++c) {
		if (c > 0) ImGui::SameLine(0.0f, itemSpacingX);
		draw_plot(row1[c], graphW, graphH);
	}

	ImGui::Dummy(ImVec2(1.0f, itemSpacingY));
	const int row2[3] = {PLOT_HOST_FRAMETIME, PLOT_PRESENT_PACING, PLOT_DROPPED_PACER};
	for (int c = 0; c < 3; ++c) {
		if (c > 0) ImGui::SameLine(0.0f, itemSpacingX);
		draw_plot(row2[c], graphW, graphH);
	}

	// 3rd row for quickly graphing something if needed
	// ImGui::Dummy(ImVec2(1.0f, itemSpacingY));
	// draw_plot(PLOT_ETC, graphW, graphH);

	ImGui::End();

	ImGuiPlots::instance().observeFloat(PLOT_OVERHEAD, (float)QpcToMs(QpcNow() - plotStartQpc));
}

void StatsRenderer::CreateDeviceDependentResources() {
	m_deviceResources->GetUWPPixelDimensions(&m_displayWidth, &m_displayHeight);

	const wchar_t *font = L"Assets\\Font\\ModeSeven-24.spritefont"; // sized for 4K
	if (m_displayHeight <= 1440) {
		font = L"Assets\\Font\\ModeSeven-12.spritefont"; // for 1080p & 1440p
	}

	m_console->RestoreDevice(m_deviceResources->GetD3DDeviceContext(), font);

	// use much faster font rendering
	m_console->SetFixedWidthFont(true);
}

void StatsRenderer::CreateWindowSizeDependentResources() {
	int left = 10;
	int right = m_displayWidth / 3;
	int bottom = 0;

	if (m_displayHeight >= 2160) { // 24pt font
		left = 20;
		right = m_displayWidth / 2;
		bottom = 448;
	} else if (m_displayHeight >= 1440) { // 12pt font
		left = 14;
		bottom = 224;
	} else {
		left = 10;
		bottom = 168;
	}

	// The size of our text area (left, top, right, bottom)
	RECT size = {left, left, right, bottom};

	m_console->SetWindow(size);
}

void StatsRenderer::ReleaseDeviceDependentResources() {
	m_console->ReleaseDevice();
}

void StatsRenderer::ToggleVisible() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_visible = m_visible == true ? false : true;
}
