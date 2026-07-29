#include "pch.h"
#include "StreaksSettingsControl.xaml.h"
#include "UI\Backgrounds\BackgroundSettingsHelpers.h"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

static Windows::UI::Color kNeonScheme[]   = { {255,255,0,0},{255,0,60,255},{255,255,0,220},{255,140,0,255},{255,0,220,255} };
static Windows::UI::Color kOceanScheme[]  = { {255,0,80,255},{255,0,200,200},{255,0,229,255},{255,0,184,122},{255,26,58,255} };
static Windows::UI::Color kSunsetScheme[] = { {255,255,106,0},{255,255,45,120},{255,255,26,26},{255,255,194,0},{255,160,32,240} };
static Windows::UI::Color kWarmScheme[]   = { {255,255,224,0},{255,255,140,0},{255,255,173,0},{255,255,215,0},{255,255,69,0} };
static Windows::UI::Color kMonoScheme[]   = { {255,224,224,224},{255,255,255,255},{255,191,191,191},{255,160,160,160},{255,207,207,207} };

static Windows::UI::Color kSwatches[] = {
    {255,255,  0,  0}, {255,255,106,  0}, {255,255,224,  0}, {255,128,255,  0},
    {255,  0,192, 96}, {255,  0,220,255}, {255,  0, 60,255}, {255,140,  0,255},
    {255,255,  0,220}, {255,255, 45,120}, {255,255,255,255}, {255,128,128,128},
};
static const int kSwatchCount = 12;

static const wchar_t* kCustomKeys[] = {
    L"streaks.custom.0", L"streaks.custom.1", L"streaks.custom.2",
    L"streaks.custom.3", L"streaks.custom.4"
};

StreaksSettingsControl::StreaksSettingsControl()
{
    InitializeComponent();
}

void StreaksSettingsControl::Initialize(DynamicBackgroundHost^ host)
{
    m_host = host;

    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    Platform::String^ scheme = L"neon";
    if (ls->HasKey("streaks.scheme"))
        scheme = safe_cast<Platform::String^>(ls->Lookup("streaks.scheme"));

    struct { const wchar_t* key; const wchar_t* label; } schemes[] = {
        { L"neon",   L"Neon (Default)" },
        { L"ocean",  L"Ocean"          },
        { L"sunset", L"Sunset"         },
        { L"warm",   L"Warm"           },
        { L"mono",   L"Monochrome"     },
        { L"custom", L"Custom"         },
    };

    int selectedIdx = 0;
    for (int i = 0; i < 6; ++i) {
        auto item = ref new ComboBoxItem();
        item->Content   = ref new Platform::String(schemes[i].label);
        item->DataContext = ref new Platform::String(schemes[i].key);
        SchemeSelector->Items->Append(item);
        if (scheme->Equals(ref new Platform::String(schemes[i].key))) selectedIdx = i;
    }
    SchemeSelector->SelectedIndex = selectedIdx;
    UpdateCustomPanelVisibility();

    Windows::UI::Color* defaults = kNeonScheme;
    if      (scheme->Equals(L"ocean"))  defaults = kOceanScheme;
    else if (scheme->Equals(L"sunset")) defaults = kSunsetScheme;
    else if (scheme->Equals(L"warm"))   defaults = kWarmScheme;
    else if (scheme->Equals(L"mono"))   defaults = kMonoScheme;

    SwatchPicker^ pickers[] = { Color0, Color1, Color2, Color3, Color4 };
    for (int i = 0; i < 5; ++i) {
        if (pickers[i] == nullptr) continue;
        pickers[i]->SetSwatches(kSwatches, kSwatchCount);
        Windows::UI::Color c = defaults[i];
        if (scheme->Equals(L"custom")) {
            auto k = ref new Platform::String(kCustomKeys[i]);
            if (ls->HasKey(k)) c = BgHexToColor(safe_cast<Platform::String^>(ls->Lookup(k)), c);
        }
        pickers[i]->SelectColor(c, false);
    }

    Color0->ColorChanged += ref new SwatchColorChangedHandler(this, &StreaksSettingsControl::Color0_ColorChanged);
    Color1->ColorChanged += ref new SwatchColorChangedHandler(this, &StreaksSettingsControl::Color1_ColorChanged);
    Color2->ColorChanged += ref new SwatchColorChangedHandler(this, &StreaksSettingsControl::Color2_ColorChanged);
    Color3->ColorChanged += ref new SwatchColorChangedHandler(this, &StreaksSettingsControl::Color3_ColorChanged);
    Color4->ColorChanged += ref new SwatchColorChangedHandler(this, &StreaksSettingsControl::Color4_ColorChanged);

    m_initialized = true;
}

