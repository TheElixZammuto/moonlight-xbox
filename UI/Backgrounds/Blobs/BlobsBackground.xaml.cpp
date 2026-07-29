#include "pch.h"
#include "UI\Backgrounds\Blobs\BlobsBackground.xaml.h"
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

static bool ParseHex6(Platform::String^ s, Color& out)
{
    if (s == nullptr || s->Length() != 6) return false;
    const wchar_t* p = s->Data();
    wchar_t buf[7]; wcsncpy_s(buf, p, 6); buf[6] = L'\0';
    wchar_t* end = nullptr;
    unsigned long v = wcstoul(buf, &end, 16);
    if (end != buf + 6) return false;
    out.R = (uint8_t)((v >> 16) & 0xFF);
    out.G = (uint8_t)((v >>  8) & 0xFF);
    out.B = (uint8_t)( v        & 0xFF);
    return true;
}

void BlobsBackground::LoadPalette()
{
    using namespace Windows::Storage;
    Platform::String^ scheme = L"crimson";
    auto ls = ApplicationData::Current->LocalSettings->Values;
    if (ls->HasKey("blobs.scheme"))
        scheme = safe_cast<Platform::String^>(ls->Lookup("blobs.scheme"));

    Color schemes[5][4] = {
        {{170,210,10,55},{170,185,0,185},{170,100,0,200},{255,8,0,16}},
        {{170,0,180,200},{170,0,100,220},{170,0,210,150},{255,0,6,20}},
        {{170,0,200,120},{170,60,190,255},{170,140,40,230},{255,2,4,10}},
        {{170,255,70,0},{170,210,20,20},{170,255,140,20},{255,12,3,0}},
        {{170,170,40,230},{170,255,60,180},{170,40,90,255},{255,5,0,18}},
    };

    int idx = -1;
    if      (scheme->Equals(L"crimson")) idx = 0;
    else if (scheme->Equals(L"ocean"))   idx = 1;
    else if (scheme->Equals(L"aurora"))  idx = 2;
    else if (scheme->Equals(L"ember"))   idx = 3;
    else if (scheme->Equals(L"nebula"))  idx = 4;

    if (idx >= 0) {
        for (int i = 0; i < 4; ++i) m_palette[i] = schemes[idx][i];
        return;
    }

    static const wchar_t* kKeys[] = {
        L"blobs.custom.0", L"blobs.custom.1", L"blobs.custom.2", L"blobs.custom.3"
    };
    Color defaults[4] = {
        {170,210,10,55},{170,185,0,185},{170,100,0,200},{255,8,0,16}
    };
    for (int i = 0; i < 4; ++i) {
        m_palette[i] = defaults[i];
        auto key = ref new Platform::String(kKeys[i]);
        if (ls->HasKey(key)) {
            Color c = m_palette[i];
            if (ParseHex6(safe_cast<Platform::String^>(ls->Lookup(key)), c)) {
                c.A = (i < 3) ? 170 : 255;
                m_palette[i] = c;
            }
        }
    }
}

BlobsBackground::BlobsBackground()
{
    m_rng = std::mt19937(std::random_device{}());
    InitializeComponent();
    this->Loaded += ref new RoutedEventHandler(this, &BlobsBackground::OnLoaded);

    BlobCanvas->Background = ref new SolidColorBrush(ColorHelper::FromArgb(255, 8, 0, 16));

    TimeSpan interval;
    interval.Duration = 16 * 10000LL;
    m_timer = ref new DispatcherTimer();
    m_timer->Interval = interval;
    Platform::WeakReference weakSelf(this);
    m_tickToken = m_timer->Tick += ref new EventHandler<Object^>(
        [weakSelf](Object^, Object^) {
            try {
                auto self = weakSelf.Resolve<BlobsBackground>();
                if (self) self->OnTick(nullptr, nullptr);
            } catch (Platform::DisconnectedException^) {}
              catch (...) {}
        });
}

void BlobsBackground::Canvas_SizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    m_canvasW = static_cast<float>(e->NewSize.Width);
    m_canvasH = static_cast<float>(e->NewSize.Height);
    if (m_canvasW > 0 && m_canvasH > 0 && !m_ready) {
        m_ready = true;
        InitBlobs();
    }
}

