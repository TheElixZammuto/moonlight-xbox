#pragma once
#include "UI\Backgrounds\DynamicBackgroundHost.g.h"
#include "State\MoonlightHost.h"

namespace moonlight_xbox_dx {

public ref class DynamicBackgroundHost sealed {
public:
    DynamicBackgroundHost();
    static property Platform::String^ DefaultKey {
        Platform::String^ get() { return ref new Platform::String(L"streaks"); }
    }
    void Refresh();
    void StartAnimations();
    void StopAnimations();
    void ApplyBackground(Platform::String^ key, Platform::String^ scheme);
    void ResetBackground();
    void ReloadBackgroundColors();
    void SetHosts(Windows::Foundation::Collections::IVector<MoonlightHost^>^ hosts);
private:
    Platform::String^ m_currentKey;
    Platform::String^ m_incomingKey;
    Windows::UI::Xaml::Media::Animation::Storyboard^ m_fadeStoryboard;
    Platform::Object^ m_discardedBg;
    Windows::Foundation::Collections::IVector<MoonlightHost^>^ m_hosts;
    void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
};

}
