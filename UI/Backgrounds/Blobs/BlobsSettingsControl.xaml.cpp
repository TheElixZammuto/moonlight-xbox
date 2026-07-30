#include "pch.h"
#include "BlobsSettingsControl.xaml.h"
#include "UI\Backgrounds\BackgroundSettingsHelpers.h"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

static Windows::UI::Color kCrimsonUI[] = { {170,210,10,55},{170,185,0,185},{170,100,0,200},{255,8,0,16} };
static Windows::UI::Color kOceanUI[]   = { {170,0,180,200},{170,0,100,220},{170,0,210,150},{255,0,6,20} };
static Windows::UI::Color kAuroraUI[]  = { {170,0,200,120},{170,60,190,255},{170,140,40,230},{255,2,4,10} };
static Windows::UI::Color kEmberUI[]   = { {170,255,70,0},{170,210,20,20},{170,255,140,20},{255,12,3,0} };
static Windows::UI::Color kNebulaUI[]  = { {170,170,40,230},{170,255,60,180},{170,40,90,255},{255,5,0,18} };

static Windows::UI::Color kColorSwatches[] = {
    {255,210, 10, 55}, {255,185,  0,185}, {255,100,  0,200},
    {255,  0,180,200}, {255,  0,210,150}, {255,  0,100,220},
    {255,  0,200,120}, {255,140, 40,230}, {255,255, 70,  0},
    {255,255,140, 20}, {255,255, 60,180}, {255,255,255,255},
};
static const int kColorSwatchCount = 12;

static Windows::UI::Color kBgSwatches[] = {
    {255,  8,  0, 16}, {255,  0,  6, 20}, {255,  2,  4, 10},
    {255, 12,  3,  0}, {255,  5,  0, 18}, {255,  0,  0,  0},
    {255,  5,  8,  0}, {255, 10,  0,  5},
};
static const int kBgSwatchCount = 8;

static const wchar_t* kCustomKeys[] = {
    L"blobs.custom.0", L"blobs.custom.1", L"blobs.custom.2", L"blobs.custom.3"
};

BlobsSettingsControl::BlobsSettingsControl()
{
    InitializeComponent();
}

void BlobsSettingsControl::Initialize(DynamicBackgroundHost^ host)
{
    m_host = host;

    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    Platform::String^ scheme = L"crimson";
    if (ls->HasKey("blobs.scheme"))
        scheme = safe_cast<Platform::String^>(ls->Lookup("blobs.scheme"));

    struct { const wchar_t* key; const wchar_t* label; } schemes[] = {
        { L"crimson", L"Crimson (Default)" },
        { L"ocean",   L"Ocean"             },
        { L"aurora",  L"Aurora"            },
        { L"ember",   L"Ember"             },
        { L"nebula",  L"Nebula"            },
        { L"custom",  L"Custom"            },
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

    Windows::UI::Color* defaults = kCrimsonUI;
    if      (scheme->Equals(L"ocean"))  defaults = kOceanUI;
    else if (scheme->Equals(L"aurora")) defaults = kAuroraUI;
    else if (scheme->Equals(L"ember"))  defaults = kEmberUI;
    else if (scheme->Equals(L"nebula")) defaults = kNebulaUI;

    SwatchPicker^ blobPickers[] = { Color0, Color1, Color2 };
    for (int i = 0; i < 3; ++i) {
        if (blobPickers[i] == nullptr) continue;
        blobPickers[i]->SetSwatches(kColorSwatches, kColorSwatchCount);
        Windows::UI::Color c = defaults[i];
        if (scheme->Equals(L"custom")) {
            auto k = ref new Platform::String(kCustomKeys[i]);
            if (ls->HasKey(k)) c = BgHexToColor(safe_cast<Platform::String^>(ls->Lookup(k)), c);
        }
        blobPickers[i]->SelectColor(c, false);
    }

    if (Color3 != nullptr) {
        Color3->SetSwatches(kBgSwatches, kBgSwatchCount);
        Windows::UI::Color c3 = defaults[3];
        if (scheme->Equals(L"custom")) {
            auto k = ref new Platform::String(kCustomKeys[3]);
            if (ls->HasKey(k)) c3 = BgHexToColor(safe_cast<Platform::String^>(ls->Lookup(k)), c3);
        }
        Color3->SelectColor(c3, false);
    }

    Color0->ColorChanged += ref new SwatchColorChangedHandler(this, &BlobsSettingsControl::Color0_ColorChanged);
    Color1->ColorChanged += ref new SwatchColorChangedHandler(this, &BlobsSettingsControl::Color1_ColorChanged);
    Color2->ColorChanged += ref new SwatchColorChangedHandler(this, &BlobsSettingsControl::Color2_ColorChanged);
    Color3->ColorChanged += ref new SwatchColorChangedHandler(this, &BlobsSettingsControl::Color3_ColorChanged);

    m_initialized = true;
}

void BlobsSettingsControl::UpdateCustomPanelVisibility()
{
    if (CustomPanel == nullptr || SchemeSelector == nullptr) return;
    auto item = dynamic_cast<ComboBoxItem^>(SchemeSelector->SelectedItem);
    bool isCustom = (item != nullptr && item->DataContext != nullptr &&
                     item->DataContext->ToString()->Equals(L"custom"));
    CustomPanel->Visibility = isCustom ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
}

void BlobsSettingsControl::SchemeSelector_SelectionChanged(Platform::Object^, SelectionChangedEventArgs^)
{
    if (!m_initialized) return;
    auto item = dynamic_cast<ComboBoxItem^>(SchemeSelector->SelectedItem);
    if (item == nullptr) return;
    auto key = item->DataContext->ToString();

    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert("blobs.scheme", key);

    if (!key->Equals(L"custom")) {
        Windows::UI::Color* defaults = kCrimsonUI;
        if      (key->Equals(L"ocean"))  defaults = kOceanUI;
        else if (key->Equals(L"aurora")) defaults = kAuroraUI;
        else if (key->Equals(L"ember"))  defaults = kEmberUI;
        else if (key->Equals(L"nebula")) defaults = kNebulaUI;
        SwatchPicker^ pickers[] = { Color0, Color1, Color2, Color3 };
        for (int i = 0; i < 4; ++i) {
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

void BlobsSettingsControl::Color0_ColorChanged(Platform::Object^, Windows::UI::Color color, bool) { SaveColor(0, color); try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {} }
void BlobsSettingsControl::Color1_ColorChanged(Platform::Object^, Windows::UI::Color color, bool) { SaveColor(1, color); try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {} }
void BlobsSettingsControl::Color2_ColorChanged(Platform::Object^, Windows::UI::Color color, bool) { SaveColor(2, color); try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {} }
void BlobsSettingsControl::Color3_ColorChanged(Platform::Object^, Windows::UI::Color color, bool) { SaveColor(3, color); try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {} }

void BlobsSettingsControl::ResetButton_Click(Platform::Object^, RoutedEventArgs^)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Remove("blobs.scheme");
    for (int i = 0; i < 4; ++i)
        ls->Remove(ref new Platform::String(kCustomKeys[i]));

    SchemeSelector->SelectedIndex = 0;
    SwatchPicker^ pickers[] = { Color0, Color1, Color2, Color3 };
    for (int i = 0; i < 4; ++i) {
        if (pickers[i] != nullptr) pickers[i]->SelectColor(kCrimsonUI[i], false);
    }
    CustomPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;

    try { if (m_host != nullptr) m_host->ReloadBackgroundColors(); } catch (...) {}
}