void StreaksSettingsControl::UpdateCustomPanelVisibility()
{
    if (CustomPanel == nullptr || SchemeSelector == nullptr) return;
    auto item = dynamic_cast<ComboBoxItem^>(SchemeSelector->SelectedItem);
    bool isCustom = (item != nullptr && item->DataContext != nullptr &&
                     item->DataContext->ToString()->Equals(L"custom"));
    CustomPanel->Visibility = isCustom ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
}

void StreaksSettingsControl::SchemeSelector_SelectionChanged(Platform::Object^, SelectionChangedEventArgs^)
{
    if (!m_initialized) return;
    auto item = dynamic_cast<ComboBoxItem^>(SchemeSelector->SelectedItem);
    if (item == nullptr) return;
    auto key = item->DataContext->ToString();

    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert("streaks.scheme", key);

    if (!key->Equals(L"custom")) {
        Windows::UI::Color* defaults = kNeonScheme;
        if      (key->Equals(L"ocean"))  defaults = kOceanScheme;
        else if (key->Equals(L"sunset")) defaults = kSunsetScheme;
        else if (key->Equals(L"warm"))   defaults = kWarmScheme;
        else if (key->Equals(L"mono"))   defaults = kMonoScheme;
        SwatchPicker^ pickers[] = { Color0, Color1, Color2, Color3, Color4 };
        for (int i = 0; i < 5; ++i) {
            if (pickers[i] != nullptr) pickers[i]->SelectColor(defaults[i], false);
        }
    }

    UpdateCustomPanelVisibility();
    try { if (m_host != nullptr) m_host->ReloadBackgroundColors(); } catch (...) {}
}

static void SaveColor(int slot, Windows::UI::Color color)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert(ref new Platform::String(kCustomKeys[slot]), BgColorToHex(color));
}

void StreaksSettingsControl::Color0_ColorChanged(Platform::Object^, Windows::UI::Color color, bool) { SaveColor(0, color); try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {} }
void StreaksSettingsControl::Color1_ColorChanged(Platform::Object^, Windows::UI::Color color, bool) { SaveColor(1, color); try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {} }
void StreaksSettingsControl::Color2_ColorChanged(Platform::Object^, Windows::UI::Color color, bool) { SaveColor(2, color); try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {} }
void StreaksSettingsControl::Color3_ColorChanged(Platform::Object^, Windows::UI::Color color, bool) { SaveColor(3, color); try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {} }
void StreaksSettingsControl::Color4_ColorChanged(Platform::Object^, Windows::UI::Color color, bool) { SaveColor(4, color); try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {} }

void StreaksSettingsControl::ResetButton_Click(Platform::Object^, RoutedEventArgs^)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Remove("streaks.scheme");
    for (int i = 0; i < 5; ++i)
        ls->Remove(ref new Platform::String(kCustomKeys[i]));

    SchemeSelector->SelectedIndex = 0;
    SwatchPicker^ pickers[] = { Color0, Color1, Color2, Color3, Color4 };
    for (int i = 0; i < 5; ++i) {
        if (pickers[i] != nullptr) pickers[i]->SelectColor(kNeonScheme[i], false);
    }
    CustomPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;

    try { if (m_host != nullptr) m_host->ReloadBackgroundColors(); } catch (...) {}
}
