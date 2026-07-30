#include "pch.h"
#include "UI\Backgrounds\Spheres\SpheresBackground.xaml.h"
#include <cmath>

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Shapes;
using namespace Windows::UI::Xaml::Media;

static const int kSphereCount = 14;

static Color kSpheresClassicScheme[3] = { {255,255,255,255}, {255,  0, 68,170}, {255,  0, 34,119} };
static Color kSpheresNeonScheme[3]    = { {255,  0,238,255}, {255,  0, 32,128}, {255,  0,  8, 48} };
static Color kSpheresSunsetScheme[3]  = { {255,255,112, 64}, {255,139, 21,  0}, {255, 32,  0,  8} };
static Color kSpheresOceanScheme[3]   = { {255,  0,200,176}, {255,  0, 64, 96}, {255,  0, 16, 32} };
static Color kSpheresNebulaScheme[3]  = { {255,192, 96,255}, {255, 64,  0,128}, {255, 13,  0, 32} };

static void SetSphereCanvasBg(Canvas^ canvas, Windows::UI::Color top, Windows::UI::Color bottom)
{
    auto lgb = ref new LinearGradientBrush();
    lgb->StartPoint = Point(0.5, 0.0);
    lgb->EndPoint   = Point(0.5, 1.1);
    auto gs0 = ref new GradientStop();
    gs0->Color  = ColorHelper::FromArgb(255, top.R,    top.G,    top.B);
    gs0->Offset = 0.0;
    auto gs1 = ref new GradientStop();
    gs1->Color  = ColorHelper::FromArgb(255, bottom.R, bottom.G, bottom.B);
    gs1->Offset = 1.0;
    lgb->GradientStops->Append(gs0);
    lgb->GradientStops->Append(gs1);
    canvas->Background = lgb;
}

static bool ParseHex6Spheres(Platform::String^ s, Color& out)
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

SpheresBackground::SpheresBackground()
{
    m_rng = std::mt19937(std::random_device{}());
    InitializeComponent();

    TimeSpan interval;
    interval.Duration = 16 * 10000LL;
    m_timer = ref new DispatcherTimer();
    m_timer->Interval = interval;
    Platform::WeakReference weakSelf(this);
    m_tickToken = m_timer->Tick += ref new EventHandler<Object^>(
        [weakSelf](Object^, Object^) {
            try {
                auto self = weakSelf.Resolve<SpheresBackground>();
                if (self) self->OnTick(nullptr, nullptr);
            } catch (Platform::DisconnectedException^) {}
              catch (...) {}
        });
}

void SpheresBackground::LoadPalette()
{
    using namespace Windows::Storage;
    Platform::String^ scheme = L"classic";
    auto ls = ApplicationData::Current->LocalSettings->Values;
    if (ls->HasKey("spheres.scheme"))
        scheme = safe_cast<Platform::String^>(ls->Lookup("spheres.scheme"));

    Color* src = kSpheresClassicScheme;
    if      (scheme->Equals(L"neon"))   src = kSpheresNeonScheme;
    else if (scheme->Equals(L"sunset")) src = kSpheresSunsetScheme;
    else if (scheme->Equals(L"ocean"))  src = kSpheresOceanScheme;
    else if (scheme->Equals(L"nebula")) src = kSpheresNebulaScheme;

    if (!scheme->Equals(L"custom")) {
        m_palette[0] = src[0];
        m_palette[1] = src[1];
        m_palette[2] = src[2];
        return;
    }

    static const wchar_t* kCustomKeys[] = { L"spheres.custom.0", L"spheres.custom.1" };
    for (int i = 0; i < 2; ++i) {
        Color c = kSpheresClassicScheme[i];
        auto key = ref new Platform::String(kCustomKeys[i]);
        if (ls->HasKey(key))
            ParseHex6Spheres(safe_cast<Platform::String^>(ls->Lookup(key)), c);
        m_palette[i] = c;
    }
    Color top = m_palette[1];
    m_palette[2] = ColorHelper::FromArgb(255,
        static_cast<uint8_t>(top.R * 0.40f),
        static_cast<uint8_t>(top.G * 0.40f),
        static_cast<uint8_t>(top.B * 0.40f));
}

