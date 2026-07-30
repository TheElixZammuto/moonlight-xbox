#include "pch.h"
#include "UI\Backgrounds\Orbs\OrbsBackground.xaml.h"
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

static const float kPi = 3.14159265358979f;

static Color kOrbsDefaultGlow = { 255, 30, 160, 255 };
static Color kOrbsDefaultBg   = { 255,  0,   8,  20 };

static bool ParseHex6Orbs(Platform::String^ s, Color& out)
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

static const float kOuterAuraR    = 40.0f;
static const float kInnerGlowR    = 30.0f;
static const float kCoreR         = 20.0f;
static const float kMinTailR      = 1.5f;
static const float kMaxTailR      = 9.0f;
static const float kMaxTailOpacity = 0.88f;

static const float kCircleTilt    = 20.0f * kPi / 180.0f;
static const float kCircleRotSpeed = 0.007f;
static const int   kDanceHoldTicks  = 420;
static const int   kCircleHoldTicks = 300;
static const int   kTransitionTicks = 90;

static const int kOuterAuraBase = kOrbCount * kOrbTailLen;
static const int kInnerGlowBase = kOrbCount * kOrbTailLen + kOrbCount;
static const int kCoreBase      = kOrbCount * kOrbTailLen + kOrbCount * 2;

static void OrbPos(const OrbState& o, float& outX, float& outY)
{
    float ca = cosf(o.angle), sa = sinf(o.angle);
    float ct = cosf(o.tilt),  st = sinf(o.tilt);
    outX = o.cx + o.rx * ca * ct - o.ry * sa * st;
    outY = o.cy + o.rx * ca * st + o.ry * sa * ct;
}

OrbsBackground::OrbsBackground()
{
    InitializeComponent();
    this->Loaded += ref new RoutedEventHandler(this, &OrbsBackground::OnLoaded);
    m_palette[0] = kOrbsDefaultGlow;
    m_palette[1] = kOrbsDefaultBg;
    OrbCanvas->Background = ref new SolidColorBrush(ColorHelper::FromArgb(255, 0, 8, 20));

    TimeSpan interval;
    interval.Duration = 16 * 10000LL;
    m_timer = ref new DispatcherTimer();
    m_timer->Interval = interval;
    Platform::WeakReference weakSelf(this);
    m_tickToken = m_timer->Tick += ref new EventHandler<Object^>(
        [weakSelf](Object^, Object^) {
            try {
                auto self = weakSelf.Resolve<OrbsBackground>();
                if (self) self->OnTick(nullptr, nullptr);
            } catch (Platform::DisconnectedException^) {}
              catch (...) {}
        });
}

void OrbsBackground::Canvas_SizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    m_canvasW = static_cast<float>(e->NewSize.Width);
    m_canvasH = static_cast<float>(e->NewSize.Height);
    if (!m_initialized && m_canvasW > 0 && m_canvasH > 0) {
        InitOrbs();
        m_initialized = true;
    }
}

void OrbsBackground::LoadPalette()
{
    using namespace Windows::Storage;
    Platform::String^ scheme = L"electric";
    auto ls = ApplicationData::Current->LocalSettings->Values;
    if (ls->HasKey("orbs.scheme"))
        scheme = safe_cast<Platform::String^>(ls->Lookup("orbs.scheme"));

    Color schemes[5][2] = {
        { {255, 30,160,255}, {255,  0,  8, 20} },
        { {255,  0,220,150}, {255,  0, 10, 15} },
        { {255,255,140,  0}, {255, 12,  5,  0} },
        { {255,200, 80,255}, {255,  8,  0, 20} },
        { {255,255, 60,140}, {255, 15,  0, 12} },
    };

    int idx = -1;
    if      (scheme->Equals(L"electric")) idx = 0;
    else if (scheme->Equals(L"aurora"))   idx = 1;
    else if (scheme->Equals(L"solar"))    idx = 2;
    else if (scheme->Equals(L"nebula"))   idx = 3;
    else if (scheme->Equals(L"rose"))     idx = 4;

    if (idx >= 0) {
        m_palette[0] = schemes[idx][0];
        m_palette[1] = schemes[idx][1];
        return;
    }

    static const wchar_t* kCustomKeys[] = { L"orbs.custom.0", L"orbs.custom.1" };
    Color defaults[2] = { kOrbsDefaultGlow, kOrbsDefaultBg };
    for (int i = 0; i < 2; ++i) {
        Color c = defaults[i];
        auto key = ref new Platform::String(kCustomKeys[i]);
        if (ls->HasKey(key))
            ParseHex6Orbs(safe_cast<Platform::String^>(ls->Lookup(key)), c);
        m_palette[i] = c;
    }
}

