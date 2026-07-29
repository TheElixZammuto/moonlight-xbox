#pragma once

#include "UI\Modals\ConfirmDialog.g.h"

namespace moonlight_xbox_dx
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class ConfirmDialog sealed
    {
    public:
        ConfirmDialog();
        void Configure(Platform::String^ title, Platform::String^ message,
                       Windows::UI::Xaml::RoutedEventHandler^ onConfirm);
        void Configure(Platform::String^ title, Platform::String^ message, Platform::String^ glyph,
                       Windows::UI::Xaml::RoutedEventHandler^ onConfirm);
        void Configure(Platform::String^ title, Platform::String^ message, Platform::String^ glyph,
                       Platform::String^ confirmText, Platform::String^ cancelText,
                       Windows::UI::Xaml::RoutedEventHandler^ onConfirm);

    private:
        Windows::UI::Xaml::RoutedEventHandler^ m_onConfirm;
    };
}
