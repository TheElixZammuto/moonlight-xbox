// clang-format off
#include "pch.h"
// clang-format on
#include "ImGuiPlots.h"
#include <cassert>
#include <numeric>
#include <vector>
#include "PlotDesc.h"

Plot::Plot(const PlotDesc &d, std::size_t capacity) :
    desc(d), buffer(capacity) {}

// singleton
ImGuiPlots &ImGuiPlots::instance()
{
	static ImGuiPlots s;
	return s;
}

ImGuiPlots::ImGuiPlots() :
    plots_{{
        Plot(kPlotDescs[PLOT_FRAMETIME]),
        Plot(kPlotDescs[PLOT_HOST_FRAMETIME]),
        Plot(kPlotDescs[PLOT_QUEUED_FRAMES]),
        Plot(kPlotDescs[PLOT_DROPPED_NETWORK]),
        Plot(kPlotDescs[PLOT_DROPPED_PACER_BACK]),
        Plot(kPlotDescs[PLOT_DROPPED_PACER_FRONT]),
        Plot(kPlotDescs[PLOT_OVERHEAD]),

        Plot(kPlotDescs[PLOT_ETC]),
    }}
{
}

ImGuiPlots::~ImGuiPlots() = default;

void ImGuiPlots::clearData()
{
	for (auto &p : plots_)
		p.buffer.clear();
}

void ImGuiPlots::observeFloat(int plotId, float value)
{
	assert(plotId >= 0 && plotId < PlotCount);
	plots_[static_cast<std::size_t>(plotId)].buffer.push(static_cast<float>(value));
}