void SpheresBackground::LoadShapeMode()
{
    using namespace Windows::Storage;
    auto ls = ApplicationData::Current->LocalSettings->Values;
    if (!ls->HasKey("spheres.shape")) { m_shapeMode = 0; return; }
    auto val = safe_cast<Platform::String^>(ls->Lookup("spheres.shape"));
    if      (val->Equals(L"squares"))   m_shapeMode = 1;
    else if (val->Equals(L"triangles")) m_shapeMode = 2;
    else if (val->Equals(L"all"))       m_shapeMode = 3;
    else                                m_shapeMode = 0;
}

void SpheresBackground::Canvas_SizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    m_canvasW = static_cast<float>(e->NewSize.Width);
    m_canvasH = static_cast<float>(e->NewSize.Height);

    if (!m_initialized && m_canvasW > 0 && m_canvasH > 0) {
        InitSpheres();
        m_initialized = true;
    }
}

static UIElement^ MakeBubbleShape(int shapeType, float r,
                                   Windows::UI::Color shapeColor, float opacity)
{
    auto xf = ref new RotateTransform();
    xf->CenterX = static_cast<double>(r);
    xf->CenterY = static_cast<double>(r);

    if (shapeType == kShapeSquare) {
        auto rect = ref new Rectangle();
        rect->Width           = r * 2.0f;
        rect->Height          = r * 2.0f;
        rect->Stroke          = ref new SolidColorBrush(
            ColorHelper::FromArgb(255, shapeColor.R, shapeColor.G, shapeColor.B));
        rect->StrokeThickness = 1.5;
        rect->Opacity         = opacity;
        rect->RenderTransform = xf;
        return rect;
    }

    if (shapeType == kShapeTriangle) {

        auto tri = ref new Polygon();
        auto pts = ref new PointCollection();
        pts->Append(Point(r,                       0.0f          ));
        pts->Append(Point(r + r * 0.866f,          r + r * 0.5f ));
        pts->Append(Point(r - r * 0.866f,          r + r * 0.5f ));
        tri->Points          = pts;
        tri->Stroke          = ref new SolidColorBrush(
            ColorHelper::FromArgb(255, shapeColor.R, shapeColor.G, shapeColor.B));
        tri->StrokeThickness = 1.5;
        tri->Opacity         = opacity;
        tri->RenderTransform = xf;
        return tri;
    }

    auto el = ref new Ellipse();
    el->Width           = r * 2.0f;
    el->Height          = r * 2.0f;
    el->Stroke          = ref new SolidColorBrush(
        ColorHelper::FromArgb(255, shapeColor.R, shapeColor.G, shapeColor.B));
    el->StrokeThickness = 1.0;
    el->Opacity         = opacity;
    el->RenderTransform = xf;
    return el;
}

