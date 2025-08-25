#pragma once
#include "../Utils/FloatBuffer.h"
#include "PlotDesc.h"
#include <array>

struct Plot
{
	const PlotDesc &desc; // non-owning reference to metadata
	FloatBuffer buffer;   // runtime samples

	explicit Plot(const PlotDesc &d, std::size_t capacity = 512);
};

class ImGuiPlots
{
  public:
	static ImGuiPlots &instance();

	void clearData();
	void observeFloat(int plotId, float value);

	Plot &get(int plotId)
	{
		return plots_[static_cast<std::size_t>(plotId)];
	}
	const Plot &get(int plotId) const
	{
		return plots_[static_cast<std::size_t>(plotId)];
	}

	std::array<Plot, PlotCount> &plots()
	{
		return plots_;
	}
	const std::array<Plot, PlotCount> &plots() const
	{
		return plots_;
	}

  private:
	ImGuiPlots();
	~ImGuiPlots();

	std::array<Plot, PlotCount> plots_;
};
