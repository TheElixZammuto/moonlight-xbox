#include "pch.h"
#include "UI\Backgrounds\SwipeReveal\SwipeRevealBackground.xaml.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <ppltasks.h>

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Shapes;
using namespace concurrency;

static const int   kHoldTicks        = 240;
static const int   kWipeTicks        = 100;
static const float kGlassWidth       = 64.0f;
static const float kPanMax           = 0.04f;
static const float kSlantH           = 180.0f;

static const float kWipeMargin       = kGlassWidth * 0.5f + kSlantH * 0.5f;
static const int   kLoadRetry        = 500;
static const int   kIntroBufferTicks = 30;

static float EaseInOut(float t)
{

    return t < 0.5f ? 4.0f * t * t * t
                    : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}
static const float kPi         = 3.14159265f;

SwipeRevealBackground::SwipeRevealBackground()
{
    InitializeComponent();

    m_frontPan = ref new CompositeTransform();
    m_frontPan->ScaleX   = 1.1;
    m_frontPan->ScaleY   = 1.1;
    m_frontPan->CenterX  = 0.5;
    m_frontPan->CenterY  = 0.5;

    m_frontBrush = ref new ImageBrush();
    m_frontBrush->Stretch             = Stretch::UniformToFill;
    m_frontBrush->RelativeTransform   = m_frontPan;

    m_frontClipRect = ref new RectangleGeometry();
    FrontGrid->Clip = m_frontClipRect;

    m_frontGridSkew = ref new SkewTransform();
    FrontGrid->RenderTransform = m_frontGridSkew;

    m_frontRectInvSkew = ref new SkewTransform();

    auto frontRect = ref new Rectangle();
    frontRect->Fill            = m_frontBrush;
    frontRect->RenderTransform = m_frontRectInvSkew;
    FrontGrid->Children->Append(frontRect);

    TimeSpan ts;
    ts.Duration = 16 * 10000LL;
    m_timer = ref new DispatcherTimer();
    m_timer->Interval = ts;
    Platform::WeakReference weakSelf(this);
    m_tickToken = m_timer->Tick += ref new EventHandler<Object^>(
        [weakSelf](Object^, Object^) {
            try {
                auto self = weakSelf.Resolve<SwipeRevealBackground>();
                if (self) self->OnTick(nullptr, nullptr);
            } catch (Platform::DisconnectedException^) {}
              catch (...) {}
        });
}

void SwipeRevealBackground::SetHosts(IVector<MoonlightHost^>^ hosts)
{
    m_hosts      = hosts;
    m_appsLoaded = false;
    m_apps       = nullptr;
    m_frontAppIdx = -1;
    m_backAppIdx  = -1;
    m_introTick   = 0;
}

void SwipeRevealBackground::Grid_SizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    m_canvasW     = static_cast<float>(e->NewSize.Width);
    m_canvasH     = static_cast<float>(e->NewSize.Height);
    m_initialized = (m_canvasW > 0 && m_canvasH > 0);
    if (m_initialized) {
        GlassEdgeSkew->CenterY    = m_canvasH * 0.5f;
        m_frontGridSkew->CenterY  = m_canvasH * 0.5f;
        m_frontRectInvSkew->CenterY = m_canvasH * 0.5f;

        auto r = m_frontClipRect->Rect;
        m_frontClipRect->Rect = Rect(r.X, 0.0f, r.Width, m_canvasH);
        UpdateGlassEdgeSkew();
    }
}

void SwipeRevealBackground::ZeroFrontClips()
{
    m_frontClipRect->Rect = Rect(0.0f, 0.0f, 0.0f, m_canvasH > 0.0f ? m_canvasH : 1.0f);
}

void SwipeRevealBackground::UpdateGlassEdgeSkew()
{
    if (m_canvasH <= 0) return;

    float angle = atanf(kSlantH / m_canvasH) * 180.0f / kPi;
    float gridAngle = (m_wipeDir > 0) ? -angle : angle;
    GlassEdgeSkew->AngleX    =  gridAngle;
    m_frontGridSkew->AngleX  =  gridAngle;
    m_frontRectInvSkew->AngleX = -gridAngle;
}

