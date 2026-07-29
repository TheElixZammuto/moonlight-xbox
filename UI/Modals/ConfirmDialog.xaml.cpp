#include "pch.h"
#include "UI\Modals\ConfirmDialog.xaml.h"

using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

namespace moonlight_xbox_dx
{

ConfirmDialog::ConfirmDialog()
{
    InitializeComponent();
    try {
        if (ConfirmButton != nullptr) {
            ConfirmButton->Click += ref new RoutedEventHandler([this](Platform::Object^ sender, RoutedEventArgs^ args) {
                try { this->Hide(); } catch (...) {}
                if (m_onConfirm != nullptr) m_onConfirm->Invoke(sender, args);
            });
        }
        if (CancelButton != nullptr) {
            CancelButton->Click += ref new RoutedEventHandler([this](Platform::Object^, RoutedEventArgs^) {
                try { this->Hide(); } catch (...) {}
            });
        }
    } catch (...) {}
}

void ConfirmDialog::Configure(Platform::String^ title, Platform::String^ message,
                               RoutedEventHandler^ onConfirm)
{
    Configure(title, message, nullptr, nullptr, nullptr, onConfirm);
}

void ConfirmDialog::Configure(Platform::String^ title, Platform::String^ message, Platform::String^ glyph,
                               RoutedEventHandler^ onConfirm)
{
    Configure(title, message, glyph, nullptr, nullptr, onConfirm);
}

void ConfirmDialog::Configure(Platform::String^ title, Platform::String^ message, Platform::String^ glyph,
                               Platform::String^ confirmText, Platform::String^ cancelText,
                               RoutedEventHandler^ onConfirm)
{
    m_onConfirm = onConfirm;
    try { if (TitleHeader != nullptr) TitleHeader->Text = title; } catch (...) {}
    try { if (MessageText != nullptr) MessageText->Text = message; } catch (...) {}
    try {
        if (TitleIcon != nullptr && glyph != nullptr && glyph->Length() > 0)
            TitleIcon->Glyph = glyph;
    } catch (...) {}
    try {
        if (ConfirmButtonText != nullptr && confirmText != nullptr && confirmText->Length() > 0)
            ConfirmButtonText->Text = confirmText;
    } catch (...) {}
    try {
        if (CancelButtonText != nullptr && cancelText != nullptr && cancelText->Length() > 0)
            CancelButtonText->Text = cancelText;
    } catch (...) {}
}

}
