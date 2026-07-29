#pragma once
#include "UI\Backgrounds\Blobs\BlobsBackground.g.h"
#include <vector>
#include <random>

namespace moonlight_xbox_dx {

static const int kBlobCount  = 3;
static const int kBlobPts    = 6;

struct BlobData {
    float cx, cy;
    float baseRadius;
    float time;
    float timeSpeed;

    float rAmp[kBlobPts];
    float rPhase[kBlobPts];
    float rFreq[kBlobPts];
    float aAmp[kBlobPts];
    float aFreq[kBlobPts];
    float aPhase[kBlobPts];

    Windows::UI::Xaml::Media::PathFigure^    figure;
    Windows::UI::Xaml::Media::BezierSegment^ s0;
    Windows::UI::Xaml::Media::BezierSegment^ s1;
    Windows::UI::Xaml::Media::BezierSegment^ s2;
    Windows::UI::Xaml::Media::BezierSegment^ s3;
    Windows::UI::Xaml::Media::BezierSegment^ s4;
    Windows::UI::Xaml::Media::BezierSegment^ s5;
};

public ref class BlobsBackground sealed {
public:
    BlobsBackground();
    void StartAnimations();
    void StopAnimations();
    void ReloadColors();
private:
    Windows::UI::Color m_palette[4];
    void LoadPalette();
    Windows::UI::Xaml::DispatcherTimer^ m_timer;
    Windows::Foundation::EventRegistrationToken m_tickToken;
    std::vector<BlobData>               m_blobs;
    float m_canvasW  = 0;
    float m_canvasH  = 0;
    bool  m_ready    = false;
    std::mt19937 m_rng;

    void Canvas_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
    void OnTick(Platform::Object^ sender, Platform::Object^ args);
    void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    void InitBlobs();
    void UpdateBlob(BlobData& b);
};

}
