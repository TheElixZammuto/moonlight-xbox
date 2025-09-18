// Defines available plots.

#pragma once
#include <array>

enum PlotType
{
	PLOT_FRAMETIME = 0,
	PLOT_HOST_FRAMETIME,
	PLOT_QUEUED_FRAMES,
	PLOT_DROPPED_NETWORK,
	PLOT_DROPPED_PACER_BACK,
	PLOT_DROPPED_PACER_FRONT,
	PLOT_OVERHEAD,
	PLOT_ETC,
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
    {"Frametime",                      PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1f, 50.0f, 0.0f, 49.9f},
    {"Host Frametime",                 PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1f, 50.0f, 0.0f, 49.9f},
    {"Queued Frames",                  PLOT_LABEL_MIN_MAX_AVG,   "", -1.0f, 10.0f, 0.0f, 0.0f},
    {"Dropped Frames (network)",       PLOT_LABEL_TOTAL_INT,     "", -1.0f, 3.0f, 0.0f, 0.0f},
    {"Dropped Frames (back pacing)",   PLOT_LABEL_TOTAL_INT,     "", -1.0f, 3.0f, 0.0f, 0.0f},
    {"Dropped Frames (front pacing)",  PLOT_LABEL_TOTAL_INT,     "", -1.0f, 3.0f, 0.0f, 0.0f},
    {"Graph overhead",                 PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1f, 6.0f, 0.0f, 0.0f},
	{"Etc...",                         PLOT_LABEL_MIN_MAX_AVG, "ms", -0.1f, 50.0f, 0.0f, 49.9f},
}};
