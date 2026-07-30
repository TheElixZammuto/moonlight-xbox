#pragma once

namespace moonlight_xbox_dx {

    struct BackgroundEntry {
        const wchar_t* key;
        const wchar_t* displayName;
    };

    static const BackgroundEntry kBackgrounds[] = {
        { L"streaks",     L"Neon Streaks"       },
        { L"particles",   L"Floating Particles" },
        { L"spheres",     L"Bouncing Bubbles"   },
        { L"blobs",       L"Morphing Blobs"     },
        { L"swipereveal", L"Swipe Reveal"       },
        { L"globegrid",   L"Globe Grid"         },
        { L"orbs",        L"Dancing Orbs"       },
    };

    static const int kBackgroundCount = sizeof(kBackgrounds) / sizeof(kBackgrounds[0]);

}
