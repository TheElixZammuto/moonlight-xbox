#pragma once
#include "UI\Backgrounds\Streaks\StreaksBackground.g.h"
#include <vector>
#include <random>

namespace moonlight_xbox_dx {

struct StreakState {
    float t;
    float lane;
    float halfLen;
    float speed;
    float glowH;
    float coreH;
    int   colorIndex;
};

public ref class StreaksBackground sealed {
public:
    StreaksBackground();
    void StartAnimations();
    void StopAnimations();
    void ReloadColors();
private:
    Windows::UI::Xaml::DispatcherTimer^ m_timer;
    Windows::Foundation::EventRegistrationToken m_tickToken;
    std::vector<StreakState> m_streaks;
    Windows::UI::Color m_palette[5];
    float m_canvasW = 0;
    float m_canvasH = 0;
    bool  m_initialized = false;
    std::mt19937 m_rng;

    void Canvas_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
    void OnTick(Platform::Object^ sender, Platform::Object^ args);
    void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    void InitStreaks();
    void LoadPalette();
};

}
