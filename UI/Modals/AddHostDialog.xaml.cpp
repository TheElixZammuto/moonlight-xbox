#include "pch.h"
#include "UI\Modals\AddHostDialog.xaml.h"

using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::Foundation::Collections;

namespace moonlight_xbox_dx
{

AddHostDialog::AddHostDialog()
{
    InitializeComponent();
}

void AddHostDialog::Configure(
    RoutedEventHandler^ onAdd,
    RoutedEventHandler^ onCancel)
{
    m_onAdd = onAdd;
    m_onCancel = onCancel;

    try {
        AddButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            if (ErrorText != nullptr) ErrorText->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
            if (m_onAdd != nullptr) m_onAdd->Invoke(sender, args);
        });

        CancelButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
            try { this->Hide(); } catch (...) {}
            if (m_onCancel != nullptr) m_onCancel->Invoke(sender, args);
        });

        HostnameTextBox->KeyDown += ref new KeyEventHandler([this](Platform::Object^, KeyRoutedEventArgs^ e) {
            if (e->Key == Windows::System::VirtualKey::Enter) {
                try { Windows::UI::ViewManagement::InputPane::GetForCurrentView()->TryHide(); } catch (...) {}
                if (ErrorText != nullptr) ErrorText->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
                if (m_onAdd != nullptr) m_onAdd->Invoke(this, ref new RoutedEventArgs());
            }
        });
    } catch (...) {}
}

Platform::String^ AddHostDialog::GetHostname()
{
    try { if (HostnameTextBox != nullptr) return HostnameTextBox->Text; } catch (...) {}
    return nullptr;
}

void AddHostDialog::ShowError(Platform::String^ message)
{
    try {
        if (ErrorText != nullptr) {
            ErrorText->Text = message;
            ErrorText->Visibility = Windows::UI::Xaml::Visibility::Visible;
        }
    } catch (...) {}
}

void AddHostDialog::SetAddButtonEnabled(bool enabled)
{
    try { if (AddButton != nullptr) AddButton->IsEnabled = enabled; } catch (...) {}
}

void AddHostDialog::SetRecentHostnames(IVector<Platform::String^>^ hostnames, IVector<Platform::String^>^ displayNames)
{
    try {
        if (hostnames == nullptr || hostnames->Size == 0) return;
        RecentHostsBorder->Visibility = Windows::UI::Xaml::Visibility::Visible;
        for (unsigned int i = 0; i < hostnames->Size; i++) {
            auto hostname = hostnames->GetAt(i);
            auto computerName = (displayNames != nullptr && displayNames->Size > i) ? displayNames->GetAt(i) : nullptr;
            auto btn = ref new Button();
            if (computerName != nullptr && computerName->Length() > 0) {
                auto content = ref new StackPanel();
                content->Orientation = Windows::UI::Xaml::Controls::Orientation::Horizontal;
                content->Spacing = 6;
                auto nameText = ref new TextBlock();
                nameText->Text = computerName;
                nameText->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily("Bahnschrift");
                nameText->Foreground = ref new Windows::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::White);
                nameText->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
                auto addressText = ref new TextBlock();
                addressText->Text = "(" + hostname + ")";
                addressText->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily("Bahnschrift");
                addressText->FontSize = 10;
                Windows::UI::Color dimWhite;
                dimWhite.A = 0x99; dimWhite.R = 0xFF; dimWhite.G = 0xFF; dimWhite.B = 0xFF;
                addressText->Foreground = ref new Windows::UI::Xaml::Media::SolidColorBrush(dimWhite);
                addressText->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
                content->Children->Append(nameText);
                content->Children->Append(addressText);
                btn->Content = content;
            } else {
                btn->Content = hostname;
            }
            btn->FontFamily = ref new Windows::UI::Xaml::Media::FontFamily("Bahnschrift");
            btn->FontSize = 12;
            btn->Height = 36;
            btn->HorizontalAlignment = Windows::UI::Xaml::HorizontalAlignment::Stretch;
            btn->HorizontalContentAlignment = Windows::UI::Xaml::HorizontalAlignment::Left;
            Windows::UI::Xaml::CornerRadius cr;
            cr.TopLeft = cr.TopRight = cr.BottomRight = cr.BottomLeft = 8.0;
            btn->CornerRadius = cr;
            btn->Padding = Windows::UI::Xaml::Thickness{ 12, 0, 12, 0 };
            Windows::UI::Color bgColor;
            bgColor.A = 0x88; bgColor.R = 0; bgColor.G = 0; bgColor.B = 0;
            btn->Background = ref new Windows::UI::Xaml::Media::SolidColorBrush(bgColor);
            btn->Foreground = ref new Windows::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::White);
            btn->Click += ref new RoutedEventHandler([this, hostname](Platform::Object^ sender, RoutedEventArgs^ args) {
                try {
                    HostnameTextBox->Text = hostname;
                    if (ErrorText != nullptr) ErrorText->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
                    if (m_onAdd != nullptr) m_onAdd->Invoke(sender, args);
                } catch (...) {}
            });
            RecentHostsPanel->Children->Append(btn);
        }
    } catch (...) {}
}

}
