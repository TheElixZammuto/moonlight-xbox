#pragma once

#include "UI\Modals\PairDialog.g.h"

namespace moonlight_xbox_dx
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class PairDialog sealed
    {
    public:
        PairDialog();
        void Configure(Platform::String^ pin);
    };
}
