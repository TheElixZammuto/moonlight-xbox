#include "pch.h"
#include "UI\Modals\PairDialog.xaml.h"

using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

namespace moonlight_xbox_dx
{

PairDialog::PairDialog()
{
    InitializeComponent();
}

void PairDialog::Configure(Platform::String^ pin)
{
    try { if (PinText != nullptr) PinText->Text = pin; } catch (...) {}
    try {
        if (OkButton != nullptr) {
            OkButton->Click += ref new RoutedEventHandler([this](Platform::Object^, RoutedEventArgs^) {
                try { this->Hide(); } catch (...) {}
            });
        }
    } catch (...) {}
}

}
