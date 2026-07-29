#include "pch.h"
#include "UI\Backgrounds\Streaks\StreaksBackground.xaml.h"
#include <cmath>

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Shapes;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;

static const float kSqrt2      = 1.41421356f;
static const float kGlowExtend = 5.0f;
static const int   kStreakCount = 24;
static const int   kGlowBase   = 0;
static const int   kCoreBase   = kStreakCount;

static const Color kStreakPalette[] = {
    { 255, 255,   0,   0 },
    { 255,   0,  60, 255 },
    { 255, 255,   0, 220 },
    { 255, 140,   0, 255 },
    { 255,   0, 220, 255 },
};
static const int kStreakColors = 5;

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

StreaksBackground::StreaksBackground()
{
    m_rng = std::mt19937(std::random_device{}());
    InitializeComponent();
    this->Loaded += ref new RoutedEventHandler(this, &StreaksBackground::OnLoaded);
    StreakCanvas->Background = ref new SolidColorBrush(ColorHelper::FromArgb(255, 3, 3, 15));

    TimeSpan interval;
    interval.Duration = 16 * 10000LL;
    m_timer = ref new DispatcherTimer();
    m_timer->Interval = interval;
    Platform::WeakReference weakSelf(this);
    m_tickToken = m_timer->Tick += ref new EventHandler<Object^>(
        [weakSelf](Object^, Object^) {
            try {
                auto self = weakSelf.Resolve<StreaksBackground>();
                if (self) self->OnTick(nullptr, nullptr);
            } catch (Platform::DisconnectedException^) {}
              catch (...) {}
        });
}

void StreaksBackground::Canvas_SizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    m_canvasW = static_cast<float>(e->NewSize.Width);
    m_canvasH = static_cast<float>(e->NewSize.Height);

    if (!m_initialized && m_canvasW > 0 && m_canvasH > 0) {
        InitStreaks();
        m_initialized = true;
    }
}

static float ExitT(const StreakState& s, float W, float H)
{
    float txW = 2.0f * W + 2.0f * s.halfLen - s.lane;
    float txH = 2.0f * H + 2.0f * s.halfLen + s.lane;
    return txW > txH ? txW : txH;
}

static float EntryT(const StreakState& s)
{
    return fabsf(s.lane) - 2.0f * s.halfLen;
}

static Rectangle^ MakeStreakRect(float rectW, float rectH, Color col, float opacity)
{
    auto rect = ref new Rectangle();
    rect->Width   = rectW;
    rect->Height  = rectH;
    rect->RadiusX = rectH * 0.5;
    rect->RadiusY = rectH * 0.5;
    rect->Fill    = ref new SolidColorBrush(ColorHelper::FromArgb(255, col.R, col.G, col.B));
    rect->Opacity = opacity;

    auto xf = ref new RotateTransform();
    xf->CenterX = rectW * 0.5;
    xf->CenterY = rectH * 0.5;
    xf->Angle   = 45.0;
    rect->RenderTransform = xf;

    return rect;
}

void StreaksBackground::LoadPalette()
{
    using namespace Windows::Storage;
    Platform::String^ scheme = L"neon";
    auto ls = ApplicationData::Current->LocalSettings->Values;
    if (ls->HasKey("streaks.scheme"))
        scheme = safe_cast<Platform::String^>(ls->Lookup("streaks.scheme"));

    Color schemes[5][5] = {
        {{255,255,0,0},{255,0,60,255},{255,255,0,220},{255,140,0,255},{255,0,220,255}},
        {{255,0,80,255},{255,0,200,200},{255,0,229,255},{255,0,184,122},{255,26,58,255}},
        {{255,255,106,0},{255,255,45,120},{255,255,26,26},{255,255,194,0},{255,160,32,240}},
        {{255,255,224,0},{255,255,140,0},{255,255,173,0},{255,255,215,0},{255,255,69,0}},
        {{255,224,224,224},{255,255,255,255},{255,191,191,191},{255,160,160,160},{255,207,207,207}},
    };

    int schemeIdx = -1;
    if      (scheme->Equals(L"neon"))   schemeIdx = 0;
    else if (scheme->Equals(L"ocean"))  schemeIdx = 1;
    else if (scheme->Equals(L"sunset")) schemeIdx = 2;
    else if (scheme->Equals(L"warm"))   schemeIdx = 3;
    else if (scheme->Equals(L"mono"))   schemeIdx = 4;

    if (schemeIdx >= 0) {
        for (int i = 0; i < kStreakColors; ++i) m_palette[i] = schemes[schemeIdx][i];
        return;
    }

    static const wchar_t* kCustomKeys[] = {
        L"streaks.custom.0", L"streaks.custom.1", L"streaks.custom.2",
        L"streaks.custom.3", L"streaks.custom.4"
    };
    for (int i = 0; i < kStreakColors; ++i) {
        Color c = kStreakPalette[i];
        auto key = ref new Platform::String(kCustomKeys[i]);
        if (ls->HasKey(key)) {
            ParseHex6(safe_cast<Platform::String^>(ls->Lookup(key)), c);
        }
        m_palette[i] = c;
    }
}