void OrbsBackground::InitOrbs()
{
    LoadPalette();
    m_orbs.clear();
    OrbCanvas->Children->Clear();
    OrbCanvas->Background = ref new SolidColorBrush(
        ColorHelper::FromArgb(m_palette[1].A, m_palette[1].R, m_palette[1].G, m_palette[1].B));

    m_lerpT       = 0.0f;
    m_targetLerpT = 0.0f;
    m_holdTicks   = kDanceHoldTicks;
    m_circleAngle = 0.0f;

    float W = m_canvasW, H = m_canvasH;
    float cx = W * 0.5f, cy = H * 0.5f;

    struct OrbDef { float rxF, ryF, tilt, phase, speed; };
    static OrbDef kDefs[kOrbCount] = {

        { 0.13f, 0.09f, 30.0f * kPi/180.0f,  0.0f,             0.036f },
        { 0.13f, 0.09f, 30.0f * kPi/180.0f,  kPi,             -0.034f },
        { 0.13f, 0.09f, 30.0f * kPi/180.0f,  kPi * 0.5f,       0.040f },

        { 0.22f, 0.14f, 65.0f * kPi/180.0f,  0.0f,             0.024f },
        { 0.22f, 0.14f, 65.0f * kPi/180.0f,  kPi * 2.0f/3.0f, -0.022f },
        { 0.22f, 0.14f, 65.0f * kPi/180.0f,  kPi * 4.0f/3.0f,  0.026f },

        { 0.32f, 0.20f, 15.0f * kPi/180.0f,  kPi * 0.25f,      0.016f },
        { 0.32f, 0.20f, 15.0f * kPi/180.0f,  kPi * 1.25f,      0.018f },
    };

    m_orbs.reserve(kOrbCount);
    for (int i = 0; i < kOrbCount; ++i) {
        OrbState s;
        s.cx    = cx;
        s.cy    = cy;
        s.rx    = kDefs[i].rxF * W;
        s.ry    = kDefs[i].ryF * H;
        s.tilt  = kDefs[i].tilt;
        s.angle = kDefs[i].phase;
        s.speed = kDefs[i].speed;

        float startX, startY;
        OrbPos(s, startX, startY);
        for (int j = 0; j < kOrbTailLen; ++j) {
            s.tailX[j] = startX;
            s.tailY[j] = startY;
        }
        m_orbs.push_back(s);
    }

    for (int i = 0; i < kOrbCount; ++i) {
        const auto& s = m_orbs[i];
        for (int j = 0; j < kOrbTailLen; ++j) {
            float t  = static_cast<float>(j) / (kOrbTailLen - 1);
            float r  = kMinTailR + t * (kMaxTailR - kMinTailR);
            float op = t * kMaxTailOpacity;

            auto el = ref new Ellipse();
            el->Width   = r * 2.0f;
            el->Height  = r * 2.0f;
            Color gc = m_palette[0];
            el->Fill    = ref new SolidColorBrush(ColorHelper::FromArgb(255, gc.R, gc.G, gc.B));
            el->Opacity = op;
            Canvas::SetLeft(el, s.tailX[j] - r);
            Canvas::SetTop(el,  s.tailY[j] - r);
            OrbCanvas->Children->Append(el);
        }
    }

    for (int i = 0; i < kOrbCount; ++i) {
        const auto& s = m_orbs[i];
        Color col = m_palette[0];
        float x, y; OrbPos(s, x, y);

        auto el = ref new Ellipse();
        el->Width   = kOuterAuraR * 2.0f;
        el->Height  = kOuterAuraR * 2.0f;
        el->Fill    = ref new SolidColorBrush(ColorHelper::FromArgb(80, col.R, col.G, col.B));
        Canvas::SetLeft(el, x - kOuterAuraR);
        Canvas::SetTop(el,  y - kOuterAuraR);
        OrbCanvas->Children->Append(el);
    }

    for (int i = 0; i < kOrbCount; ++i) {
        const auto& s = m_orbs[i];
        Color col = m_palette[0];
        float x, y; OrbPos(s, x, y);

        auto el = ref new Ellipse();
        el->Width   = kInnerGlowR * 2.0f;
        el->Height  = kInnerGlowR * 2.0f;
        el->Fill    = ref new SolidColorBrush(ColorHelper::FromArgb(215, col.R, col.G, col.B));
        Canvas::SetLeft(el, x - kInnerGlowR);
        Canvas::SetTop(el,  y - kInnerGlowR);
        OrbCanvas->Children->Append(el);
    }

    for (int i = 0; i < kOrbCount; ++i) {
        const auto& s = m_orbs[i];
        float x, y; OrbPos(s, x, y);

        auto el = ref new Ellipse();
        el->Width   = kCoreR * 2.0f;
        el->Height  = kCoreR * 2.0f;
        el->Fill    = ref new SolidColorBrush(ColorHelper::FromArgb(255, 255, 255, 255));
        Canvas::SetLeft(el, x - kCoreR);
        Canvas::SetTop(el,  y - kCoreR);
        OrbCanvas->Children->Append(el);
    }
}

