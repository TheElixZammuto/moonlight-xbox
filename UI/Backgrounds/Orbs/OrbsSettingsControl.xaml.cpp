#include "pch.h"
#include "OrbsSettingsControl.xaml.h"
#include "UI\Backgrounds\BackgroundSettingsHelpers.h"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

static Windows::UI::Color kElectricUI[] = { {255, 30,160,255}, {255,  0,  8, 20} };
static Windows::UI::Color kAuroraUI[]   = { {255,  0,220,150}, {255,  0, 10, 15} };
static Windows::UI::Color kSolarUI[]    = { {255,255,140,  0}, {255, 12,  5,  0} };
static Windows::UI::Color kNebulaUI[]   = { {255,200, 80,255}, {255,  8,  0, 20} };
static Windows::UI::Color kRoseUI[]     = { {255,255, 60,140}, {255, 15,  0, 12} };

static Windows::UI::Color kGlowSwatches[] = {
    {255, 30,160,255}, {255,  0,220,150}, {255,255,140,  0}, {255,200, 80,255},
    {255,255, 60,140}, {255,  0,240,240}, {255,255,  0,  0}, {255,  0,255,128},
    {255,255,255,  0}, {255,255,128,  0}, {255,128,  0,255}, {255,255,255,255},
};
static const int kGlowSwatchCount = 12;

static Windows::UI::Color kBgSwatches[] = {
    {255,  0,  8, 20}, {255,  0, 10, 15}, {255, 12,  5,  0}, {255,  8,  0, 20},
    {255, 15,  0, 12}, {255,  0,  0,  0}, {255,  5, 10,  0}, {255, 10,  5,  5},
};
static const int kBgSwatchCount = 8;

static const wchar_t* kCustomKeys[] = { L"orbs.custom.0", L"orbs.custom.1" };

OrbsSettingsControl::OrbsSettingsControl()
{
    InitializeComponent();
}

void OrbsSettingsControl::Initialize(DynamicBackgroundHost^ host)
{
    m_host = host;

    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    Platform::String^ scheme = L"electric";
    if (ls->HasKey("orbs.scheme"))
        scheme = safe_cast<Platform::String^>(ls->Lookup("orbs.scheme"));

    struct { const wchar_t* key; const wchar_t* label; } schemes[] = {
        { L"electric", L"Electric (Default)" },
        { L"aurora",   L"Aurora"             },
        { L"solar",    L"Solar"              },
        { L"nebula",   L"Nebula"             },
        { L"rose",     L"Rose"               },
        { L"custom",   L"Custom"             },
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

    Windows::UI::Color* defaults = kElectricUI;
    if      (scheme->Equals(L"aurora")) defaults = kAuroraUI;
    else if (scheme->Equals(L"solar"))  defaults = kSolarUI;
    else if (scheme->Equals(L"nebula")) defaults = kNebulaUI;
    else if (scheme->Equals(L"rose"))   defaults = kRoseUI;

    if (Color0 != nullptr) {
        Color0->SetSwatches(kGlowSwatches, kGlowSwatchCount);
        Windows::UI::Color c0 = defaults[0];
        if (scheme->Equals(L"custom")) {
            auto k = ref new Platform::String(kCustomKeys[0]);
            if (ls->HasKey(k)) c0 = BgHexToColor(safe_cast<Platform::String^>(ls->Lookup(k)), c0);
        }
        Color0->SelectColor(c0, false);
        Color0->ColorChanged += ref new SwatchColorChangedHandler(this, &OrbsSettingsControl::Color0_ColorChanged);
    }

    if (Color1 != nullptr) {
        Color1->SetSwatches(kBgSwatches, kBgSwatchCount);
        Windows::UI::Color c1 = defaults[1];
        if (scheme->Equals(L"custom")) {
            auto k = ref new Platform::String(kCustomKeys[1]);
            if (ls->HasKey(k)) c1 = BgHexToColor(safe_cast<Platform::String^>(ls->Lookup(k)), c1);
        }
        Color1->SelectColor(c1, false);
        Color1->ColorChanged += ref new SwatchColorChangedHandler(this, &OrbsSettingsControl::Color1_ColorChanged);
    }

    m_initialized = true;
}

void OrbsSettingsControl::UpdateCustomPanelVisibility()
{
    if (CustomPanel == nullptr || SchemeSelector == nullptr) return;
    auto item = dynamic_cast<ComboBoxItem^>(SchemeSelector->SelectedItem);
    bool isCustom = (item != nullptr && item->DataContext != nullptr &&
                     item->DataContext->ToString()->Equals(L"custom"));
    CustomPanel->Visibility = isCustom ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
}

void OrbsSettingsControl::SchemeSelector_SelectionChanged(Platform::Object^, SelectionChangedEventArgs^)
{
    if (!m_initialized) return;
    auto item = dynamic_cast<ComboBoxItem^>(SchemeSelector->SelectedItem);
    if (item == nullptr) return;
    auto key = item->DataContext->ToString();

    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert("orbs.scheme", key);

    if (!key->Equals(L"custom")) {
        Windows::UI::Color* defaults = kElectricUI;
        if      (key->Equals(L"aurora")) defaults = kAuroraUI;
        else if (key->Equals(L"solar"))  defaults = kSolarUI;
        else if (key->Equals(L"nebula")) defaults = kNebulaUI;
        else if (key->Equals(L"rose"))   defaults = kRoseUI;
        if (Color0 != nullptr) Color0->SelectColor(defaults[0], false);
        if (Color1 != nullptr) Color1->SelectColor(defaults[1], false);
    }

    UpdateCustomPanelVisibility();
    try { if (m_host != nullptr) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void OrbsSettingsControl::Color0_ColorChanged(Platform::Object^, Windows::UI::Color color, bool)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert(ref new Platform::String(kCustomKeys[0]), BgColorToHex(color));
    try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void OrbsSettingsControl::Color1_ColorChanged(Platform::Object^, Windows::UI::Color color, bool)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert(ref new Platform::String(kCustomKeys[1]), BgColorToHex(color));
    try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void OrbsSettingsControl::ResetButton_Click(Platform::Object^, RoutedEventArgs^)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Remove("orbs.scheme");
    ls->Remove(ref new Platform::String(kCustomKeys[0]));
    ls->Remove(ref new Platform::String(kCustomKeys[1]));

    SchemeSelector->SelectedIndex = 0;
    if (Color0 != nullptr) Color0->SelectColor(kElectricUI[0], false);
    if (Color1 != nullptr) Color1->SelectColor(kElectricUI[1], false);
    CustomPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;

    try { if (m_host != nullptr) m_host->ReloadBackgroundColors(); } catch (...) {}
}
