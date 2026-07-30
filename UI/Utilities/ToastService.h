#pragma once
#include "pch.h"

namespace moonlight_xbox_dx {

void InitializeToastService(Windows::UI::Xaml::Controls::Grid^ rootGrid);
void ShowToast(Platform::String^ message);

}
