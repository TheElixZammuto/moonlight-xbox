#include "pch.h"
#include "UI\Controls\LunarPhaseControl.xaml.h"
#define MLOG_TAG_OVERRIDE "LunarPhaseControl"
#include "..\..\Utils.hpp"
#include <cmath>

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace concurrency;

static constexpr long long kAnimationMs = 150;
static constexpr double kShadowTravelPx = 100.0;
static constexpr long long kOrbitLoopMs = 2040;

static std::atomic<long long> s_orbitLoopMs { kOrbitLoopMs };
static std::once_flag s_orbitDurationFlag;

static void ReadOrbitGifDurationAsync()
{
    std::call_once(s_orbitDurationFlag, []() {
        auto uri = ref new Uri(L"ms-appx:///Assets/orbit_load3.gif");
        create_task(StorageFile::GetFileFromApplicationUriAsync(uri))
        .then([](task<StorageFile^> fileTask) -> task<IBuffer^> {
            try {
                return create_task(FileIO::ReadBufferAsync(fileTask.get()));
            } catch (Platform::COMException^ ex) {
                MLOGF(Utils::LogLevel::Warning, "failed to open orbit gif hr=0x%08x — using fallback %lldms\n", ex->HResult, kOrbitLoopMs);
                return task_from_result<IBuffer^>(nullptr);
            } catch (...) {
                MLOGF(Utils::LogLevel::Warning, "failed to open orbit gif (unknown) — using fallback %lldms\n", kOrbitLoopMs);
                return task_from_result<IBuffer^>(nullptr);
            }
        })
        .then([](IBuffer^ buf) -> long long {
            if (buf == nullptr) return kOrbitLoopMs;

            auto reader = DataReader::FromBuffer(buf);
            unsigned int len = buf->Length;
            auto bytes = ref new Array<unsigned char>(len);
            reader->ReadBytes(bytes);

            int frameCount = 0;
            long long totalMs = 0;
            for (unsigned int i = 0; i + 5 < len; i++) {
                if (bytes[i] == 0x21 && bytes[i+1] == 0xF9 && bytes[i+2] == 0x04) {
                    unsigned short cs = bytes[i+4] | (unsigned short)(bytes[i+5] << 8);
                    totalMs += (long long)cs * 10;
                    frameCount++;
                    i += 7;
                }
            }

            if (totalMs > 0) {
                return totalMs;
            }
            MLOGF(Utils::LogLevel::Warning, "orbit gif parse found no GCE frames (len=%u) — using fallback %lldms\n", len, kOrbitLoopMs);
            return kOrbitLoopMs;
        })
        .then([](task<long long> t) {
            try { s_orbitLoopMs = t.get(); } catch (...) {}
        });
    });
}

Windows::UI::Xaml::DependencyProperty^ LunarPhaseControl::m_showOrbitProperty =
    DependencyProperty::Register(
        "ShowOrbit", bool::typeid, LunarPhaseControl::typeid,
        ref new PropertyMetadata(false,
            ref new PropertyChangedCallback(&LunarPhaseControl::OnShowOrbitChanged)));

Windows::UI::Xaml::DependencyProperty^ LunarPhaseControl::m_showLockProperty =
    DependencyProperty::Register(
        "ShowLock", Windows::UI::Xaml::Visibility::typeid, LunarPhaseControl::typeid,
        ref new PropertyMetadata(Windows::UI::Xaml::Visibility::Collapsed,
            ref new PropertyChangedCallback(&LunarPhaseControl::OnShowLockChanged)));

Windows::UI::Xaml::DependencyProperty^ LunarPhaseControl::m_showDisconnectedProperty =
    DependencyProperty::Register(
        "ShowDisconnected", Windows::UI::Xaml::Visibility::typeid, LunarPhaseControl::typeid,
        ref new PropertyMetadata(Windows::UI::Xaml::Visibility::Collapsed,
            ref new PropertyChangedCallback(&LunarPhaseControl::OnShowDisconnectedChanged)));

Windows::UI::Xaml::DependencyProperty^ LunarPhaseControl::m_shadowCenterXProperty =
    DependencyProperty::Register(
        "ShadowCenterX", double::typeid, LunarPhaseControl::typeid,
        ref new PropertyMetadata(80.0,
            ref new PropertyChangedCallback(&LunarPhaseControl::OnShadowCenterXChanged)));