void SwipeRevealBackground::ShuffleAndApply(Platform::Collections::Vector<MoonlightApp^>^ collected)
{
    std::vector<MoonlightApp^> vec;
    vec.reserve(collected->Size);
    for (auto a : collected) vec.push_back(a);
    std::mt19937 rng(std::random_device{}());
    std::shuffle(vec.begin(), vec.end(), rng);
    auto shuffled = ref new Platform::Collections::Vector<MoonlightApp^>();
    for (auto a : vec) shuffled->Append(a);
    m_apps       = shuffled;
    m_appsLoaded = true;
    if (m_introTick >= kIntroBufferTicks)
        InitSlidesWithWipe();
    else
        InitSlides();
}

void SwipeRevealBackground::LoadAppsAsync()
{
    if (m_hosts == nullptr) return;

    {
        auto inMemory = ref new Vector<MoonlightApp^>();
        for (auto h : m_hosts)
            if (h->Paired)
                for (auto a : h->Apps) inMemory->Append(a);
        if (inMemory->Size > 0) {
            ShuffleAndApply(inMemory);

            auto targets = ref new Vector<MoonlightHost^>();
            for (auto h : m_hosts)
                if (h->Paired && h->Connected) targets->Append(h);
            if (targets->Size > 0)
                create_task([targets]() {
                    for (auto h : targets)
                        try { h->UpdateApps(); } catch (...) {}
                });
            return;
        }
    }

    Platform::WeakReference weakThis(this);
    Platform::String^ baseImages = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
    baseImages = Platform::String::Concat(baseImages, L"\\images\\");

    auto hostDirs = std::make_shared<std::vector<std::wstring>>();
    for (auto h : m_hosts) {
        if (h->InstanceId != nullptr && !h->InstanceId->IsEmpty())
            hostDirs->push_back(std::wstring(Platform::String::Concat(baseImages,
                Platform::String::Concat(h->InstanceId, L"\\"))->Data()));
    }

    if (hostDirs->empty()) {
        LoadFromNetworkAsync();
        return;
    }

    auto imagePaths = std::make_shared<std::vector<std::wstring>>();
    create_task([hostDirs, imagePaths]() {
        for (auto& dir : *hostDirs) {
            std::wstring search = dir + L"*.png";
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW(search.c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) continue;
            do {
                imagePaths->push_back(dir + fd.cFileName);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }).then([weakThis, imagePaths]() {
        auto dispatcher = Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher;
        dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Low,
            ref new Windows::UI::Core::DispatchedHandler([weakThis, imagePaths]() {
                auto that = weakThis.Resolve<SwipeRevealBackground>();
                if (that == nullptr) return;

                if (!imagePaths->empty()) {
                    auto fromDisk = ref new Platform::Collections::Vector<MoonlightApp^>();
                    for (auto& path : *imagePaths) {
                        auto app = ref new MoonlightApp();
                        app->ImagePath = ref new Platform::String(path.c_str());
                        fromDisk->Append(app);
                    }
                    that->ShuffleAndApply(fromDisk);

                    if (that->m_hosts != nullptr) {
                        auto targets = ref new Platform::Collections::Vector<MoonlightHost^>();
                        for (auto h : that->m_hosts)
                            if (h->Paired && h->Connected) targets->Append(h);
                        if (targets->Size > 0)
                            create_task([targets]() {
                                for (auto h : targets)
                                    try { h->UpdateApps(); } catch (...) {}
                            });
                    }
                    return;
                }

                that->LoadFromNetworkAsync();
            }));
    });
}

void SwipeRevealBackground::LoadFromNetworkAsync()
{
    if (m_hosts == nullptr) return;
    auto targets = ref new Vector<MoonlightHost^>();
    for (auto h : m_hosts)
        if (h->Paired && h->Connected) targets->Append(h);
    if (targets->Size == 0) return;

    Platform::WeakReference weakThis(this);
    create_task([targets]() {
        for (auto h : targets)
            try { h->UpdateApps(); } catch (...) {}
    }).then([weakThis, targets]() {
        auto dispatcher = Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher;
        dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Low,
            ref new Windows::UI::Core::DispatchedHandler([weakThis, targets]() {
                auto that = weakThis.Resolve<SwipeRevealBackground>();
                if (that == nullptr) return;
                auto collected = ref new Vector<MoonlightApp^>();
                for (auto h : targets)
                    for (auto a : h->Apps) collected->Append(a);
                if (collected->Size > 0)
                    that->ShuffleAndApply(collected);
            }));
    });
}

