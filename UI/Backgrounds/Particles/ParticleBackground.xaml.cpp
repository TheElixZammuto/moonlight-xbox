#include "pch.h"
#include "UI\Backgrounds\Particles\ParticleBackground.xaml.h"
#include <cmath>

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Shapes;
using namespace Windows::UI::Xaml::Media;

static Color LerpRGB(Color a, Color b, float t, uint8_t alpha = 255) {
    auto ch = [](uint8_t x, uint8_t y, float f) -> uint8_t {
        return static_cast<uint8_t>(x + (static_cast<int>(y) - static_cast<int>(x)) * f);
    };
    return ColorHelper::FromArgb(alpha, ch(a.R, b.R, t), ch(a.G, b.G, t), ch(a.B, b.B, t));
}

static Color ScaleRGB(Color c, float s, uint8_t alpha = 255) {
    return ColorHelper::FromArgb(alpha,
        static_cast<uint8_t>(fminf(c.R * s, 255.0f)),
        static_cast<uint8_t>(fminf(c.G * s, 255.0f)),
        static_cast<uint8_t>(fminf(c.B * s, 255.0f)));
}

static bool ParseHex6(Platform::String^ s, Color& out)
{
    if (s == nullptr || s->Length() != 6) return false;
    const wchar_t* p = s->Data();
    wchar_t buf[7]; wcsncpy_s(buf, p, 6); buf[6] = L'\0';
    wchar_t* end = nullptr;
    unsigned long v = wcstoul(buf, &end, 16);
    if (end != buf + 6) return false;
    out.A = 255;
    out.R = (uint8_t)((v >> 16) & 0xFF);
    out.G = (uint8_t)((v >>  8) & 0xFF);
    out.B = (uint8_t)( v        & 0xFF);
    return true;
}

static const int kBokehCount    = 50;
static const int kSmallCount    = 130;
static const int kParticleCount = kBokehCount + kSmallCount;
static const float kPi          = 3.14159265f;
static const float kSpreadRange = 0.18f;

float ParticleBackground::WaveY(float t) {
    float centerY = m_canvasH * (0.80f - t * 0.60f);
    return centerY + m_canvasH * 0.10f * sinf(t * kPi * 3.0f + m_wavePhase);
}

void ParticleBackground::LoadPalette()
{
    using namespace Windows::Storage;
    Platform::String^ scheme = L"champagne";
    auto ls = ApplicationData::Current->LocalSettings->Values;
    if (ls->HasKey("particles.scheme"))
        scheme = safe_cast<Platform::String^>(ls->Lookup("particles.scheme"));

    struct { Color particle; Color gradient; } presets[] = {
        { {255,210,165, 75}, {255, 50, 30,120} },
        { {255,255,120, 40}, {255, 15, 80, 90} },
        { {255, 80,215,190}, {255, 90, 15,150} },
        { {255,180, 90,230}, {255, 10, 20,100} },
        { {255,235,110,150}, {255, 15, 85, 60} },
    };
    int idx = 0;
    if      (scheme->Equals(L"ember"))   idx = 1;
    else if (scheme->Equals(L"aurora"))  idx = 2;
    else if (scheme->Equals(L"nebula"))  idx = 3;
    else if (scheme->Equals(L"blossom")) idx = 4;

    if (scheme->Equals(L"custom")) {
        m_colorA       = presets[0].particle;
        m_gradientColor = presets[0].gradient;
        if (ls->HasKey("particles.custom.0"))
            ParseHex6(safe_cast<Platform::String^>(ls->Lookup("particles.custom.0")), m_colorA);
        if (ls->HasKey("particles.custom.1"))
            ParseHex6(safe_cast<Platform::String^>(ls->Lookup("particles.custom.1")), m_gradientColor);
    } else {
        m_colorA        = presets[idx].particle;
        m_gradientColor = presets[idx].gradient;
    }
    m_colorB = ScaleRGB(m_colorA, 0.35f);
}

ParticleBackground::ParticleBackground()
{
    m_rng = std::mt19937(std::random_device{}());
    InitializeComponent();

    TimeSpan interval;
    interval.Duration = 33 * 10000LL;
    m_timer = ref new DispatcherTimer();
    m_timer->Interval = interval;
    Platform::WeakReference weakSelf(this);
    m_tickToken = m_timer->Tick += ref new EventHandler<Object^>(
        [weakSelf](Object^, Object^) {
            try {
                auto self = weakSelf.Resolve<ParticleBackground>();
                if (self) self->OnTick(nullptr, nullptr);
            } catch (Platform::DisconnectedException^) {}
              catch (...) {}
        });
}