Windows::UI::Xaml::DependencyProperty^ LunarPhaseControl::m_isDashedProperty =
    DependencyProperty::Register(
        "IsDashed", bool::typeid, LunarPhaseControl::typeid,
        ref new PropertyMetadata(false,
            ref new PropertyChangedCallback(&LunarPhaseControl::OnIsDashedChanged)));

LunarPhaseControl::LunarPhaseControl()
{
    InitializeComponent();
    m_phaseStoryboard = nullptr;
    m_selectionStoryboard = nullptr;
    m_orbitHideTimer = nullptr;
    m_orbitShownTick = 0;
    ReadOrbitGifDurationAsync();

    m_outerArc = ref new ArcSegment();
    m_outerArc->Size = Size(76.0f, 76.0f);
    m_outerArc->IsLargeArc = true;
    m_outerArc->SweepDirection = SweepDirection::Clockwise;

    m_innerArc = ref new ArcSegment();
    m_innerArc->Size = Size(76.0f, 76.0f);
    m_innerArc->IsLargeArc = false;
    m_innerArc->SweepDirection = SweepDirection::Counterclockwise;

    m_crescentFigure = ref new PathFigure();
    m_crescentFigure->IsClosed = true;
    m_crescentFigure->IsFilled = true;
    m_crescentFigure->Segments->Append(m_outerArc);
    m_crescentFigure->Segments->Append(m_innerArc);

    auto pathGeo = ref new PathGeometry();
    pathGeo->Figures->Append(m_crescentFigure);

    CrescentPath->Data = pathGeo;

    m_dashedOuterArc = ref new ArcSegment();
    m_dashedOuterArc->Size = Size(76.0f, 76.0f);
    m_dashedOuterArc->IsLargeArc = true;
    m_dashedOuterArc->SweepDirection = SweepDirection::Clockwise;

    m_dashedInnerArc = ref new ArcSegment();
    m_dashedInnerArc->Size = Size(76.0f, 76.0f);
    m_dashedInnerArc->IsLargeArc = false;
    m_dashedInnerArc->SweepDirection = SweepDirection::Counterclockwise;

    m_dashedCrescentFigure = ref new PathFigure();
    m_dashedCrescentFigure->IsClosed = true;
    m_dashedCrescentFigure->IsFilled = true;
    m_dashedCrescentFigure->Segments->Append(m_dashedOuterArc);
    m_dashedCrescentFigure->Segments->Append(m_dashedInnerArc);

    auto dashedPathGeo = ref new PathGeometry();
    dashedPathGeo->Figures->Append(m_dashedCrescentFigure);
    DashedCrescentPath->Data = dashedPathGeo;

    CrescentPath->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    DashedCrescentPath->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
}

bool LunarPhaseControl::ShowOrbit::get()
{
    return (bool)GetValue(m_showOrbitProperty);
}
void LunarPhaseControl::ShowOrbit::set(bool v)
{
    SetValue(m_showOrbitProperty, v);
}

Windows::UI::Xaml::Visibility LunarPhaseControl::ShowLock::get()
{
    return (Windows::UI::Xaml::Visibility)GetValue(m_showLockProperty);
}
void LunarPhaseControl::ShowLock::set(Windows::UI::Xaml::Visibility v)
{
    SetValue(m_showLockProperty, v);
}

Windows::UI::Xaml::Visibility LunarPhaseControl::ShowDisconnected::get()
{
    return (Windows::UI::Xaml::Visibility)GetValue(m_showDisconnectedProperty);
}
void LunarPhaseControl::ShowDisconnected::set(Windows::UI::Xaml::Visibility v)
{
    SetValue(m_showDisconnectedProperty, v);
}

double LunarPhaseControl::ShadowCenterX::get()
{
    return (double)GetValue(m_shadowCenterXProperty);
}
void LunarPhaseControl::ShadowCenterX::set(double v)
{
    SetValue(m_shadowCenterXProperty, v);
}

