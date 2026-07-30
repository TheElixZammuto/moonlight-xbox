#include "pch.h"
#include "UI\Backgrounds\DynamicBackgroundHost.xaml.h"
#include "UI\Backgrounds\BackgroundRegistry.h"
#include "UI\Backgrounds\Particles\ParticleBackground.xaml.h"
#include "UI\Backgrounds\Spheres\SpheresBackground.xaml.h"
#include "UI\Backgrounds\Streaks\StreaksBackground.xaml.h"
#include "UI\Backgrounds\Blobs\BlobsBackground.xaml.h"
#include "UI\Backgrounds\SwipeReveal\SwipeRevealBackground.xaml.h"
#include "UI\Backgrounds\GlobeGrid\GlobeGridBackground.xaml.h"
#include "UI\Backgrounds\Orbs\OrbsBackground.xaml.h"

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::Storage;

static void TryStartAnimations(UIElement^ el)
{
    if (auto p = dynamic_cast<ParticleBackground^>(el))    { p->StartAnimations(); return; }
    if (auto s = dynamic_cast<SpheresBackground^>(el))     { s->StartAnimations(); return; }
    if (auto k = dynamic_cast<StreaksBackground^>(el))     { k->StartAnimations(); return; }
    if (auto l = dynamic_cast<BlobsBackground^>(el))       { l->StartAnimations(); return; }
    if (auto sr = dynamic_cast<SwipeRevealBackground^>(el)) { sr->StartAnimations(); return; }
    if (auto gg = dynamic_cast<GlobeGridBackground^>(el))   { gg->StartAnimations(); return; }
    if (auto ob = dynamic_cast<OrbsBackground^>(el))        { ob->StartAnimations(); return; }
}

static void TryStopAnimations(UIElement^ el)
{
    if (auto p = dynamic_cast<ParticleBackground^>(el))    { p->StopAnimations(); return; }
    if (auto s = dynamic_cast<SpheresBackground^>(el))     { s->StopAnimations(); return; }
    if (auto k = dynamic_cast<StreaksBackground^>(el))     { k->StopAnimations(); return; }
    if (auto l = dynamic_cast<BlobsBackground^>(el))       { l->StopAnimations(); return; }
    if (auto sr = dynamic_cast<SwipeRevealBackground^>(el)) { sr->StopAnimations(); return; }
    if (auto gg = dynamic_cast<GlobeGridBackground^>(el))   { gg->StopAnimations(); return; }
    if (auto ob = dynamic_cast<OrbsBackground^>(el))        { ob->StopAnimations(); return; }
}

static UIElement^ CreateBackground(String^ key)
{
    if (key != nullptr) {
        if (key->Equals(ref new String(L"particles")))   return ref new ParticleBackground();
        if (key->Equals(ref new String(L"spheres")))     return ref new SpheresBackground();
        if (key->Equals(ref new String(L"streaks")))     return ref new StreaksBackground();
        if (key->Equals(ref new String(L"blobs")))       return ref new BlobsBackground();
        if (key->Equals(ref new String(L"swipereveal"))) return ref new SwipeRevealBackground();
        if (key->Equals(ref new String(L"globegrid")))   return ref new GlobeGridBackground();
        if (key->Equals(ref new String(L"orbs")))         return ref new OrbsBackground();
    }
    return ref new StreaksBackground();
}

DynamicBackgroundHost::DynamicBackgroundHost()
{
    InitializeComponent();
    this->Loaded += ref new RoutedEventHandler(this, &DynamicBackgroundHost::OnLoaded);
}

void DynamicBackgroundHost::OnLoaded(Object^ sender, RoutedEventArgs^ e)
{
    Refresh();
}