void ParticleBackground::Canvas_SizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    m_canvasW = static_cast<float>(e->NewSize.Width);
    m_canvasH = static_cast<float>(e->NewSize.Height);

    if (!m_initialized && m_canvasW > 0 && m_canvasH > 0) {
        InitParticles();
        m_initialized = true;
    }
}

void ParticleBackground::ApplyGradient()
{
    Color stop0 = ScaleRGB(m_gradientColor, 0.15f); stop0.A = 255;
    Color stop1 = { 255, 5, 5, 10 };

    if (m_bgBrush == nullptr) {
        auto gs0 = ref new GradientStop(); gs0->Color = stop0; gs0->Offset = 0.0;
        auto gs1 = ref new GradientStop(); gs1->Color = stop1; gs1->Offset = 1.0;
        m_bgBrush = ref new LinearGradientBrush();
        m_bgBrush->StartPoint = { 0.0, 1.0 };
        m_bgBrush->EndPoint   = { 1.0, 0.0 };
        m_bgBrush->GradientStops->Append(gs0);
        m_bgBrush->GradientStops->Append(gs1);
        ParticleCanvas->Background = m_bgBrush;
    } else {
        m_bgBrush->GradientStops->GetAt(0)->Color = stop0;
        m_bgBrush->GradientStops->GetAt(1)->Color = stop1;
    }
}

void ParticleBackground::ReloadColors()
{
    if (!m_initialized) return;
    LoadPalette();
    ApplyGradient();

    SolidColorBrush^ bokehBrushes[2];
    bokehBrushes[0] = ref new SolidColorBrush(LerpRGB(m_colorA, m_colorB, 0.3f, 45));
    bokehBrushes[1] = ref new SolidColorBrush(LerpRGB(m_colorA, m_colorB, 0.7f, 40));

    Color white = { 255, 255, 255, 255 };
    Color coreCol = LerpRGB(m_colorA, white, 0.45f, 220);
    SolidColorBrush^ smallBrushes[5];
    for (int b = 0; b < 5; ++b) {
        float   t = b / 4.0f;
        uint8_t a = static_cast<uint8_t>(220 - t * 65);
        smallBrushes[b] = ref new SolidColorBrush(LerpRGB(coreCol, m_colorB, t, a));
    }

    int count = static_cast<int>(m_particles.size());
    for (int i = 0; i < count; ++i) {
        auto& s = m_particles[i];
        auto el = safe_cast<Ellipse^>(ParticleCanvas->Children->GetAt(i));
        el->Fill = s.isBokeh ? bokehBrushes[s.colorSlot] : smallBrushes[s.colorSlot];
    }
}