void LunarPhaseControl::OnShowOrbitChanged(
    DependencyObject^ d, DependencyPropertyChangedEventArgs^ e)
{
    auto ctrl = dynamic_cast<LunarPhaseControl^>(d);
    if (ctrl == nullptr) return;
    bool show = (bool)e->NewValue;

    if (show) {
        if (ctrl->m_orbitHideTimer != nullptr) {
            try { ctrl->m_orbitHideTimer->Stop(); } catch (...) {}
            ctrl->m_orbitHideTimer = nullptr;
        }

        ctrl->OrbitGif->Source = ref new BitmapImage(
            ref new Uri(L"ms-appx:///Assets/orbit_load3.gif"));
        ctrl->OrbitGif->Visibility = Windows::UI::Xaml::Visibility::Visible;
        ctrl->m_orbitShownTick = QpcNow();
        return;
    }

    static constexpr long long kTimerMarginMs = 20;
    long long loopMs = s_orbitLoopMs.load();
    long long rem;
    if (ctrl->m_orbitShownTick == 0) {
        rem = loopMs - kTimerMarginMs;
    } else {
        long long elapsed = (long long)QpcToMs(QpcNow() - ctrl->m_orbitShownTick);
        if (elapsed < 0) elapsed = 0;
        rem = loopMs - (elapsed % loopMs) - kTimerMarginMs;
        if (rem <= 50) {

            ctrl->OrbitGif->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
            return;
        }
    }

    if (ctrl->m_orbitHideTimer != nullptr) {
        try { ctrl->m_orbitHideTimer->Stop(); } catch (...) {}
    }

    auto timer = ref new Windows::UI::Xaml::DispatcherTimer();
    TimeSpan ts;
    ts.Duration = rem * 10000LL;
    timer->Interval = ts;

    Platform::WeakReference weakCtrl(ctrl);
    timer->Tick += ref new Windows::Foundation::EventHandler<Platform::Object^>(
        [weakCtrl](Platform::Object^ sender, Platform::Object^) {
            auto that = weakCtrl.Resolve<LunarPhaseControl>();
            try { dynamic_cast<Windows::UI::Xaml::DispatcherTimer^>(sender)->Stop(); } catch (...) {}
            if (that == nullptr) return;
            that->m_orbitHideTimer = nullptr;
            that->OrbitGif->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        });

    ctrl->m_orbitHideTimer = timer;
    timer->Start();
}

void LunarPhaseControl::OnShowLockChanged(
    DependencyObject^ d, DependencyPropertyChangedEventArgs^ e)
{
    auto ctrl = dynamic_cast<LunarPhaseControl^>(d);
    if (ctrl == nullptr) return;
    ctrl->LockIcon->Visibility = (Windows::UI::Xaml::Visibility)e->NewValue;
}

void LunarPhaseControl::OnShowDisconnectedChanged(
    DependencyObject^ d, DependencyPropertyChangedEventArgs^ e)
{
    auto ctrl = dynamic_cast<LunarPhaseControl^>(d);
    if (ctrl == nullptr) return;
    ctrl->DisconnectedIcon->Visibility = (Windows::UI::Xaml::Visibility)e->NewValue;
}

void LunarPhaseControl::OnShadowCenterXChanged(
    DependencyObject^ d, DependencyPropertyChangedEventArgs^ e)
{
    auto ctrl = dynamic_cast<LunarPhaseControl^>(d);
    if (ctrl == nullptr) return;
    ctrl->SetCrescentPath((double)e->NewValue);
}

bool LunarPhaseControl::IsDashed::get()
{
    return (bool)GetValue(m_isDashedProperty);
}
void LunarPhaseControl::IsDashed::set(bool v)
{
    SetValue(m_isDashedProperty, v);
}

void LunarPhaseControl::OnIsDashedChanged(
    DependencyObject^ d, DependencyPropertyChangedEventArgs^ e)
{
    auto ctrl = dynamic_cast<LunarPhaseControl^>(d);
    if (ctrl == nullptr) return;
    VisualStateManager::GoToState(ctrl, (bool)e->NewValue ? "Dashed" : "Solid", true);
}

