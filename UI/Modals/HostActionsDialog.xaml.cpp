#include "pch.h"
#include "UI\Modals\HostActionsDialog.xaml.h"

using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

namespace moonlight_xbox_dx
{

namespace {
    void HideIfOpen(ContentDialog^ dialog) {
        try { if (dialog != nullptr) dialog->Hide(); } catch (...) {}
    }
}

HostActionsDialog::HostActionsDialog()
{
    InitializeComponent();

    try {
        this->HostSettingsButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onHostSettings != nullptr) m_onHostSettings->Invoke(sender, args);
        });
        this->WakeHostButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onWakeHost != nullptr) m_onWakeHost->Invoke(sender, args);
        });
        this->TestConnectionButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onTestConnection != nullptr) m_onTestConnection->Invoke(sender, args);
        });
        this->RemoveHostButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onRemoveHost != nullptr) m_onRemoveHost->Invoke(sender, args);
        });
        this->MoonlightSettingsButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onMoonlightSettings != nullptr) m_onMoonlightSettings->Invoke(sender, args);
        });
    } catch (...) {}
}

void HostActionsDialog::Configure(
    Platform::String^ hostName,
    bool showWake,
    bool showTestConnection,
    RoutedEventHandler^ onHostSettings,
    RoutedEventHandler^ onWakeHost,
    RoutedEventHandler^ onTestConnection,
    RoutedEventHandler^ onRemoveHost,
    RoutedEventHandler^ onMoonlightSettings)
{
    m_onHostSettings = onHostSettings;
    m_onWakeHost = onWakeHost;
    m_onTestConnection = onTestConnection;
    m_onRemoveHost = onRemoveHost;
    m_onMoonlightSettings = onMoonlightSettings;

    try {
        if (this->HostNameHeader != nullptr) this->HostNameHeader->Text = hostName;

        if (this->WakeHostButton != nullptr)
            this->WakeHostButton->Visibility = showWake ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
        if (this->TestConnectionButton != nullptr)
            this->TestConnectionButton->Visibility = showTestConnection ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
    } catch (...) {}
}

}
