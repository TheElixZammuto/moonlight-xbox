#pragma once

#include "UI\Modals\AlertDialog.g.h"

namespace moonlight_xbox_dx
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class AlertDialog sealed
    {
    public:
        AlertDialog();
        void Configure(Platform::String^ title, Platform::String^ message);
        void Configure(Platform::String^ title, Platform::String^ message, Platform::String^ glyph);
    };
}
