#include "pch.h"
#include "ParticleSettingsControl.xaml.h"
#include "UI\Backgrounds\BackgroundSettingsHelpers.h"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

static Windows::UI::Color kChampagneScheme[] = { {255,210,165, 75}, {255, 50, 30,120} };
static Windows::UI::Color kEmberScheme[]     = { {255,255,120, 40}, {255, 15, 80, 90} };
static Windows::UI::Color kAuroraScheme[]    = { {255, 80,215,190}, {255, 90, 15,150} };
static Windows::UI::Color kNebulaScheme[]    = { {255,180, 90,230}, {255, 10, 20,100} };
static Windows::UI::Color kBlossomScheme[]   = { {255,235,110,150}, {255, 15, 85, 60} };

static Windows::UI::Color kSwatches[] = {
    {255,210,165, 75}, {255,230,120,140}, {255, 80,200,180},
    {255,160,100,220}, {255,160,200,255}, {255,255,100, 80},
    {255,100,200,100}, {255,255,180, 80}, {255, 80,120,255},
    {255,220,180,255}, {255,255,255,255}, {255,180,180,180},
};
static const int kSwatchCount = 12;

static const wchar_t* kKey0 = L"particles.custom.0";
static const wchar_t* kKey1 = L"particles.custom.1";

ParticleSettingsControl::ParticleSettingsControl()
{
    InitializeComponent();
}

void ParticleSettingsControl::Initialize(DynamicBackgroundHost^ host)
{
    m_host = host;

    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    Platform::String^ scheme = L"champagne";
    if (ls->HasKey("particles.scheme"))
        scheme = safe_cast<Platform::String^>(ls->Lookup("particles.scheme"));

    struct { const wchar_t* key; const wchar_t* label; } schemes[] = {
        { L"champagne", L"Champagne (Default)" },
        { L"ember",     L"Ember"               },
        { L"aurora",    L"Aurora"              },
        { L"nebula",    L"Nebula"              },
        { L"blossom",   L"Blossom"             },
        { L"custom",    L"Custom"              },
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

    Windows::UI::Color* defaults = kChampagneScheme;
    if      (scheme->Equals(L"ember"))   defaults = kEmberScheme;
    else if (scheme->Equals(L"aurora"))  defaults = kAuroraScheme;
    else if (scheme->Equals(L"nebula"))  defaults = kNebulaScheme;
    else if (scheme->Equals(L"blossom")) defaults = kBlossomScheme;

    Color0->SetSwatches(kSwatches, kSwatchCount);
    Color1->SetSwatches(kSwatches, kSwatchCount);

    Windows::UI::Color c0 = defaults[0], c1 = defaults[1];
    if (scheme->Equals(L"custom")) {
        auto k0 = ref new Platform::String(kKey0);
        auto k1 = ref new Platform::String(kKey1);
        if (ls->HasKey(k0)) c0 = BgHexToColor(safe_cast<Platform::String^>(ls->Lookup(k0)), c0);
        if (ls->HasKey(k1)) c1 = BgHexToColor(safe_cast<Platform::String^>(ls->Lookup(k1)), c1);
    }
    Color0->SelectColor(c0, false);
    Color1->SelectColor(c1, false);

    Color0->ColorChanged += ref new SwatchColorChangedHandler(this, &ParticleSettingsControl::Color0_ColorChanged);
    Color1->ColorChanged += ref new SwatchColorChangedHandler(this, &ParticleSettingsControl::Color1_ColorChanged);

    m_initialized = true;
}

void ParticleSettingsControl::UpdateCustomPanelVisibility()
{
    if (CustomPanel == nullptr || SchemeSelector == nullptr) return;
    auto item = dynamic_cast<ComboBoxItem^>(SchemeSelector->SelectedItem);
    bool isCustom = (item != nullptr && item->DataContext != nullptr &&
                     item->DataContext->ToString()->Equals(L"custom"));
    CustomPanel->Visibility = isCustom ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
}

void ParticleSettingsControl::SchemeSelector_SelectionChanged(Platform::Object^, SelectionChangedEventArgs^)
{
    if (!m_initialized) return;
    auto item = dynamic_cast<ComboBoxItem^>(SchemeSelector->SelectedItem);
    if (item == nullptr) return;
    auto key = item->DataContext->ToString();

    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert("particles.scheme", key);

    if (!key->Equals(L"custom")) {
        Windows::UI::Color* defaults = kChampagneScheme;
        if      (key->Equals(L"ember"))   defaults = kEmberScheme;
        else if (key->Equals(L"aurora"))  defaults = kAuroraScheme;
        else if (key->Equals(L"nebula"))  defaults = kNebulaScheme;
        else if (key->Equals(L"blossom")) defaults = kBlossomScheme;
        if (Color0 != nullptr) Color0->SelectColor(defaults[0], false);
        if (Color1 != nullptr) Color1->SelectColor(defaults[1], false);
    }

    UpdateCustomPanelVisibility();
    try { if (m_host != nullptr) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void ParticleSettingsControl::Color0_ColorChanged(Platform::Object^, Windows::UI::Color color, bool)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert(ref new Platform::String(kKey0), BgColorToHex(color));
    try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void ParticleSettingsControl::Color1_ColorChanged(Platform::Object^, Windows::UI::Color color, bool)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert(ref new Platform::String(kKey1), BgColorToHex(color));
    try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void ParticleSettingsControl::ResetButton_Click(Platform::Object^, RoutedEventArgs^)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Remove("particles.scheme");
    ls->Remove(ref new Platform::String(kKey0));
    ls->Remove(ref new Platform::String(kKey1));

    SchemeSelector->SelectedIndex = 0;
    if (Color0 != nullptr) Color0->SelectColor(kChampagneScheme[0], false);
    if (Color1 != nullptr) Color1->SelectColor(kChampagneScheme[1], false);
    CustomPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;

    try { if (m_host != nullptr) m_host->ReloadBackgroundColors(); } catch (...) {}
}
