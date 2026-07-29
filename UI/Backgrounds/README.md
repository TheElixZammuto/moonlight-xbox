# Dynamic Backgrounds

This directory contains all animated backgrounds for the moonlight-xbox app. Each background is an
independent C++/CX XAML UserControl that plugs into `DynamicBackgroundHost`.

Each background lives in its own subfolder:

```
UI\Backgrounds\
  Particles\       ParticleBackground + ParticleSettingsControl
  Streaks\         StreaksBackground  + StreaksSettingsControl
  Spheres\         SpheresBackground  + SpheresSettingsControl
  Blobs\           BlobsBackground    + BlobsSettingsControl
  Orbs\            OrbsBackground     + OrbsSettingsControl
  SwipeReveal\     SwipeRevealBackground
  GlobeGrid\       GlobeGridBackground
```

---

## Architecture

`DynamicBackgroundHost` is a `UserControl` that manages two `ContentPresenter` slots:

- **`BackgroundPresenter`** — the currently visible, stable background.
- **`FadePresenter`** — the incoming background, crossfaded in over 300ms, then promoted to stable.

On load (and when `Refresh()` is called) the host reads
`ApplicationData::Current->LocalSettings->Values["background"]`, calls `CreateBackground(key)` to
instantiate the right class, and fades it in. It routes `StartAnimations()` / `StopAnimations()` to
the active child via `TryStartAnimations` / `TryStopAnimations` — manual `dynamic_cast` chains
because there is **no base class or interface**.

Each background exposes exactly two required public methods:

```cpp
void StartAnimations();
void StopAnimations();
```

Optional public methods (only implement if needed):

```cpp
void ReloadOptions();              // apply updated LocalSettings without recreating canvas
void SetHosts(IVector<MoonlightHost^>^ hosts);  // only if background renders app art
```

### Per-background settings controls

Backgrounds that expose user-configurable settings (colors, presets, shape, etc.) each own a
companion **settings UserControl** in the same subfolder — e.g. `Orbs\OrbsSettingsControl.xaml`.
`HostSettingsPage` instantiates the right one dynamically via `UpdateBackgroundSettingsContent(key)`
and places it in a `ContentControl`. No changes to `HostSettingsPage` are needed when adding a new
background with settings.

---

## Adding a New Background — Step-by-Step

### 1. Register the key

Open `BackgroundRegistry.h` and add one entry to the `kBackgrounds` array:

```cpp
{ L"mykey", L"My Display Name" },
```

`kBackgroundCount` is computed automatically via `sizeof` — do not touch it.

---

### 2. Create a subfolder and the three source files

```
UI\Backgrounds\Foo\FooBackground.xaml
UI\Backgrounds\Foo\FooBackground.xaml.h
UI\Backgrounds\Foo\FooBackground.xaml.cpp
```