int SwipeRevealBackground::FindNextAppWithImage(int startIdx)
{
    if (!m_appsLoaded || m_apps == nullptr || m_apps->Size == 0) return -1;
    int sz = static_cast<int>(m_apps->Size);
    startIdx = ((startIdx % sz) + sz) % sz;
    for (int i = 0; i < sz; i++) {
        int idx = (startIdx + i) % sz;
        if (m_apps->GetAt(idx)->Image != nullptr) return idx;
    }
    return -1;
}

void SwipeRevealBackground::InitPanForLayer(float& px, float& py, float& vx, float& vy)
{
    static const float dirX[] = {  1, -1,  1, -1 };
    static const float dirY[] = {  1, -1, -1,  1 };
    int d   = (m_panDirIdx++) % 4;
    float sx = dirX[d], sy = dirY[d];
    float speed = 2.0f * kPanMax / static_cast<float>(kHoldTicks + kWipeTicks);
    px = -sx * kPanMax;
    py = -sy * kPanMax;
    vx = sx * speed;
    vy = sy * speed;
}

void SwipeRevealBackground::AdvancePan(float& px, float& py, float vx, float vy, CompositeTransform^ xf)
{
    px += vx;
    py += vy;
    xf->TranslateX = px;
    xf->TranslateY = py;
}

void SwipeRevealBackground::UpdateDiagonalClip(float swept)
{
    if (m_wipeDir > 0) {

        float clipW = swept + kSlantH * 0.5f;
        if (clipW <= 0.0f)
            m_frontClipRect->Rect = Rect(0.0f, 0.0f, 0.0f, m_canvasH);
        else
            m_frontClipRect->Rect = Rect(-kSlantH * 0.5f, 0.0f, clipW, m_canvasH);
    } else {

        float clipW = swept + kSlantH * 0.5f;
        if (clipW <= 0.0f)
            m_frontClipRect->Rect = Rect(m_canvasW, 0.0f, 0.0f, m_canvasH);
        else
            m_frontClipRect->Rect = Rect(m_canvasW - swept, 0.0f, clipW, m_canvasH);
    }
}

void SwipeRevealBackground::InitSlides()
{

    m_backAppIdx = FindNextAppWithImage(0);
    if (m_backAppIdx < 0) return;

    BackBrush->ImageSource = m_apps->GetAt(m_backAppIdx)->Image;

    m_frontAppIdx = FindNextAppWithImage(m_backAppIdx + 1);
    m_frontBrush->ImageSource = (m_frontAppIdx >= 0)
        ? m_apps->GetAt(m_frontAppIdx)->Image : nullptr;

    InitPanForLayer(m_backPanX,  m_backPanY,  m_backVX,  m_backVY);
    InitPanForLayer(m_frontPanX, m_frontPanY, m_frontVX, m_frontVY);
    BackPan->TranslateX      = m_backPanX;  BackPan->TranslateY  = m_backPanY;
    m_frontPan->TranslateX   = m_frontPanX; m_frontPan->TranslateY = m_frontPanY;

    m_wipeDir  = 1;
    m_wipeTick = 0;
    m_phase    = 0;
    m_holdTick = 0;
    GlassEdge->Opacity = 0.0;
    ZeroFrontClips();
    UpdateGlassEdgeSkew();
}