void LunarPhaseControl::SetCrescentPath(double sx)
{
    const double cx = 80.0, cy = 80.0, r = 76.0;
    double d = std::abs(sx - cx);

    if (d < 0.5) {
        CrescentPath->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        DashedCrescentPath->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        return;
    }

    CrescentPath->Visibility = Windows::UI::Xaml::Visibility::Visible;
    DashedCrescentPath->Visibility = Windows::UI::Xaml::Visibility::Visible;

    if (d > 2.0 * r - 0.5) d = 2.0 * r - 0.5;

    sx = (sx < cx) ? cx - d : cx + d;

    double ix = (cx + sx) / 2.0;
    double h = std::sqrt(r * r - (d / 2.0) * (d / 2.0));
    double iy1 = cy - h;
    double iy2 = cy + h;

    bool crescentOnRight = (sx < cx);

    Point topPt((float)ix, (float)iy1);
    Point botPt((float)ix, (float)iy2);
    SweepDirection outerSweep = crescentOnRight ? SweepDirection::Clockwise : SweepDirection::Counterclockwise;
    SweepDirection innerSweep = crescentOnRight ? SweepDirection::Counterclockwise : SweepDirection::Clockwise;

    m_crescentFigure->StartPoint = topPt;
    m_outerArc->Point = botPt;
    m_outerArc->IsLargeArc = true;
    m_outerArc->SweepDirection = outerSweep;
    m_innerArc->Point = topPt;
    m_innerArc->IsLargeArc = false;
    m_innerArc->SweepDirection = innerSweep;

    m_dashedCrescentFigure->StartPoint = topPt;
    m_dashedOuterArc->Point = botPt;
    m_dashedOuterArc->IsLargeArc = true;
    m_dashedOuterArc->SweepDirection = outerSweep;
    m_dashedInnerArc->Point = topPt;
    m_dashedInnerArc->IsLargeArc = false;
    m_dashedInnerArc->SweepDirection = innerSweep;
}

void LunarPhaseControl::SetSelected(bool selected, bool animated)
{
    VisualStateManager::GoToState(this, selected ? "Selected" : "Normal", animated);

    double targetWidth = selected ? 160.0 : 96.0;

    double fromWidth = this->Width;

    if (m_selectionStoryboard != nullptr) {
        try { m_selectionStoryboard->Stop(); } catch (...) {}
        m_selectionStoryboard = nullptr;
    }

    if (!animated) {
        this->Width = targetWidth;
        return;
    }

    this->Width = fromWidth;

    auto anim = ref new DoubleAnimation();
    anim->From = fromWidth;
    anim->To = targetWidth;

    TimeSpan ts;
    ts.Duration = 125LL * 10000LL;
    anim->Duration = Windows::UI::Xaml::Duration(ts);
    anim->EnableDependentAnimation = true;

    auto sb = ref new Storyboard();
    Storyboard::SetTarget(anim, this);
    Storyboard::SetTargetProperty(anim, "Width");
    sb->Children->Append(anim);

    m_selectionStoryboard = sb;
    sb->Begin();
}

void LunarPhaseControl::UpdatePhase(double fillAmount, int side, bool animated)
{

    double targetX;
    if (side == 0) {
        targetX = 80.0;
    } else if (side > 0) {
        targetX = 80.0 - fillAmount * kShadowTravelPx;
    } else {
        targetX = 80.0 + fillAmount * kShadowTravelPx;
    }

    double fromX = ShadowCenterX;

    if (m_phaseStoryboard != nullptr) {
        try { m_phaseStoryboard->Stop(); } catch (...) {}
        m_phaseStoryboard = nullptr;
    }

    if (!animated) {
        ShadowCenterX = targetX;
        return;
    }

    ShadowCenterX = fromX;

    auto anim = ref new DoubleAnimation();
    anim->From = fromX;
    anim->To = targetX;

    TimeSpan ts;
    ts.Duration = kAnimationMs * 10000LL;
    anim->Duration = Windows::UI::Xaml::Duration(ts);
    anim->EnableDependentAnimation = true;

    auto ease = ref new CubicEase();
    ease->EasingMode = EasingMode::EaseOut;
    anim->EasingFunction = ease;

    auto sb = ref new Storyboard();
    Storyboard::SetTarget(anim, this);
    Storyboard::SetTargetProperty(anim, "ShadowCenterX");
    sb->Children->Append(anim);

    m_phaseStoryboard = sb;
    sb->Begin();
}
