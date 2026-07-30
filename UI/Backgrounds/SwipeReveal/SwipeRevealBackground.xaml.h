#pragma once
#include "UI\Backgrounds\SwipeReveal\SwipeRevealBackground.g.h"
#include "State\MoonlightHost.h"
#include <vector>

namespace moonlight_xbox_dx {

public ref class SwipeRevealBackground sealed {
public:
    SwipeRevealBackground();
    void StartAnimations();
    void StopAnimations();
    void SetHosts(Windows::Foundation::Collections::IVector<MoonlightHost^>^ hosts);
private:
    Windows::UI::Xaml::DispatcherTimer^                          m_timer;
    Windows::Foundation::EventRegistrationToken                  m_tickToken;
    Windows::Foundation::Collections::IVector<MoonlightHost^>^  m_hosts;
    Platform::Collections::Vector<MoonlightApp^>^                m_apps;

    Windows::UI::Xaml::Media::ImageBrush^                        m_frontBrush;
    Windows::UI::Xaml::Media::CompositeTransform^                m_frontPan;
    Windows::UI::Xaml::Media::RectangleGeometry^                 m_frontClipRect;
    Windows::UI::Xaml::Media::SkewTransform^                     m_frontGridSkew;
    Windows::UI::Xaml::Media::SkewTransform^                     m_frontRectInvSkew;

    float m_canvasW        = 0.0f;
    float m_canvasH        = 0.0f;
    bool  m_initialized    = false;
    bool  m_appsLoaded     = false;

    int   m_phase          = 0;
    int   m_holdTick       = 0;
    int   m_wipeTick       = 0;
    int   m_wipeDir        = 1;
    int   m_loadRetryTick  = 0;
    int   m_imageRetryTick = 0;
    int   m_introTick      = 0;
    int   m_backAppIdx     = -1;
    int   m_frontAppIdx    = -1;
    int   m_panDirIdx      = 0;

    float m_backPanX  = 0.0f, m_backPanY  = 0.0f;
    float m_backVX    = 0.0f, m_backVY    = 0.0f;
    float m_frontPanX = 0.0f, m_frontPanY = 0.0f;
    float m_frontVX   = 0.0f, m_frontVY   = 0.0f;

    void Grid_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
    void OnTick(Platform::Object^ sender, Platform::Object^ args);
    void LoadAppsAsync();
    void LoadFromNetworkAsync();
    void ShuffleAndApply(Platform::Collections::Vector<MoonlightApp^>^ apps);
    void InitSlides();
    void InitSlidesWithWipe();
    void AdvanceSlide();
    int  FindNextAppWithImage(int startIdx);
    void InitPanForLayer(float& px, float& py, float& vx, float& vy);
    void AdvancePan(float& px, float& py, float vx, float vy,
                    Windows::UI::Xaml::Media::CompositeTransform^ xf);
    void UpdateDiagonalClip(float swept);
    void ZeroFrontClips();
    void UpdateGlassEdgeSkew();
};

}
