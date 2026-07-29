#pragma once
#include "UI\Backgrounds\Spheres\SpheresBackground.g.h"
#include <vector>
#include <random>

namespace moonlight_xbox_dx {

static const int kShapeCircle   = 0;
static const int kShapeSquare   = 1;
static const int kShapeTriangle = 2;

struct SphereState {
    float x, y;
    float vx, vy;
    float radius;
    float opacity;
    float spinAngle;
    float spinSpeed;
    int   shapeType;
};

public ref class SpheresBackground sealed {
public:
    SpheresBackground();
    void StartAnimations();
    void StopAnimations();
    void ReloadColors();
private:
    Windows::UI::Xaml::DispatcherTimer^ m_timer;
    Windows::Foundation::EventRegistrationToken m_tickToken;
    std::vector<SphereState> m_spheres;
    float m_canvasW = 0;
    float m_canvasH = 0;
    bool  m_initialized = false;
    std::mt19937 m_rng;
    Windows::UI::Color m_palette[3];
    int m_shapeMode = 0;

    void Canvas_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
    void OnTick(Platform::Object^ sender, Platform::Object^ args);
    void InitSpheres();
    void LoadPalette();
    void LoadShapeMode();
};

}
