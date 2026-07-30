#pragma once

#include "UI\Modals\HostActionsDialog.g.h"

namespace moonlight_xbox_dx
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class HostActionsDialog sealed
    {
    public:
        HostActionsDialog();

        void Configure(
            Platform::String^ hostName,
            bool showWake,
            bool showTestConnection,
            Windows::UI::Xaml::RoutedEventHandler^ onHostSettings,
            Windows::UI::Xaml::RoutedEventHandler^ onWakeHost,
            Windows::UI::Xaml::RoutedEventHandler^ onTestConnection,
            Windows::UI::Xaml::RoutedEventHandler^ onRemoveHost,
            Windows::UI::Xaml::RoutedEventHandler^ onMoonlightSettings);

    private:
        Windows::UI::Xaml::RoutedEventHandler^ m_onHostSettings;
        Windows::UI::Xaml::RoutedEventHandler^ m_onWakeHost;
        Windows::UI::Xaml::RoutedEventHandler^ m_onTestConnection;
        Windows::UI::Xaml::RoutedEventHandler^ m_onRemoveHost;
        Windows::UI::Xaml::RoutedEventHandler^ m_onMoonlightSettings;
    };
}