void DynamicBackgroundHost::Refresh()
{
    try {
        auto localSettings = ApplicationData::Current->LocalSettings->Values;
        String^ key = L"streaks";
        if (localSettings->HasKey("background")) {
            key = safe_cast<String^>(localSettings->Lookup("background"));
        }

        m_discardedBg = nullptr;

        if (m_incomingKey != nullptr && m_incomingKey->Equals(key)) return;
        if (m_incomingKey == nullptr && m_currentKey != nullptr && m_currentKey->Equals(key)) return;

        if (m_fadeStoryboard != nullptr) {
            try { m_fadeStoryboard->Stop(); } catch (...) {}
            m_fadeStoryboard = nullptr;
            auto incomingEl = dynamic_cast<UIElement^>(FadePresenter->Content);
            if (incomingEl != nullptr) {
                m_discardedBg = incomingEl;
                TryStopAnimations(incomingEl);
            }
            FadePresenter->Content = nullptr;
            FadePresenter->Opacity = 0.0;
            m_incomingKey = nullptr;
        }

        auto newBg = CreateBackground(key);
        if (auto sr = dynamic_cast<SwipeRevealBackground^>(newBg)) { sr->SetHosts(m_hosts); }
        FadePresenter->Content = newBg;
        FadePresenter->Opacity = 0.0;
        m_incomingKey = key;

        auto anim = ref new DoubleAnimation();
        anim->From = 0.0;
        anim->To = 1.0;
        TimeSpan ts; ts.Duration = 3000000LL;
        anim->Duration = DurationHelper::FromTimeSpan(ts);

        auto sb = ref new Storyboard();
        Storyboard::SetTarget(anim, FadePresenter);
        Storyboard::SetTargetProperty(anim, "Opacity");
        sb->Children->Append(anim);
        m_fadeStoryboard = sb;

        auto weakThis = WeakReference(this);
        sb->Completed += ref new EventHandler<Object^>(
            [weakThis](Object^, Object^) {
                auto that = weakThis.Resolve<DynamicBackgroundHost>();
                if (that == nullptr) return;
                try {

                    auto oldEl = dynamic_cast<UIElement^>(that->BackgroundPresenter->Content);
                    if (oldEl != nullptr) {
                        that->m_discardedBg = oldEl;
                        TryStopAnimations(oldEl);
                    }
                    auto incoming = that->FadePresenter->Content;
                    that->FadePresenter->Content = nullptr;
                    that->FadePresenter->Opacity = 0.0;
                    that->BackgroundPresenter->Content = incoming;
                    that->m_currentKey = that->m_incomingKey;
                    that->m_incomingKey = nullptr;
                    that->m_fadeStoryboard = nullptr;
                } catch (...) {}
            });

        sb->Begin();
    } catch (...) {}
}

void DynamicBackgroundHost::SetHosts(Windows::Foundation::Collections::IVector<MoonlightHost^>^ hosts)
{
    m_hosts = hosts;
    try {
        auto el = dynamic_cast<UIElement^>(BackgroundPresenter->Content);
        if (auto sr = dynamic_cast<SwipeRevealBackground^>(el)) { sr->SetHosts(hosts); }
    } catch (...) {}
    try {
        auto el = dynamic_cast<UIElement^>(FadePresenter->Content);
        if (auto sr = dynamic_cast<SwipeRevealBackground^>(el)) { sr->SetHosts(hosts); }
    } catch (...) {}
}

void DynamicBackgroundHost::StartAnimations()
{
    try {
        auto el = dynamic_cast<UIElement^>(BackgroundPresenter->Content);
        if (el != nullptr) TryStartAnimations(el);
    } catch (...) {}
    try {
        auto el = dynamic_cast<UIElement^>(FadePresenter->Content);
        if (el != nullptr) TryStartAnimations(el);
    } catch (...) {}
}

void DynamicBackgroundHost::StopAnimations()
{
    m_discardedBg = nullptr;
    if (m_fadeStoryboard != nullptr) {
        try { m_fadeStoryboard->Stop(); } catch (...) {}
        m_fadeStoryboard = nullptr;
    }
    try {
        auto el = dynamic_cast<UIElement^>(BackgroundPresenter->Content);
        if (el != nullptr) TryStopAnimations(el);
    } catch (...) {}
    try {
        auto el = dynamic_cast<UIElement^>(FadePresenter->Content);
        if (el != nullptr) TryStopAnimations(el);
    } catch (...) {}
}

void DynamicBackgroundHost::ReloadBackgroundColors()
{
    try {
        auto el = dynamic_cast<UIElement^>(BackgroundPresenter->Content);
        if (auto p  = dynamic_cast<ParticleBackground^>(el)) { p->ReloadColors();  return; }
        if (auto k  = dynamic_cast<StreaksBackground^>(el))  { k->ReloadColors();  return; }
        if (auto s  = dynamic_cast<SpheresBackground^>(el))  { s->ReloadColors();  return; }
        if (auto ob = dynamic_cast<OrbsBackground^>(el))     { ob->ReloadColors(); return; }
        if (auto l  = dynamic_cast<BlobsBackground^>(el))    { l->ReloadColors();  return; }
    } catch (...) {}
}

void DynamicBackgroundHost::ApplyBackground(Platform::String^ key, Platform::String^ scheme)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert("background", key);
    if (scheme != nullptr)
        ls->Insert(key + ".scheme", scheme);
    Refresh();
    StartAnimations();
}

void DynamicBackgroundHost::ResetBackground()
{
    ApplyBackground(DefaultKey, "neon");
}