void ParticleBackground::InitParticles()
{
    LoadPalette();
    ApplyGradient();
    m_particles.clear();
    m_particles.reserve(kParticleCount);
    ParticleCanvas->Children->Clear();

    SolidColorBrush^ bokehBrushes[2];
    bokehBrushes[0] = ref new SolidColorBrush(LerpRGB(m_colorA, m_colorB, 0.3f, 45));
    bokehBrushes[1] = ref new SolidColorBrush(LerpRGB(m_colorA, m_colorB, 0.7f, 40));

    Color white = { 255, 255, 255, 255 };
    Color coreCol = LerpRGB(m_colorA, white, 0.45f, 220);
    SolidColorBrush^ smallBrushes[5];
    for (int b = 0; b < 5; ++b) {
        float   t = b / 4.0f;
        uint8_t a = static_cast<uint8_t>(220 - t * 65);
        smallBrushes[b] = ref new SolidColorBrush(LerpRGB(coreCol, m_colorB, t, a));
    }

    std::uniform_real_distribution<float> distT(0.0f, 1.0f);

    std::uniform_real_distribution<float> bSpread(-0.35f, 0.35f);
    std::uniform_real_distribution<float> bSpeed(0.0005f, 0.0018f);
    std::uniform_real_distribution<float> bSize(20.0f, 80.0f);
    std::uniform_real_distribution<float> bOp(0.03f, 0.15f);
    std::uniform_real_distribution<float> bDelta(0.001f, 0.003f);

    for (int i = 0; i < kBokehCount; ++i) {
        ParticleState s;
        s.isBokeh      = true;
        s.t            = distT(m_rng);
        s.tSpeed       = bSpeed(m_rng);
        s.spreadY      = bSpread(m_rng) * m_canvasH;
        s.size         = bSize(m_rng);
        s.opacityMin   = 0.16f;
        s.opacityMax   = 0.32f;
        s.opacity      = bOp(m_rng);
        s.opacityDelta = bDelta(m_rng) * (m_rng() % 2 == 0 ? 1.0f : -1.0f);
        s.colorSlot    = m_rng() % 2;
        m_particles.push_back(s);

        float x = s.t * m_canvasW;
        float y = WaveY(s.t) + s.spreadY;
        auto el = ref new Ellipse();
        el->Width = s.size; el->Height = s.size;
        el->Fill = bokehBrushes[s.colorSlot];
        el->Opacity = s.opacity;
        Canvas::SetLeft(el, x - s.size * 0.5f);
        Canvas::SetTop(el, y - s.size * 0.5f);
        ParticleCanvas->Children->Append(el);
    }

    std::uniform_real_distribution<float> sSpread(-kSpreadRange, kSpreadRange);
    std::uniform_real_distribution<float> sSpeed(0.0010f, 0.0030f);
    std::uniform_real_distribution<float> sSize(1.5f, 5.5f);
    std::uniform_real_distribution<float> sDelta(0.004f, 0.012f);

    for (int i = 0; i < kSmallCount; ++i) {
        float spread   = sSpread(m_rng) * m_canvasH;
        float normDist = fabsf(spread) / (kSpreadRange * m_canvasH);

        ParticleState s;
        s.isBokeh      = false;
        s.t            = distT(m_rng);
        s.tSpeed       = sSpeed(m_rng);
        s.spreadY      = spread;
        s.size         = sSize(m_rng);
        s.opacityMax   = fmaxf(0.28f, 0.95f - normDist * 0.65f);
        s.opacityMin   = s.opacityMax * 0.15f;
        s.opacity      = s.opacityMin + (s.opacityMax - s.opacityMin)
                         * static_cast<float>(m_rng() % 100) / 100.0f;
        s.opacityDelta = sDelta(m_rng) * (m_rng() % 2 == 0 ? 1.0f : -1.0f);
        int band = static_cast<int>(normDist * 5.0f);
        if (band >= 5) band = 4;
        s.colorSlot    = band;
        m_particles.push_back(s);

        float x = s.t * m_canvasW;
        float y = WaveY(s.t) + s.spreadY;
        auto el = ref new Ellipse();
        el->Width = s.size; el->Height = s.size;
        el->Fill = smallBrushes[band];
        el->Opacity = s.opacity;
        Canvas::SetLeft(el, x - s.size * 0.5f);
        Canvas::SetTop(el, y - s.size * 0.5f);
        ParticleCanvas->Children->Append(el);
    }
}

void ParticleBackground::OnTick(Object^ sender, Object^ args)
{
    if (!m_initialized) return;

    m_wavePhase += 0.008f;
    if (m_wavePhase > kPi * 2.0f) m_wavePhase -= kPi * 2.0f;

    std::uniform_real_distribution<float> bSpread(-0.35f, 0.35f);
    std::uniform_real_distribution<float> sSpread(-kSpreadRange, kSpreadRange);

    int count = static_cast<int>(m_particles.size());
    for (int i = 0; i < count; ++i) {
        auto& s = m_particles[i];

        s.t += s.tSpeed;
        if (s.t > 1.05f) {
            s.t = -0.05f;
            if (s.isBokeh) {
                s.spreadY = bSpread(m_rng) * m_canvasH;
            } else {
                float spread   = sSpread(m_rng) * m_canvasH;
                s.spreadY      = spread;
                float normDist = fabsf(spread) / (kSpreadRange * m_canvasH);
                s.opacityMax   = fmaxf(0.28f, 0.95f - normDist * 0.65f);
                s.opacityMin   = s.opacityMax * 0.15f;
            }
        }

        float x = s.t * m_canvasW;
        float y = WaveY(s.t) + s.spreadY;

        s.opacity += s.opacityDelta;
        if (s.opacity > s.opacityMax) { s.opacity = s.opacityMax; s.opacityDelta = -fabsf(s.opacityDelta); }
        if (s.opacity < s.opacityMin) { s.opacity = s.opacityMin; s.opacityDelta =  fabsf(s.opacityDelta); }

        auto el = safe_cast<Ellipse^>(ParticleCanvas->Children->GetAt(i));
        Canvas::SetLeft(el, x - s.size * 0.5f);
        Canvas::SetTop(el, y - s.size * 0.5f);
        el->Opacity = s.opacity;
    }
}

void ParticleBackground::StartAnimations()
{
    if (m_timer != nullptr) m_timer->Start();
}

void ParticleBackground::StopAnimations()
{
    if (m_timer != nullptr) {
        m_timer->Stop();
        m_timer->Tick -= m_tickToken;
    }
}
