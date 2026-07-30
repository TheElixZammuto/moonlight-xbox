#include "pch.h"
#include "UI\Modals\AppActionsDialog.xaml.h"

using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

namespace moonlight_xbox_dx
{

namespace {
    void HideIfOpen(ContentDialog^ dialog) {
        try { if (dialog != nullptr) dialog->Hide(); } catch (...) {}
    }
}

AppActionsDialog::AppActionsDialog()
{
    InitializeComponent();

    try {
        this->ResumeButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onResume != nullptr) m_onResume->Invoke(sender, args);
        });
        this->CloseButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onClose != nullptr) m_onClose->Invoke(sender, args);
        });
        this->CloseAndStartButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onCloseAndStart != nullptr) m_onCloseAndStart->Invoke(sender, args);
        });
        this->StartButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onStart != nullptr) m_onStart->Invoke(sender, args);
        });
        this->MoonlightSettingsButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onMoonlightSettings != nullptr) m_onMoonlightSettings->Invoke(sender, args);
        });
        this->HostSettingsButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onHostSettings != nullptr) m_onHostSettings->Invoke(sender, args);
        });
        this->FavoriteButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            HideIfOpen(this);
            if (m_onToggleFavorite != nullptr) m_onToggleFavorite->Invoke(sender, args);
        });
    } catch (...) {}
}

void AppActionsDialog::Configure(
    Platform::String^ appName,
    bool showResumeClose,
    bool showCloseAndStart,
    bool showStart,
    bool isFavorite,
    RoutedEventHandler^ onResume,
    RoutedEventHandler^ onClose,
    RoutedEventHandler^ onCloseAndStart,
    RoutedEventHandler^ onStart,
    RoutedEventHandler^ onMoonlightSettings,
    RoutedEventHandler^ onHostSettings,
    RoutedEventHandler^ onToggleFavorite)
{
    m_onResume = onResume;
    m_onClose = onClose;
    m_onCloseAndStart = onCloseAndStart;
    m_onStart = onStart;
    m_onMoonlightSettings = onMoonlightSettings;
    m_onHostSettings = onHostSettings;
    m_onToggleFavorite = onToggleFavorite;

    try {
        if (this->AppNameHeader != nullptr) this->AppNameHeader->Text = appName;

        if (this->FavoriteIcon != nullptr)
            this->FavoriteIcon->Glyph = isFavorite ? L"\xE735" : L"\xE734";
        if (this->FavoriteLabel != nullptr)
            this->FavoriteLabel->Text = isFavorite ? "Remove from Favorites" : "Add to Favorites";

        if (this->ResumeButton != nullptr)
            this->ResumeButton->Visibility = showResumeClose ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
        if (this->CloseButton != nullptr)
            this->CloseButton->Visibility = showResumeClose ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;

        if (this->CloseAndStartButton != nullptr)
            this->CloseAndStartButton->Visibility = showCloseAndStart ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
        if (this->StartButton != nullptr)
            this->StartButton->Visibility = showStart ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
    } catch (...) {}
}

}