void SpheresBackground::InitSpheres()
{
    LoadPalette();
    LoadShapeMode();

    m_spheres.clear();
    m_spheres.reserve(kSphereCount);
    SphereCanvas->Children->Clear();
    SetSphereCanvasBg(SphereCanvas, m_palette[1], m_palette[2]);

    std::uniform_real_distribution<float> distAngle(0.0f, 6.28318f);
    std::uniform_real_distribution<float> distOpacity(0.05f, 0.25f);

    for (int i = 0; i < kSphereCount; ++i) {
        SphereState s;
        float minSpeed, maxSpeed, minSpin, maxSpin;

        if (i < 3) {
            s.radius = 110.0f + static_cast<float>(m_rng() % 70);
            minSpeed = 0.15f; maxSpeed = 0.45f;
            minSpin  = 0.2f;  maxSpin  = 0.6f;
        } else if (i < 8) {
            s.radius = 50.0f  + static_cast<float>(m_rng() % 50);
            minSpeed = 0.40f; maxSpeed = 1.0f;
            minSpin  = 0.5f;  maxSpin  = 1.2f;
        } else {
            s.radius = 18.0f  + static_cast<float>(m_rng() % 28);
            minSpeed = 0.75f; maxSpeed = 1.6f;
            minSpin  = 1.0f;  maxSpin  = 2.5f;
        }

        std::uniform_real_distribution<float> distX(s.radius, m_canvasW - s.radius);
        std::uniform_real_distribution<float> distY(s.radius, m_canvasH - s.radius);
        std::uniform_real_distribution<float> distSpeed(minSpeed, maxSpeed);
        std::uniform_real_distribution<float> distSpin(minSpin, maxSpin);

        s.x         = distX(m_rng);
        s.y         = distY(m_rng);
        s.opacity   = distOpacity(m_rng);
        s.spinAngle = distAngle(m_rng) * (180.0f / 3.14159265f);
        float spinMag = distSpin(m_rng);
        s.spinSpeed = (m_rng() % 2 == 0) ? spinMag : -spinMag;

        float moveAngle = distAngle(m_rng);
        float speed     = distSpeed(m_rng);
        s.vx = cosf(moveAngle) * speed;
        s.vy = sinf(moveAngle) * speed;

        s.shapeType = (m_shapeMode == 3) ? (i % 3) : m_shapeMode;

        m_spheres.push_back(s);

        auto el = MakeBubbleShape(s.shapeType, s.radius, m_palette[0], s.opacity);
        auto rt = dynamic_cast<RotateTransform^>(el->RenderTransform);
        if (rt != nullptr) rt->Angle = static_cast<double>(s.spinAngle);
        Canvas::SetLeft(el, s.x - s.radius);
        Canvas::SetTop(el,  s.y - s.radius);
        SphereCanvas->Children->Append(el);
    }
}

void SpheresBackground::OnTick(Object^ sender, Object^ args)
{
    if (!m_initialized) return;

    int count = static_cast<int>(m_spheres.size());
    for (int i = 0; i < count; ++i) {
        auto& s = m_spheres[i];

        s.x += s.vx;
        s.y += s.vy;

        if      (s.x - s.radius < 0.0f)          { s.x = s.radius;             s.vx =  fabsf(s.vx); }
        else if (s.x + s.radius > m_canvasW)      { s.x = m_canvasW - s.radius; s.vx = -fabsf(s.vx); }
        if      (s.y - s.radius < 0.0f)          { s.y = s.radius;             s.vy =  fabsf(s.vy); }
        else if (s.y + s.radius > m_canvasH)      { s.y = m_canvasH - s.radius; s.vy = -fabsf(s.vy); }

        s.spinAngle += s.spinSpeed;

        auto el = SphereCanvas->Children->GetAt(i);
        Canvas::SetLeft(el, s.x - s.radius);
        Canvas::SetTop(el,  s.y - s.radius);
        auto rt = dynamic_cast<RotateTransform^>(el->RenderTransform);
        if (rt != nullptr) rt->Angle = static_cast<double>(s.spinAngle);
    }
}

void SpheresBackground::ReloadColors()
{
    if (!m_initialized) return;

    int prevShapeMode = m_shapeMode;
    LoadShapeMode();

    if (m_shapeMode != prevShapeMode) {
        InitSpheres();
        return;
    }

    LoadPalette();

    SetSphereCanvasBg(SphereCanvas, m_palette[1], m_palette[2]);

    Color shapeCol = m_palette[0];
    auto brush = ref new SolidColorBrush(
        ColorHelper::FromArgb(255, shapeCol.R, shapeCol.G, shapeCol.B));

    int count = static_cast<int>(m_spheres.size());
    for (int i = 0; i < count; ++i) {
        auto el = SphereCanvas->Children->GetAt(i);
        if (auto shape = dynamic_cast<Shape^>(el)) {
            shape->Stroke = brush;
        }
    }
}

void SpheresBackground::StartAnimations()
{
    if (m_timer != nullptr) m_timer->Start();
}

void SpheresBackground::StopAnimations()
{
    if (m_timer != nullptr) {
        m_timer->Stop();
        m_timer->Tick -= m_tickToken;
    }
}