void OrbsBackground::OnTick(Object^ sender, Object^ args)
{
    if (!m_initialized) return;

    float W = m_canvasW, H = m_canvasH;
    float cx = W * 0.5f, cy = H * 0.5f;
    float circleR = (W < H ? W : H) * 0.26f;

    if (--m_holdTicks <= 0) {
        if (m_targetLerpT < 0.5f) {
            m_targetLerpT = 1.0f;
            m_holdTicks = kCircleHoldTicks + kTransitionTicks;
        } else {
            m_targetLerpT = 0.0f;
            m_holdTicks = kDanceHoldTicks + kTransitionTicks;
        }
    }

    float step = 1.0f / static_cast<float>(kTransitionTicks);
    float remaining = m_targetLerpT - m_lerpT;
    if ((remaining < 0.0f ? -remaining : remaining) < step) {
        m_lerpT = m_targetLerpT;
    } else {
        m_lerpT += remaining > 0.0f ? step : -step;
    }

    float st = m_lerpT * m_lerpT * (3.0f - 2.0f * m_lerpT);

    m_circleAngle += kCircleRotSpeed;
    float ctilt = cosf(kCircleTilt), stilt = sinf(kCircleTilt);

    for (int i = 0; i < kOrbCount; ++i) {
        auto& s = m_orbs[i];

        for (int j = 0; j < kOrbTailLen - 1; ++j) {
            s.tailX[j] = s.tailX[j + 1];
            s.tailY[j] = s.tailY[j + 1];
        }

        if (m_lerpT == 0.0f) s.angle += s.speed;
        float dx, dy;
        OrbPos(s, dx, dy);

        float cAngle = m_circleAngle + i * (2.0f * kPi / kOrbCount);
        float cca = cosf(cAngle), csa = sinf(cAngle);
        float px = cx + circleR * cca * ctilt - circleR * csa * stilt;
        float py = cy + circleR * cca * stilt + circleR * csa * ctilt;

        float nx = dx + st * (px - dx);
        float ny = dy + st * (py - dy);

        s.tailX[kOrbTailLen - 1] = nx;
        s.tailY[kOrbTailLen - 1] = ny;

        for (int j = 0; j < kOrbTailLen; ++j) {
            float t = static_cast<float>(j) / (kOrbTailLen - 1);
            float r = kMinTailR + t * (kMaxTailR - kMinTailR);
            auto el = safe_cast<Ellipse^>(OrbCanvas->Children->GetAt(i * kOrbTailLen + j));
            Canvas::SetLeft(el, s.tailX[j] - r);
            Canvas::SetTop(el,  s.tailY[j] - r);
        }

        auto aura = safe_cast<Ellipse^>(OrbCanvas->Children->GetAt(kOuterAuraBase + i));
        Canvas::SetLeft(aura, nx - kOuterAuraR);
        Canvas::SetTop(aura,  ny - kOuterAuraR);

        auto glow = safe_cast<Ellipse^>(OrbCanvas->Children->GetAt(kInnerGlowBase + i));
        Canvas::SetLeft(glow, nx - kInnerGlowR);
        Canvas::SetTop(glow,  ny - kInnerGlowR);

        auto core = safe_cast<Ellipse^>(OrbCanvas->Children->GetAt(kCoreBase + i));
        Canvas::SetLeft(core, nx - kCoreR);
        Canvas::SetTop(core,  ny - kCoreR);
    }
}

void OrbsBackground::OnLoaded(Object^ sender, RoutedEventArgs^ e)
{
    auto anim = ref new DoubleAnimation();
    anim->From = 0.0;
    anim->To   = 1.0;
    TimeSpan ts;
    ts.Duration = 5000000LL;
    anim->Duration = Windows::UI::Xaml::Duration(ts);
    OrbCanvas->Opacity = 0.0;
    auto sb = ref new Storyboard();
    Storyboard::SetTarget(anim, OrbCanvas);
    Storyboard::SetTargetProperty(anim, "Opacity");
    sb->Children->Append(anim);
    sb->Begin();
}

void OrbsBackground::ReloadColors()
{
    if (!m_initialized) return;
    LoadPalette();

    Color gc = m_palette[0];
    Color bc = m_palette[1];

    OrbCanvas->Background = ref new SolidColorBrush(
        ColorHelper::FromArgb(bc.A, bc.R, bc.G, bc.B));

    for (int i = 0; i < kOrbCount; ++i) {
        for (int j = 0; j < kOrbTailLen; ++j) {
            auto el = safe_cast<Ellipse^>(OrbCanvas->Children->GetAt(i * kOrbTailLen + j));
            el->Fill = ref new SolidColorBrush(ColorHelper::FromArgb(255, gc.R, gc.G, gc.B));
        }
    }
    for (int i = 0; i < kOrbCount; ++i) {
        auto el = safe_cast<Ellipse^>(OrbCanvas->Children->GetAt(kOuterAuraBase + i));
        el->Fill = ref new SolidColorBrush(ColorHelper::FromArgb(80, gc.R, gc.G, gc.B));
    }
    for (int i = 0; i < kOrbCount; ++i) {
        auto el = safe_cast<Ellipse^>(OrbCanvas->Children->GetAt(kInnerGlowBase + i));
        el->Fill = ref new SolidColorBrush(ColorHelper::FromArgb(215, gc.R, gc.G, gc.B));
    }

}

void OrbsBackground::StartAnimations() { if (m_timer) m_timer->Start(); }

void OrbsBackground::StopAnimations()
{
    if (m_timer != nullptr) {
        m_timer->Stop();
        m_timer->Tick -= m_tickToken;
    }
}
