// clang-format off
#include "pch.h"
// clang-format on
#include "ImGuiPlots.h"
#include <cassert>
#include <numeric>
#include <vector>
#include "PlotDesc.h"

Plot::Plot(const PlotDesc &d, std::size_t capacity)
    : desc(d),
      buffer(capacity) {}

// singleton
ImGuiPlots &ImGuiPlots::instance() {
	static ImGuiPlots s;
	return s;
}

ImGuiPlots::ImGuiPlots()
    : plots_{{
          Plot(kPlotDescs[PLOT_FRAMETIME]),
          Plot(kPlotDescs[PLOT_HOST_FRAMETIME]),
          Plot(kPlotDescs[PLOT_DROPPED_NETWORK]),
          Plot(kPlotDescs[PLOT_DROPPED_PACER]),
          Plot(kPlotDescs[PLOT_QUEUED_FRAMES]),
          Plot(kPlotDescs[PLOT_BANDWIDTH]),

          Plot(kPlotDescs[PLOT_AUDIO_BUFFER_MS]),
          Plot(kPlotDescs[PLOT_ETC]),
      }},
      m_isEnabled(true) {
    #ifdef TRACY_INCLUDE_PLOTS
        TracyPlotConfig(kPlotDescs[PLOT_FRAMETIME].title, tracy::PlotFormatType::Number, false, true, 0);
        TracyPlotConfig(kPlotDescs[PLOT_HOST_FRAMETIME].title, tracy::PlotFormatType::Number, false, true, 0);
        TracyPlotConfig(kPlotDescs[PLOT_DROPPED_NETWORK].title, tracy::PlotFormatType::Number, true, true, 0);
        TracyPlotConfig(kPlotDescs[PLOT_DROPPED_PACER].title, tracy::PlotFormatType::Number, true, true, 0);
        TracyPlotConfig(kPlotDescs[PLOT_QUEUED_FRAMES].title, tracy::PlotFormatType::Number, true, true, 0);
        TracyPlotConfig(kPlotDescs[PLOT_BANDWIDTH].title, tracy::PlotFormatType::Number, false, true, 0);
        TracyPlotConfig(kPlotDescs[PLOT_AUDIO_BUFFER_MS].title, tracy::PlotFormatType::Number, false, true, 0);
    #endif
}

ImGuiPlots::~ImGuiPlots() = default;

void ImGuiPlots::clearData() {
	for (auto &p : plots_)
		p.buffer.clear();
}

void ImGuiPlots::clearData(int plotId)
{
    assert(plotId >= 0 && plotId < PlotCount);
    plots_[static_cast<std::size_t>(plotId)].buffer.clear();
}

void ImGuiPlots::observeFloat(int plotId, float value)
{
    if (!m_isEnabled) return;
	assert(plotId >= 0 && plotId < PlotCount);
	plots_[static_cast<std::size_t>(plotId)].buffer.push(static_cast<float>(value));
#ifdef TRACY_INCLUDE_PLOTS
	TracyPlot(kPlotDescs[plotId].title, value);
#endif
}

float ImGuiPlots::observeFloatReturnAvg(int plotId, float value) {
	// still runs even if !m_isEnabled, caller needs the return average
	assert(plotId >= 0 && plotId < PlotCount);
	plots_[static_cast<std::size_t>(plotId)].buffer.push(static_cast<float>(value));
#ifdef TRACY_INCLUDE_PLOTS
	TracyPlot(kPlotDescs[plotId].title, value);
#endif
	return plots_[static_cast<std::size_t>(plotId)].buffer.average();
}

float ImGuiPlots::getAvg(int plotId) {
	assert(plotId >= 0 && plotId < PlotCount);
	return plots_[static_cast<std::size_t>(plotId)].buffer.average();
}

void ImGuiPlots::clearBuffer(int plotId) {
	assert(plotId >= 0 && plotId < PlotCount);
	plots_[static_cast<std::size_t>(plotId)].buffer.clear();
}