void StreaksBackground::InitStreaks()
{
    LoadPalette();
    m_streaks.clear();
    m_streaks.reserve(kStreakCount);
    StreakCanvas->Children->Clear();

    float W = m_canvasW, H = m_canvasH;

    std::uniform_real_distribution<float> distLane(-H, W);
    std::uniform_real_distribution<float> distSpeed(3.0f, 9.5f);

    for (int i = 0; i < kStreakCount; ++i) {
        StreakState s;
        s.colorIndex = m_rng() % kStreakColors;
        s.lane       = distLane(m_rng);
        s.halfLen    = 120.0f + static_cast<float>(m_rng() % 380);
        s.speed      = distSpeed(m_rng);
        s.glowH      = 10.0f + static_cast<float>(m_rng() % 22);
        s.coreH      = 2.0f  + static_cast<float>(m_rng() % 3);

        float tMin = EntryT(s);
        float tMax = ExitT(s, W, H);
        std::uniform_real_distribution<float> distT(tMin, tMax);
        s.t = distT(m_rng);

        m_streaks.push_back(s);
    }

    for (int i = 0; i < kStreakCount; ++i) {
        const auto& s = m_streaks[i];
        Color col    = m_palette[s.colorIndex];
        float glowRectW = 2.0f * (s.halfLen + kGlowExtend) * kSqrt2;
        float opacity = 0.55f + static_cast<float>(m_rng() % 45) / 100.0f;

        auto glow = MakeStreakRect(glowRectW, s.glowH, col, opacity);
        float cx = (s.t + s.lane) * 0.5f;
        float cy = (s.t - s.lane) * 0.5f;
        Canvas::SetLeft(glow, cx - glowRectW * 0.5f);
        Canvas::SetTop(glow,  cy - s.glowH   * 0.5f);
        StreakCanvas->Children->Append(glow);
    }

    for (int i = 0; i < kStreakCount; ++i) {
        const auto& s = m_streaks[i];
        Color col   = kStreakPalette[s.colorIndex];
        float rectW = 2.0f * s.halfLen * kSqrt2;
        float opacity = 0.90f + static_cast<float>(m_rng() % 10) / 100.0f;

        auto core = MakeStreakRect(rectW, s.coreH, col, opacity);
        float cx = (s.t + s.lane) * 0.5f;
        float cy = (s.t - s.lane) * 0.5f;
        Canvas::SetLeft(core, cx - rectW     * 0.5f);
        Canvas::SetTop(core,  cy - s.coreH   * 0.5f);
        StreakCanvas->Children->Append(core);
    }

}

void StreaksBackground::OnTick(Object^ sender, Object^ args)
{
    if (!m_initialized) return;

    float W = m_canvasW, H = m_canvasH;
    int count = static_cast<int>(m_streaks.size());

    for (int i = 0; i < count; ++i) {
        auto& s = m_streaks[i];
        s.t += s.speed;

        if (s.t > ExitT(s, W, H)) {
            s.t = EntryT(s) - 50.0f - static_cast<float>(m_rng() % 200);
        }

        float cx       = (s.t + s.lane) * 0.5f;
        float cy       = (s.t - s.lane) * 0.5f;
        float coreRectW = 2.0f * s.halfLen                  * kSqrt2;
        float glowRectW = 2.0f * (s.halfLen + kGlowExtend)  * kSqrt2;

        auto glow = safe_cast<Rectangle^>(StreakCanvas->Children->GetAt(kGlowBase + i));
        Canvas::SetLeft(glow, cx - glowRectW * 0.5f);
        Canvas::SetTop(glow,  cy - s.glowH   * 0.5f);

        auto core = safe_cast<Rectangle^>(StreakCanvas->Children->GetAt(kCoreBase + i));
        Canvas::SetLeft(core, cx - coreRectW * 0.5f);
        Canvas::SetTop(core,  cy - s.coreH   * 0.5f);
    }
}

void StreaksBackground::OnLoaded(Object^ sender, RoutedEventArgs^ e)
{
    StreakCanvas->Opacity = 0.0;
    auto anim = ref new DoubleAnimation();
    anim->From = 0.0;
    anim->To   = 1.0;
    TimeSpan ts;
    ts.Duration = 5000000LL;
    anim->Duration = Windows::UI::Xaml::Duration(ts);
    auto sb = ref new Storyboard();
    Storyboard::SetTarget(anim, StreakCanvas);
    Storyboard::SetTargetProperty(anim, "Opacity");
    sb->Children->Append(anim);
    sb->Begin();
}

void StreaksBackground::ReloadColors()
{
    if (!m_initialized) return;
    LoadPalette();
    for (int i = 0; i < kStreakCount; ++i) {
        Color col = m_palette[m_streaks[i].colorIndex];
        auto brush = ref new SolidColorBrush(ColorHelper::FromArgb(255, col.R, col.G, col.B));
        safe_cast<Rectangle^>(StreakCanvas->Children->GetAt(kGlowBase + i))->Fill = brush;
        safe_cast<Rectangle^>(StreakCanvas->Children->GetAt(kCoreBase + i))->Fill = brush;
    }
}

void StreaksBackground::StartAnimations()
{
    if (m_timer != nullptr) m_timer->Start();
}

void StreaksBackground::StopAnimations()
{
    if (m_timer != nullptr) {
        m_timer->Stop();
        m_timer->Tick -= m_tickToken;
    }
}
