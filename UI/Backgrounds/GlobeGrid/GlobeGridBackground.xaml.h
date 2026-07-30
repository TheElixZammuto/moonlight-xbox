#pragma once
#include "UI\Backgrounds\GlobeGrid\GlobeGridBackground.g.h"
#include <vector>
#include <random>

namespace moonlight_xbox_dx {

struct GlobeStarState {
    float angle;
    float radius;
    float speed;
    float size;
    float baseOpacity;
    float phase;
};

public ref class GlobeGridBackground sealed {
public:
    GlobeGridBackground();
    void StartAnimations();
    void StopAnimations();
private:
    Windows::UI::Xaml::DispatcherTimer^ m_timer;
    Windows::Foundation::EventRegistrationToken m_tickToken;
    std::vector<GlobeStarState>         m_stars;
    std::mt19937                        m_rng;

    float m_canvasW      = 0.0f;
    float m_canvasH      = 0.0f;
    bool  m_initialized  = false;
    bool  m_itemsCreated = false;
    float m_rotY         = 0.0f;

    Windows::Foundation::Point ProjectSphere(float latRad, float lonRad);
    void Canvas_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
    void OnTick(Platform::Object^ sender, Platform::Object^ args);
    void InitItems();
    void UpdateGlobe();
    void UpdateStars();
};

}
