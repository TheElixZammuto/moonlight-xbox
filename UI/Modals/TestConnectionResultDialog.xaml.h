#pragma once

#include "UI\Modals\TestConnectionResultDialog.g.h"

namespace moonlight_xbox_dx
{
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class TestConnectionResultDialog sealed
    {
    public:
        TestConnectionResultDialog();
        void Configure(Platform::String^ hostname, Platform::String^ resultMsg);
    };
}
