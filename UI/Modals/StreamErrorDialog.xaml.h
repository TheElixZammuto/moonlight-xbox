#pragma once

#include "UI\Modals\StreamErrorDialog.g.h"

namespace moonlight_xbox_dx
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class StreamErrorDialog sealed
    {
    public:
        StreamErrorDialog();
        void Configure(Platform::String^ title,
                       Platform::String^ message,
                       Platform::String^ headerGlyph,
                       Platform::String^ primaryText,
                       Platform::String^ primaryGlyph,
                       Platform::String^ secondaryText,
                       Platform::String^ secondaryGlyph,
                       Windows::UI::Xaml::RoutedEventHandler^ onPrimary,
                       Windows::UI::Xaml::RoutedEventHandler^ onSecondary);
    };
}
