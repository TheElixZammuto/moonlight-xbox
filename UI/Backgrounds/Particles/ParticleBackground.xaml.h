#pragma once
#include "UI\Backgrounds\Particles\ParticleBackground.g.h"
#include <vector>
#include <random>

namespace moonlight_xbox_dx {

struct ParticleState {
    float t;
    float tSpeed;
    float spreadY;
    float opacity;
    float opacityDelta;
    float opacityMin;
    float opacityMax;
    float size;
    bool isBokeh;
    int colorSlot;
};

public ref class ParticleBackground sealed {
public:
    ParticleBackground();
    void StartAnimations();
    void StopAnimations();
    void ReloadColors();
private:
    Windows::UI::Xaml::DispatcherTimer^ m_timer;
    Windows::Foundation::EventRegistrationToken m_tickToken;
    std::vector<ParticleState> m_particles;
    float m_canvasW = 0;
    float m_canvasH = 0;
    float m_wavePhase = 0.0f;
    bool m_initialized = false;
    std::mt19937 m_rng;
    Windows::UI::Color m_colorA;
    Windows::UI::Color m_colorB;
    Windows::UI::Color m_gradientColor;
    Windows::UI::Xaml::Media::LinearGradientBrush^ m_bgBrush;

    void Canvas_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
    void OnTick(Platform::Object^ sender, Platform::Object^ args);
    void InitParticles();
    void LoadPalette();
    void ApplyGradient();
    float WaveY(float t);
};

}