void BlobsBackground::InitBlobs()
{
    LoadPalette();
    BlobCanvas->Background = ref new SolidColorBrush(m_palette[3]);
    m_blobs.clear();
    BlobCanvas->Children->Clear();

    std::uniform_real_distribution<float> distR(170.0f, 240.0f);
    std::uniform_real_distribution<float> distWobR(70.0f, 160.0f);
    std::uniform_real_distribution<float> distWobA(0.2f, 0.5f);
    std::uniform_real_distribution<float> distPhase(0.0f, kPi * 2.0f);
    std::uniform_real_distribution<float> distFreqR(0.03f, 0.08f);
    std::uniform_real_distribution<float> distFreqA(0.02f, 0.06f);
    std::uniform_real_distribution<float> distTime(0.0f, 200.0f);
    std::uniform_real_distribution<float> distSpeed(0.2f, 0.5f);

    float positions[kBlobCount][2] = {
        { m_canvasW * 0.0f,  m_canvasH * 0.7f },
        { m_canvasW * 0.5f,  m_canvasH * 0.2f },
        { m_canvasW * 0.9f,  m_canvasH * 0.7f },
    };

    for (int i = 0; i < kBlobCount; i++) {
        BlobData b{};
        b.cx        = positions[i][0];
        b.cy        = positions[i][1];
        b.baseRadius = distR(m_rng);
        b.time      = distTime(m_rng);
        b.timeSpeed  = distSpeed(m_rng);

        for (int j = 0; j < kBlobPts; j++) {
            b.rAmp[j]   = distWobR(m_rng);
            b.rPhase[j] = distPhase(m_rng);
            b.rFreq[j]  = distFreqR(m_rng);
            b.aAmp[j]   = distWobA(m_rng);
            b.aFreq[j]  = distFreqA(m_rng);
            b.aPhase[j] = distPhase(m_rng);
        }

        auto s0 = ref new BezierSegment();
        auto s1 = ref new BezierSegment();
        auto s2 = ref new BezierSegment();
        auto s3 = ref new BezierSegment();
        auto s4 = ref new BezierSegment();
        auto s5 = ref new BezierSegment();
        b.s0 = s0; b.s1 = s1; b.s2 = s2;
        b.s3 = s3; b.s4 = s4; b.s5 = s5;

        auto figure = ref new PathFigure();
        figure->IsClosed = true;
        figure->IsFilled = true;
        figure->Segments->Append(s0);
        figure->Segments->Append(s1);
        figure->Segments->Append(s2);
        figure->Segments->Append(s3);
        figure->Segments->Append(s4);
        figure->Segments->Append(s5);
        b.figure = figure;

        auto geom = ref new PathGeometry();
        geom->Figures->Append(figure);

        auto path = ref new Path();
        path->Data = geom;
        path->Fill = ref new SolidColorBrush(m_palette[i]);

        BlobCanvas->Children->Append(path);
        m_blobs.push_back(b);
        UpdateBlob(m_blobs.back());
    }
}

void BlobsBackground::UpdateBlob(BlobData& b)
{
    b.time += b.timeSpeed;

    float px[kBlobPts], py[kBlobPts];
    for (int i = 0; i < kBlobPts; i++) {
        float baseAngle = i * (2.0f * kPi / kBlobPts);
        float angle = baseAngle + b.aAmp[i] * sinf(b.time * b.aFreq[i] + b.aPhase[i]);
        float r     = b.baseRadius + b.rAmp[i] * sinf(b.time * b.rFreq[i] + b.rPhase[i]);
        px[i] = b.cx + r * cosf(angle);
        py[i] = b.cy + r * sinf(angle);
    }

    auto cp1x = [&](int i) { return px[i] + (px[(i+1)%kBlobPts] - px[(i-1+kBlobPts)%kBlobPts]) / 6.0f; };
    auto cp1y = [&](int i) { return py[i] + (py[(i+1)%kBlobPts] - py[(i-1+kBlobPts)%kBlobPts]) / 6.0f; };
    auto cp2x = [&](int i) { return px[(i+1)%kBlobPts] - (px[(i+2)%kBlobPts] - px[i]) / 6.0f; };
    auto cp2y = [&](int i) { return py[(i+1)%kBlobPts] - (py[(i+2)%kBlobPts] - py[i]) / 6.0f; };

    b.figure->StartPoint = Point(px[0], py[0]);

    b.s0->Point1 = Point(cp1x(0), cp1y(0)); b.s0->Point2 = Point(cp2x(0), cp2y(0)); b.s0->Point3 = Point(px[1], py[1]);
    b.s1->Point1 = Point(cp1x(1), cp1y(1)); b.s1->Point2 = Point(cp2x(1), cp2y(1)); b.s1->Point3 = Point(px[2], py[2]);
    b.s2->Point1 = Point(cp1x(2), cp1y(2)); b.s2->Point2 = Point(cp2x(2), cp2y(2)); b.s2->Point3 = Point(px[3], py[3]);
    b.s3->Point1 = Point(cp1x(3), cp1y(3)); b.s3->Point2 = Point(cp2x(3), cp2y(3)); b.s3->Point3 = Point(px[4], py[4]);
    b.s4->Point1 = Point(cp1x(4), cp1y(4)); b.s4->Point2 = Point(cp2x(4), cp2y(4)); b.s4->Point3 = Point(px[5], py[5]);
    b.s5->Point1 = Point(cp1x(5), cp1y(5)); b.s5->Point2 = Point(cp2x(5), cp2y(5)); b.s5->Point3 = Point(px[0], py[0]);
}

void BlobsBackground::OnTick(Object^ sender, Object^ args)
{
    if (!m_ready) return;
    for (auto& b : m_blobs) UpdateBlob(b);
}

void BlobsBackground::OnLoaded(Object^ sender, RoutedEventArgs^ e)
{
    auto anim = ref new DoubleAnimation();
    anim->From = 0.0;
    anim->To   = 1.0;
    TimeSpan ts;
    ts.Duration = 5000000LL;
    anim->Duration = Windows::UI::Xaml::Duration(ts);
    BlobCanvas->Opacity = 0.0;
    auto sb = ref new Storyboard();
    Storyboard::SetTarget(anim, BlobCanvas);
    Storyboard::SetTargetProperty(anim, "Opacity");
    sb->Children->Append(anim);
    sb->Begin();
}

void BlobsBackground::ReloadColors()
{
    if (!m_ready) return;
    LoadPalette();
    for (int i = 0; i < kBlobCount; ++i) {
        safe_cast<Path^>(BlobCanvas->Children->GetAt(i))->Fill =
            ref new SolidColorBrush(m_palette[i]);
    }
    BlobCanvas->Background = ref new SolidColorBrush(m_palette[3]);
}

void BlobsBackground::StartAnimations()
{
    if (m_timer != nullptr) m_timer->Start();
}

void BlobsBackground::StopAnimations()
{
    if (m_timer != nullptr) {
        m_timer->Stop();
        m_timer->Tick -= m_tickToken;
    }
}
