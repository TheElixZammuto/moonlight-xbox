#include "pch.h"
#include "UI\Modals\StreamErrorDialog.xaml.h"

using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

namespace moonlight_xbox_dx
{

StreamErrorDialog::StreamErrorDialog()
{
    InitializeComponent();
}

void StreamErrorDialog::Configure(
    Platform::String^ title,
    Platform::String^ message,
    Platform::String^ headerGlyph,
    Platform::String^ primaryText,
    Platform::String^ primaryGlyph,
    Platform::String^ secondaryText,
    Platform::String^ secondaryGlyph,
    RoutedEventHandler^ onPrimary,
    RoutedEventHandler^ onSecondary)
{
    try { if (TitleHeader) TitleHeader->Text = title; } catch (...) {}
    try { if (MessageText) MessageText->Text = message; } catch (...) {}
    try { if (TitleIcon && headerGlyph && headerGlyph->Length() > 0) TitleIcon->Glyph = headerGlyph; } catch (...) {}
    try { if (PrimaryButtonLabel && primaryText) PrimaryButtonLabel->Text = primaryText; } catch (...) {}
    try { if (PrimaryButtonIcon && primaryGlyph && primaryGlyph->Length() > 0) PrimaryButtonIcon->Glyph = primaryGlyph; } catch (...) {}
    try { if (SecondaryButtonLabel && secondaryText) SecondaryButtonLabel->Text = secondaryText; } catch (...) {}
    try { if (SecondaryButtonIcon && secondaryGlyph && secondaryGlyph->Length() > 0) SecondaryButtonIcon->Glyph = secondaryGlyph; } catch (...) {}

    try {
        if (PrimaryButton) {
            PrimaryButton->Click += ref new RoutedEventHandler(
                [this, onPrimary](Platform::Object^ sender, RoutedEventArgs^ args) {
                    try { this->Hide(); } catch (...) {}
                    if (onPrimary) onPrimary->Invoke(sender, args);
                });
        }
    } catch (...) {}

    try {
        if (SecondaryButton) {
            SecondaryButton->Click += ref new RoutedEventHandler(
                [this, onSecondary](Platform::Object^ sender, RoutedEventArgs^ args) {
                    try { this->Hide(); } catch (...) {}
                    if (onSecondary) onSecondary->Invoke(sender, args);
                });
        }
    } catch (...) {}
}

}
