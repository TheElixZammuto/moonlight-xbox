#include "pch.h"
#include "UI\Modals\TestConnectionResultDialog.xaml.h"
#include "Utils.hpp"

using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;

namespace moonlight_xbox_dx
{

TestConnectionResultDialog::TestConnectionResultDialog()
{
    InitializeComponent();
}

void TestConnectionResultDialog::Configure(Platform::String^ hostname, Platform::String^ resultMsg)
{
    try { if (HostNameHeader != nullptr) HostNameHeader->Text = hostname; } catch (...) {}

    std::string msg = Utils::PlatformStringToStdString(resultMsg);
    bool isSuccess = (msg.rfind("Connection OK", 0) == 0);

    std::string statusText = msg;
    auto parenPos = msg.find(" (RTT:");
    if (parenPos != std::string::npos) statusText = msg.substr(0, parenPos);

    double rttMs = -1.0;
    auto rttPos = msg.find("RTT: ");
    if (rttPos != std::string::npos) {
        try { rttMs = std::stod(msg.substr(rttPos + 5)); } catch (...) {}
    }

    try { if (StatusText != nullptr) StatusText->Text = Utils::StringFromStdString(statusText); } catch (...) {}

    try {
        if (ResultIcon != nullptr) {
            if (isSuccess) {
                ResultIcon->Glyph = L"\xE73E";
                ResultIcon->Foreground = ref new SolidColorBrush(
                    Windows::UI::ColorHelper::FromArgb(255, 76, 175, 80));
            } else {
                ResultIcon->Glyph = L"\xE711";
                ResultIcon->Foreground = ref new SolidColorBrush(
                    Windows::UI::ColorHelper::FromArgb(255, 244, 67, 54));
            }
        }
    } catch (...) {}

    try {
        if (rttMs >= 0 && RttBadge != nullptr && RttText != nullptr) {
            char buf[32];
            snprintf(buf, sizeof(buf), "RTT: %.1f ms", rttMs);
            RttText->Text = Utils::StringFromStdString(std::string(buf));
            RttBadge->Visibility = Windows::UI::Xaml::Visibility::Visible;
        }
    } catch (...) {}

    try {
        if (OkButton != nullptr) {
            OkButton->Click += ref new RoutedEventHandler([this](Platform::Object^, RoutedEventArgs^) {
                try { this->Hide(); } catch (...) {}
            });
        }
    } catch (...) {}
}

}
