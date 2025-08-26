// Defines available plots.

#pragma once
#include <array>

enum PlotType
{
	PLOT_FRAMETIME = 0,
	PLOT_HOST_FRAMETIME,
	PLOT_QUEUED_FRAMES,
	PLOT_DROPPED_NETWORK,
	PLOT_DROPPED_PACER,
	PLOT_OVERHEAD,
	PlotCount
};

enum PlotLabelType
{
	PLOT_LABEL_MIN_MAX_AVG = 0,
	PLOT_LABEL_MIN_MAX_AVG_INT,
	PLOT_LABEL_TOTAL_INT
};

struct PlotDesc
{
	const char *title;
	PlotLabelType labelType;
	const char *unit;
	float scaleMin;
	float scaleMax;
	float scaleTarget;
	float clampMax;
};

// clang-format off
inline constexpr std::array<PlotDesc, PlotCount> kPlotDescs = {{
    {"Frametime",                      PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1, 50.0, 0.0, 49.9},
    {"Host Frametime",                 PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1, 50.0, 0.0, 49.9},
    {"Queued Frames",                  PLOT_LABEL_MIN_MAX_AVG,   "", -0.1, 5.0, 0.0, 0.0},
    {"Dropped Frames (network)",       PLOT_LABEL_TOTAL_INT,     "", -0.1, 5.0, 0.0, 0.0},
    {"Dropped Frames (pacing jitter)", PLOT_LABEL_TOTAL_INT,     "", -0.1, 5.0, 0.0, 0.0},
    {"Graph overhead",                 PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1, 5.0, 0.0, 0.0},
}};
