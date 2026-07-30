#pragma once
#include "UI\Backgrounds\Spheres\SpheresSettingsControl.g.h"
#include "UI\Backgrounds\DynamicBackgroundHost.xaml.h"
#include "UI\Controls\SwatchPicker.xaml.h"

namespace moonlight_xbox_dx {

public ref class SpheresSettingsControl sealed {
public:
    SpheresSettingsControl();
    void Initialize(DynamicBackgroundHost^ host);
private:
    DynamicBackgroundHost^ m_host;
    bool m_initialized = false;

    void SchemeSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
    void ShapeSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
    void Color0_ColorChanged(Platform::Object^ sender, Windows::UI::Color color, bool useSystemAccent);
    void Color1_ColorChanged(Platform::Object^ sender, Windows::UI::Color color, bool useSystemAccent);
    void ResetButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    void UpdateCustomPanelVisibility();
};

}
