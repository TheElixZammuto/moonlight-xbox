#include "pch.h"
#include "UI\Backgrounds\GlobeGrid\GlobeGridBackground.xaml.h"
#include <cmath>
#include <algorithm>

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Shapes;

static const int   kLats      = 9;
static const float kLatDeg[]  = {-80,-60,-40,-20,0,20,40,60,80};
static const int   kLons      = 12;
static const int   kFrontSamp = 48;
static const int   kLonSamp   = 30;
static const float kPi        = 3.14159265f;
static const float kRotSpeed  = 0.0015f;
static const float kGlobeScale= 0.48f;
static const int   kStars     = 180;

GlobeGridBackground::GlobeGridBackground()
{
    m_rng = std::mt19937(std::random_device{}());
    InitializeComponent();
    TimeSpan ts;
    ts.Duration = 16 * 10000LL;
    m_timer = ref new DispatcherTimer();
    m_timer->Interval = ts;
    Platform::WeakReference weakSelf(this);
    m_tickToken = m_timer->Tick += ref new EventHandler<Object^>(
        [weakSelf](Object^, Object^) {
            try {
                auto self = weakSelf.Resolve<GlobeGridBackground>();
                if (self) self->OnTick(nullptr, nullptr);
            } catch (Platform::DisconnectedException^) {}
              catch (...) {}
        });
}

void GlobeGridBackground::Canvas_SizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    m_canvasW     = static_cast<float>(e->NewSize.Width);
    m_canvasH     = static_cast<float>(e->NewSize.Height);
    m_initialized = (m_canvasW > 0 && m_canvasH > 0);
}

Point GlobeGridBackground::ProjectSphere(float latRad, float lonRad)
{
    float r      = kGlobeScale * std::min(m_canvasW, m_canvasH) * 0.5f;
    float lonView= lonRad + m_rotY;
    float x3     = r * cosf(latRad) * sinf(lonView);
    float y3     = r * sinf(latRad);
    return Point(m_canvasW * 0.5f + x3, m_canvasH * 0.5f - y3);
}

void GlobeGridBackground::InitItems()
{
    WireCanvas->Children->Clear();
    StarCanvas->Children->Clear();
    m_stars.clear();

    Color gold = ColorHelper::FromArgb(255, 255, 196,  64);
    Color cyan = ColorHelper::FromArgb(255,  64, 210, 255);

    for (int i = 0; i < kLats; ++i) {
        auto pl = ref new Polyline();
        pl->Stroke          = ref new SolidColorBrush(gold);
        pl->StrokeThickness = 1.0;
        pl->Opacity         = 0.18;
        auto pts = ref new PointCollection();
        for (int k = 0; k <= kFrontSamp; ++k) pts->Append(Point(0, 0));
        pl->Points = pts;
        WireCanvas->Children->Append(pl);
    }

    for (int i = 0; i < kLats; ++i) {
        auto pl = ref new Polyline();
        if (i == 4) {
            pl->Stroke          = ref new SolidColorBrush(cyan);
            pl->StrokeThickness = 2.0;
        } else {
            pl->Stroke          = ref new SolidColorBrush(gold);
            pl->StrokeThickness = 1.5;
        }
        pl->Opacity = 1.0;
        auto pts = ref new PointCollection();
        for (int k = 0; k <= kFrontSamp; ++k) pts->Append(Point(0, 0));
        pl->Points = pts;
        WireCanvas->Children->Append(pl);
    }

    for (int j = 0; j < kLons; ++j) {
        auto pl = ref new Polyline();
        pl->Stroke          = ref new SolidColorBrush(gold);
        pl->StrokeThickness = 1.5;
        pl->Opacity         = 1.0;
        auto pts = ref new PointCollection();
        for (int k = 0; k <= kLonSamp; ++k) pts->Append(Point(0, 0));
        pl->Points = pts;
        WireCanvas->Children->Append(pl);
    }

    float maxR = hypotf(m_canvasW, m_canvasH) * 0.56f;
    std::uniform_real_distribution<float> distUnit(0.0f, 1.0f);
    std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * kPi);

    for (int i = 0; i < kStars; ++i) {
        GlobeStarState s;
        s.radius      = sqrtf(distUnit(m_rng)) * maxR;
        s.angle       = distAngle(m_rng);

        float speedBase = 0.00025f + distUnit(m_rng) * 0.00035f;
        s.speed       = speedBase * (1.0f + 0.6f * (1.0f - s.radius / maxR));
        s.size        = 1.0f + distUnit(m_rng) * 2.2f;
        s.baseOpacity = 0.25f + distUnit(m_rng) * 0.75f;
        s.phase       = distAngle(m_rng);
        m_stars.push_back(s);

        auto star = ref new Ellipse();
        star->Width  = s.size;
        star->Height = s.size;
        star->Fill   = ref new SolidColorBrush(Colors::White);
        star->Opacity = s.baseOpacity;
        StarCanvas->Children->Append(star);
    }

    m_itemsCreated = true;
    UpdateGlobe();
    UpdateStars();
}