void SwipeRevealBackground::InitSlidesWithWipe()
{

    m_frontAppIdx = FindNextAppWithImage(0);
    if (m_frontAppIdx < 0) { InitSlides(); return; }

    BackBrush->ImageSource   = nullptr;
    m_backAppIdx             = -1;
    m_frontBrush->ImageSource = m_apps->GetAt(m_frontAppIdx)->Image;

    InitPanForLayer(m_backPanX,  m_backPanY,  m_backVX,  m_backVY);
    InitPanForLayer(m_frontPanX, m_frontPanY, m_frontVX, m_frontVY);
    BackPan->TranslateX      = m_backPanX;  BackPan->TranslateY  = m_backPanY;
    m_frontPan->TranslateX   = m_frontPanX; m_frontPan->TranslateY = m_frontPanY;

    m_wipeDir  = 1;
    m_wipeTick = 0;
    m_phase    = 1;
    m_holdTick = 0;
    GlassEdge->Opacity = 1.0;
    ZeroFrontClips();
    UpdateGlassEdgeSkew();
}

void SwipeRevealBackground::AdvanceSlide()
{

    BackBrush->ImageSource  = m_frontBrush->ImageSource;
    BackPan->TranslateX     = m_frontPan->TranslateX;
    BackPan->TranslateY     = m_frontPan->TranslateY;
    m_backPanX = m_frontPanX; m_backPanY = m_frontPanY;
    m_backVX   = m_frontVX;   m_backVY   = m_frontVY;
    m_backAppIdx = m_frontAppIdx;

    m_frontAppIdx = FindNextAppWithImage(m_backAppIdx + 1);
    if (m_frontAppIdx >= 0)
        m_frontBrush->ImageSource = m_apps->GetAt(m_frontAppIdx)->Image;

    InitPanForLayer(m_frontPanX, m_frontPanY, m_frontVX, m_frontVY);
    m_frontPan->TranslateX = m_frontPanX;
    m_frontPan->TranslateY = m_frontPanY;

    m_wipeDir  = -m_wipeDir;
    m_wipeTick = 0;
    m_phase    = 0;
    m_holdTick = 0;
    GlassEdge->Opacity = 0.0;
    ZeroFrontClips();
    UpdateGlassEdgeSkew();
}

void SwipeRevealBackground::OnTick(Object^ sender, Object^ args)
{
    if (!m_initialized) return;

    m_introTick++;

    if (!m_appsLoaded) {
        if (++m_loadRetryTick >= kLoadRetry) {
            m_loadRetryTick = 0;
            LoadAppsAsync();
        }
        return;
    }

    if (m_frontAppIdx < 0) {
        if (++m_imageRetryTick >= 60) {
            m_imageRetryTick = 0;
            if (m_introTick >= kIntroBufferTicks)
                InitSlidesWithWipe();
            else
                InitSlides();
        }
        return;
    }

    AdvancePan(m_backPanX, m_backPanY, m_backVX, m_backVY, BackPan);

    if (m_phase == 0) {
        if (++m_holdTick >= kHoldTicks) {
            m_phase    = 1;
            m_wipeTick = 0;
            GlassEdge->Opacity = 1.0;
        }
    } else {
        AdvancePan(m_frontPanX, m_frontPanY, m_frontVX, m_frontVY, m_frontPan);

        m_wipeTick++;
        float t     = std::min(static_cast<float>(m_wipeTick) / static_cast<float>(kWipeTicks), 1.0f);

        float swept = EaseInOut(t) * (m_canvasW + 2.0f * kWipeMargin) - kWipeMargin;

        UpdateDiagonalClip(swept);

        float cx = (m_wipeDir > 0) ? swept : (m_canvasW - swept);
        GlassEdgeTranslate->X = cx - kGlassWidth * 0.5f;

        if (m_wipeTick >= kWipeTicks)
            AdvanceSlide();
    }
}

void SwipeRevealBackground::StartAnimations()
{
    LoadAppsAsync();
    if (m_timer) m_timer->Start();
}

void SwipeRevealBackground::StopAnimations()
{
    if (m_timer) {
        m_timer->Stop();
        m_timer->Tick -= m_tickToken;
    }
    GlassEdge->Opacity = 0.0;
}