See the [canonical templates](#canonical-templates) below.

---

### 3. Wire into DynamicBackgroundHost.xaml.cpp

Add the `#include` at the top:

```cpp
#include "UI\Backgrounds\Foo\FooBackground.xaml.h"
```

Add one branch to each of the three static functions:

```cpp
// TryStartAnimations
if (auto f = dynamic_cast<FooBackground^>(el)) { f->StartAnimations(); return; }

// TryStopAnimations
if (auto f = dynamic_cast<FooBackground^>(el)) { f->StopAnimations(); return; }

// CreateBackground
if (key->Equals(ref new String(L"mykey"))) return ref new FooBackground();
```

---

### 4. Register in moonlight-xbox-dx.vcxproj

Add three XML entries (copy the pattern from any existing background):

```xml
<!-- in the ClInclude ItemGroup -->
<ClInclude Include="UI\Backgrounds\Foo\FooBackground.xaml.h">
  <DependentUpon>UI\Backgrounds\Foo\FooBackground.xaml</DependentUpon>
</ClInclude>

<!-- in the Page ItemGroup -->
<Page Include="UI\Backgrounds\Foo\FooBackground.xaml" />

<!-- in the ClCompile ItemGroup -->
<ClCompile Include="UI\Backgrounds\Foo\FooBackground.xaml.cpp">
  <DependentUpon>UI\Backgrounds\Foo\FooBackground.xaml</DependentUpon>
</ClCompile>
```

---

### 5. Register in moonlight-xbox-dx.vcxproj.filters

Only the `Page` item needs an explicit entry (the `.h` and `.cpp` are pulled in via `DependentUpon`):

```xml
<Page Include="UI\Backgrounds\Foo\FooBackground.xaml" />
```

---

### 6. (Optional) SetHosts — only if your background renders app art

If your background needs the list of `MoonlightHost^` objects (like `SwipeRevealBackground`), add
to **both** `Refresh()` and `SetHosts()` in `DynamicBackgroundHost.xaml.cpp`:

```cpp
if (auto f = dynamic_cast<FooBackground^>(newBg)) { f->SetHosts(m_hosts); }
```

And declare the method on the class:

```cpp
void SetHosts(Windows::Foundation::Collections::IVector<MoonlightHost^>^ hosts);
```

---

### 7. (Optional) Custom options — only if your background exposes user-configurable settings

If users can adjust settings (colors, speed, density, presets, etc.), create a companion
**settings UserControl** in the same subfolder. `HostSettingsPage` picks it up automatically —
no edits to `HostSettingsPage` are required.

See [Adding Custom Options](#adding-custom-options) for the full pattern.

---

## Canonical Templates

### FooBackground.xaml

```xml
<UserControl
    x:Class="moonlight_xbox_dx.FooBackground"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
    xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
    mc:Ignorable="d">
    <Canvas x:Name="FooCanvas"
            HorizontalAlignment="Stretch"
            VerticalAlignment="Stretch"
            SizeChanged="Canvas_SizeChanged">
        <!-- background color / gradient / static visuals go here -->
    </Canvas>
</UserControl>
```

### FooBackground.xaml.h

```cpp
#pragma once
#include "UI\Backgrounds\Foo\FooBackground.g.h"
#include <vector>
#include <random>

namespace moonlight_xbox_dx {

struct FooState {
    float x, y;
    float vx, vy;
};

public ref class FooBackground sealed {
public:
    FooBackground();
    void StartAnimations();
    void StopAnimations();
private:
    Windows::UI::Xaml::DispatcherTimer^       m_timer;
    Windows::Foundation::EventRegistrationToken m_tickToken;
    std::vector<FooState> m_items;
    std::mt19937          m_rng;
    float m_canvasW    = 0.0f;
    float m_canvasH    = 0.0f;
    bool  m_initialized = false;

    void Canvas_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
    void OnTick(Platform::Object^ sender, Platform::Object^ args);
    void InitItems();
};

}
```

### FooBackground.xaml.cpp

```cpp
#include "pch.h"
#include "UI\Backgrounds\Foo\FooBackground.xaml.h"
#include <cmath>

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Shapes;
using namespace Windows::UI::Xaml::Media;

FooBackground::FooBackground()
{
    m_rng = std::mt19937(std::random_device{}());
    InitializeComponent();

    TimeSpan interval;
    interval.Duration = 16 * 10000LL;  // 16ms ≈ 60fps; use 33*10000LL for 30fps
    m_timer = ref new DispatcherTimer();
    m_timer->Interval = interval;
    m_tickToken = m_timer->Tick += ref new EventHandler<Object^>(this, &FooBackground::OnTick);
}

void FooBackground::Canvas_SizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    m_canvasW = static_cast<float>(e->NewSize.Width);
    m_canvasH = static_cast<float>(e->NewSize.Height);
    if (!m_initialized && m_canvasW > 0 && m_canvasH > 0) {
        InitItems();
        m_initialized = true;
    }
}

void FooBackground::InitItems()
{
    m_items.clear();
    FooCanvas->Children->Clear();
    // Create shapes, push to m_items, append to FooCanvas->Children in a fixed order.
    // OnTick retrieves children by index — order here must match retrieval order there.
}

void FooBackground::OnTick(Object^ sender, Object^ args)
{
    if (!m_initialized) return;
    int count = static_cast<int>(m_items.size());
    for (int i = 0; i < count; ++i) {
        auto& s = m_items[i];
        // update s.x, s.y, etc.
        auto el = safe_cast<Ellipse^>(FooCanvas->Children->GetAt(i));
        Canvas::SetLeft(el, s.x);
        Canvas::SetTop(el,  s.y);
    }
}

void FooBackground::StartAnimations() { if (m_timer) m_timer->Start(); }
void FooBackground::StopAnimations()  { if (m_timer) m_timer->Stop();  }
```

---

## Key Patterns and Invariants

### Timer intervals

| Duration constant | Frame rate | Used by |
|---|---|---|
| `16 * 10000LL` | ~60 fps | Streaks, Spheres, Orbs |
| `33 * 10000LL` | ~30 fps | Particles, GlobeGrid |

### Canvas children index contract

`InitItems()` appends children in a **fixed, known order**. `OnTick` retrieves them by index via
`safe_cast`. Never shuffle or conditionally skip insertions. If each item has multiple visual layers
(e.g., glow + core), push all glows first (indices `0..N-1`) then all cores (indices `N..2N-1`).
See `StreaksBackground` for an example using `kGlowBase` / `kCoreBase` constants.

### SizeChanged / lazy initialization

Canvas dimensions are unknown at construction time. Always gate `InitItems()`:

```cpp
if (!m_initialized && m_canvasW > 0 && m_canvasH > 0) {
    InitItems();
    m_initialized = true;
}
```

### RNG

```cpp
// Seed in constructor:
m_rng = std::mt19937(std::random_device{}());

// Use:
std::uniform_real_distribution<float> dist(minVal, maxVal);
float val = dist(m_rng);
```

### WinRT color construction

```cpp
ColorHelper::FromArgb(alpha, r, g, b);  // all uint8_t; A, R, G, B order
```

### Color helpers (copy from Particles\ParticleBackground.xaml.cpp if needed)

```cpp
// Linear interpolate two Colors
static Color LerpRGB(Color a, Color b, float t, uint8_t alpha = 255);

// Scale (darken) a Color
static Color ScaleRGB(Color c, float s, uint8_t alpha = 255);
```

### C++/CX palette array restrictions

Palette arrays **must be non-const** — `const Windows::UI::Color` fails to compile in C++/CX.
Also do **not** use `ARRAYSIZE` on member arrays; use a literal count instead.

```cpp
// .h
Windows::UI::Color m_palette[4];  // non-const, literal count

// .cpp — OK
int n = 4;
```

---

## Adding Custom Options

Backgrounds with user-configurable settings (colors, speed, density, presets, etc.) expose them
via a companion **settings UserControl** that lives in the same subfolder:

```
UI\Backgrounds\Foo\FooSettingsControl.xaml
UI\Backgrounds\Foo\FooSettingsControl.xaml.h
UI\Backgrounds\Foo\FooSettingsControl.xaml.cpp
```

`HostSettingsPage` calls `UpdateBackgroundSettingsContent(key)` whenever the background selection
changes. That function instantiates the right settings control and places it in a `ContentControl`
— **you do not touch `HostSettingsPage` at all**. Just register the new control in
`UpdateBackgroundSettingsContent` (in `HostSettingsPage.xaml.cpp`):

```cpp
} else if (key->Equals(L"mykey")) {
    auto c = ref new FooSettingsControl();
    c->Initialize(BackgroundHost);
    ctrl = c;
}
```

And add the corresponding `#include` at the top of `HostSettingsPage.xaml.cpp` and
`HostSettingsPage.xaml.h`.

### Settings control structure

**FooSettingsControl.xaml** — A `UserControl` with a scheme `ComboBox`, a collapsed `CustomPanel`
with `SwatchPicker` rows, and a Reset button. See `OrbsSettingsControl.xaml` for a minimal 2-color
example and `BlobsSettingsControl.xaml` for a 4-color example.

**FooSettingsControl.xaml.h**

```cpp
#pragma once
#include "UI\Backgrounds\Foo\FooSettingsControl.g.h"
#include "UI\Backgrounds\DynamicBackgroundHost.xaml.h"
#include "UI\Controls\SwatchPicker.xaml.h"

namespace moonlight_xbox_dx {

public ref class FooSettingsControl sealed {
public:
    FooSettingsControl();
    void Initialize(DynamicBackgroundHost^ host);
private:
    DynamicBackgroundHost^ m_host = nullptr;
    bool m_initialized = false;
    void SchemeSelector_SelectionChanged(Platform::Object^, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^);
    void Color0_ColorChanged(Platform::Object^, Windows::UI::Color, bool);
    void ResetButton_Click(Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^);
    void UpdateCustomPanelVisibility();
};

}
```

**FooSettingsControl.xaml.cpp**

```cpp
#include "pch.h"
#include "FooSettingsControl.xaml.h"
#include "UI\Backgrounds\BackgroundSettingsHelpers.h"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

// Scheme palette arrays — must be non-const (C++/CX restriction)
static Windows::UI::Color kDefaultScheme[] = { {255, 30,160,255}, {255, 0, 8, 20} };
// ... additional schemes ...

static const wchar_t* kCustomKeys[] = { L"mykey.custom.0", L"mykey.custom.1" };

FooSettingsControl::FooSettingsControl() { InitializeComponent(); }

void FooSettingsControl::Initialize(DynamicBackgroundHost^ host)
{
    m_host = host;
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;

    // Populate scheme ComboBox, restore saved index
    // Set up SwatchPicker swatches and call SelectColor(savedColor, false)
    // Wire ColorChanged events

    m_initialized = true;
}

void FooSettingsControl::SchemeSelector_SelectionChanged(Platform::Object^, SelectionChangedEventArgs^)
{
    if (!m_initialized) return;
    // Save scheme key to LocalSettings, update pickers, call UpdateCustomPanelVisibility()
    try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void FooSettingsControl::Color0_ColorChanged(Platform::Object^, Windows::UI::Color color, bool)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert(ref new Platform::String(kCustomKeys[0]), BgColorToHex(color));
    try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void FooSettingsControl::ResetButton_Click(Platform::Object^, RoutedEventArgs^)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Remove("mykey.scheme");
    for (int i = 0; i < 2; ++i) ls->Remove(ref new Platform::String(kCustomKeys[i]));
    SchemeSelector->SelectedIndex = 0;
    // Reset pickers to default scheme colors
    try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {}
}
```

### Init guard pattern

Gate every `SelectionChanged` / `ColorChanged` handler with `if (!m_initialized) return;` to
prevent spurious LocalSettings writes while controls are being programmatically initialized.

### Color hex helpers

Use the shared utilities from `BackgroundSettingsHelpers.h` (already included above):

```cpp
Platform::String^ BgColorToHex(Windows::UI::Color c);  // Color → "RRGGBB"
Windows::UI::Color BgHexToColor(Platform::String^ s, Windows::UI::Color fallback);
```

### Reloading colors in the background

When a setting changes, call `m_host->ReloadBackgroundColors()`. This dispatches to the active
background's `ReloadOptions()` (or equivalent) without recreating the canvas.

> **Important:** Never call `Refresh()` to apply option changes. `Refresh()` is a no-op when the
> background key is unchanged. Always go through the dedicated reload path.

---

## Files Modified for Every New Background

| File | What to add |
|---|---|
| `UI\Backgrounds\BackgroundRegistry.h` | New `{ L"key", L"Display Name" }` entry |
| `UI\Backgrounds\DynamicBackgroundHost.xaml.cpp` | `#include` + branches in `TryStartAnimations`, `TryStopAnimations`, `CreateBackground` |
| `UI\Backgrounds\Foo\FooBackground.xaml` | New file |
| `UI\Backgrounds\Foo\FooBackground.xaml.h` | New file |
| `UI\Backgrounds\Foo\FooBackground.xaml.cpp` | New file |
| `moonlight-xbox-dx.vcxproj` | `ClInclude` + `Page` + `ClCompile` entries (subfolder paths) |
| `moonlight-xbox-dx.vcxproj.filters` | `Page` entry (subfolder path) |

## Additional Files for Backgrounds with Custom Settings

| File | What to add |
|---|---|
| `UI\Backgrounds\Foo\FooSettingsControl.xaml` | New file |
| `UI\Backgrounds\Foo\FooSettingsControl.xaml.h` | New file |
| `UI\Backgrounds\Foo\FooSettingsControl.xaml.cpp` | New file |
| `UI\Pages\HostSettingsPage.xaml.h` | `#include` for `FooSettingsControl.xaml.h` |
| `UI\Pages\HostSettingsPage.xaml.cpp` | `#include` + new `else if` branch in `UpdateBackgroundSettingsContent()` |
| `moonlight-xbox-dx.vcxproj` | `ClInclude` + `Page` + `ClCompile` entries for the settings control |
| `moonlight-xbox-dx.vcxproj.filters` | `Page` entry for the settings control |
