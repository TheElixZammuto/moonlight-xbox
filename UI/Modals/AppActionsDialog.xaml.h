#pragma once

#include "UI\Modals\AppActionsDialog.g.h"

namespace moonlight_xbox_dx
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class AppActionsDialog sealed
    {
    public:
        AppActionsDialog();

        void Configure(
            Platform::String^ appName,
            bool showResumeClose,
            bool showCloseAndStart,
            bool showStart,
            bool isFavorite,
            Windows::UI::Xaml::RoutedEventHandler^ onResume,
            Windows::UI::Xaml::RoutedEventHandler^ onClose,
            Windows::UI::Xaml::RoutedEventHandler^ onCloseAndStart,
            Windows::UI::Xaml::RoutedEventHandler^ onStart,
            Windows::UI::Xaml::RoutedEventHandler^ onMoonlightSettings,
            Windows::UI::Xaml::RoutedEventHandler^ onHostSettings,
            Windows::UI::Xaml::RoutedEventHandler^ onToggleFavorite);

    private:
        Windows::UI::Xaml::RoutedEventHandler^ m_onResume;
        Windows::UI::Xaml::RoutedEventHandler^ m_onClose;
        Windows::UI::Xaml::RoutedEventHandler^ m_onCloseAndStart;
        Windows::UI::Xaml::RoutedEventHandler^ m_onStart;
        Windows::UI::Xaml::RoutedEventHandler^ m_onMoonlightSettings;
        Windows::UI::Xaml::RoutedEventHandler^ m_onHostSettings;
        Windows::UI::Xaml::RoutedEventHandler^ m_onToggleFavorite;
    };
}