void GlobeGridBackground::UpdateGlobe()
{

    for (int i = 0; i < kLats; ++i) {
        float latR = kLatDeg[i] * kPi / 180.0f;
        auto  pl   = safe_cast<Polyline^>(WireCanvas->Children->GetAt(i));
        auto  pts  = pl->Points;
        for (int k = 0; k <= kFrontSamp; ++k) {
            float lonView = kPi * 0.5f + k * kPi / static_cast<float>(kFrontSamp);
            pts->SetAt(k, ProjectSphere(latR, lonView - m_rotY));
        }
    }

    for (int i = 0; i < kLats; ++i) {
        float latR = kLatDeg[i] * kPi / 180.0f;
        auto  pl   = safe_cast<Polyline^>(WireCanvas->Children->GetAt(kLats + i));
        auto  pts  = pl->Points;
        for (int k = 0; k <= kFrontSamp; ++k) {
            float lonView = -kPi * 0.5f + k * kPi / static_cast<float>(kFrontSamp);
            pts->SetAt(k, ProjectSphere(latR, lonView - m_rotY));
        }
    }

    for (int j = 0; j < kLons; ++j) {
        float worldLon = j * 2.0f * kPi / static_cast<float>(kLons);
        float lonView  = worldLon + m_rotY;
        bool  isFront  = (cosf(lonView) > 0.0f);
        auto  pl       = safe_cast<Polyline^>(WireCanvas->Children->GetAt(2 * kLats + j));
        pl->Opacity    = isFront ? 1.0 : 0.18;
        auto  pts      = pl->Points;
        for (int k = 0; k <= kLonSamp; ++k) {
            float latR = (-80.0f + k * 160.0f / static_cast<float>(kLonSamp)) * kPi / 180.0f;
            pts->SetAt(k, ProjectSphere(latR, worldLon));
        }
    }
}

void GlobeGridBackground::UpdateStars()
{
    int count = static_cast<int>(m_stars.size());
    float cx = m_canvasW * 0.5f;
    float cy = m_canvasH * 0.5f;
    for (int i = 0; i < count; ++i) {
        auto& s = m_stars[i];
        s.angle += s.speed;
        float x = cx + cosf(s.angle) * s.radius;
        float y = cy + sinf(s.angle) * s.radius;
        auto star = safe_cast<Ellipse^>(StarCanvas->Children->GetAt(i));
        Canvas::SetLeft(star, x - s.size * 0.5f);
        Canvas::SetTop(star,  y - s.size * 0.5f);

        star->Opacity = s.baseOpacity * (0.60f + 0.40f * sinf(s.angle * 7.0f + s.phase));
    }
}

void GlobeGridBackground::OnTick(Object^ sender, Object^ args)
{
    if (!m_initialized) return;

    if (!m_itemsCreated) {
        InitItems();
        return;
    }

    m_rotY += kRotSpeed;
    if (m_rotY > 2.0f * kPi) m_rotY -= 2.0f * kPi;

    UpdateGlobe();
    UpdateStars();
}

void GlobeGridBackground::StartAnimations()
{
    if (m_timer) m_timer->Start();
}

void GlobeGridBackground::StopAnimations()
{
    if (m_timer) {
        m_timer->Stop();
        m_timer->Tick -= m_tickToken;
    }
}
