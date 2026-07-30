#include "pch.h"
#include "UI\Modals\AlertDialog.xaml.h"

using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

namespace moonlight_xbox_dx
{

AlertDialog::AlertDialog()
{
    InitializeComponent();
}

void AlertDialog::Configure(Platform::String^ title, Platform::String^ message)
{
    Configure(title, message, nullptr);
}

void AlertDialog::Configure(Platform::String^ title, Platform::String^ message, Platform::String^ glyph)
{
    try { if (TitleHeader != nullptr) TitleHeader->Text = title; } catch (...) {}
    try { if (MessageText != nullptr) MessageText->Text = message; } catch (...) {}
    try {
        if (TitleIcon != nullptr && glyph != nullptr && glyph->Length() > 0)
            TitleIcon->Glyph = glyph;
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
