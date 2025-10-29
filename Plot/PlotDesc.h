// Defines available plots.

#pragma once
#include <array>

enum PlotType {
	PLOT_FRAMETIME = 0,
	PLOT_HOST_FRAMETIME,
	PLOT_VSYNC_INTERVAL,
	PLOT_DROPPED_NETWORK,
	PLOT_DROPPED_PACER,
	PLOT_PRESENT_PACING,

	PLOT_OVERHEAD,
	PLOT_ETC,
	PlotCount
};

enum PlotLabelType {
	PLOT_LABEL_MIN_MAX_AVG = 0,
	PLOT_LABEL_MIN_MAX_AVG_INT,
	PLOT_LABEL_TOTAL_INT
};

struct PlotDesc {
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
    {"Frametime",                      PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1f, 50.0f, 0.0f, 41.0f},
    {"Host Frametime",                 PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1f, 50.0f, 0.0f, 41.0f},
    {"Vsync interval",                 PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1f, 50.0f, 0.0f, 0.0f},
    {"Dropped frames (network)",       PLOT_LABEL_TOTAL_INT,     "", -1.0f, 3.0f, 0.0f, 0.0f},
    {"Dropped frames (pacing)",        PLOT_LABEL_TOTAL_INT,     "", -1.0f, 3.0f, 0.0f, 0.0f},
    {"Present to display latency",     PLOT_LABEL_MIN_MAX_AVG, "ms", -10.0f, 25.0f, 0.0f, 0.0f},
    {"Graph overhead",                 PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1f, 6.0f, 0.0f, 0.0f},
	{"Etc...",                         PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1f, 50.0f, 0.0f, 49.9f},
}};
