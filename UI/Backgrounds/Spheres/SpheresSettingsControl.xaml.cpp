#include "pch.h"
#include "SpheresSettingsControl.xaml.h"
#include "UI\Backgrounds\BackgroundSettingsHelpers.h"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

static Windows::UI::Color kClassicUI[] = { {255,255,255,255}, {255,  0, 68,170} };
static Windows::UI::Color kNeonUI[]    = { {255,  0,238,255}, {255,  0, 32,128} };
static Windows::UI::Color kSunsetUI[]  = { {255,255,112, 64}, {255,139, 21,  0} };
static Windows::UI::Color kOceanUI[]   = { {255,  0,200,176}, {255,  0, 64, 96} };
static Windows::UI::Color kNebulaUI[]  = { {255,192, 96,255}, {255, 64,  0,128} };

static Windows::UI::Color kShapeSwatches[] = {
    {255,255,255,255}, {255,  0,238,255}, {255,255,112, 64}, {255,  0,200,176},
    {255,192, 96,255}, {255,255,224,  0}, {255,  0,255,128}, {255,255, 80,120},
    {255,100,200,255}, {255,220,180,255}, {255,255,200,100}, {255,180,255,180},
};
static const int kShapeSwatchCount = 12;

static Windows::UI::Color kBgSwatches[] = {
    {255,  0, 68,170}, {255,  0, 32,128}, {255,139, 21,  0}, {255,  0, 64, 96},
    {255, 64,  0,128}, {255,  0,100, 60}, {255,120, 60,  0}, {255, 80, 20, 80},
};
static const int kBgSwatchCount = 8;

static const wchar_t* kCustomKeys[] = { L"spheres.custom.0", L"spheres.custom.1" };

SpheresSettingsControl::SpheresSettingsControl()
{
    InitializeComponent();
}

void SpheresSettingsControl::Initialize(DynamicBackgroundHost^ host)
{
    m_host = host;

    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    Platform::String^ scheme = L"classic";
    if (ls->HasKey("spheres.scheme"))
        scheme = safe_cast<Platform::String^>(ls->Lookup("spheres.scheme"));

    struct { const wchar_t* key; const wchar_t* label; } schemes[] = {
        { L"classic", L"Classic (Default)" },
        { L"neon",    L"Neon"              },
        { L"sunset",  L"Sunset"            },
        { L"ocean",   L"Ocean"             },
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

    struct { const wchar_t* key; const wchar_t* label; } shapes[] = {
        { L"circles",   L"Circles (Default)" },
        { L"squares",   L"Squares"           },
        { L"triangles", L"Triangles"         },
        { L"all",       L"All Shapes"        },
    };

    Platform::String^ savedShape = L"circles";
    if (ls->HasKey("spheres.shape"))
        savedShape = safe_cast<Platform::String^>(ls->Lookup("spheres.shape"));

    int shapeIdx = 0;
    for (int i = 0; i < 4; ++i) {
        auto item = ref new ComboBoxItem();
        item->Content    = ref new Platform::String(shapes[i].label);
        item->DataContext = ref new Platform::String(shapes[i].key);
        ShapeSelector->Items->Append(item);
        if (savedShape->Equals(ref new Platform::String(shapes[i].key))) shapeIdx = i;
    }
    ShapeSelector->SelectedIndex = shapeIdx;

    Windows::UI::Color* defaults = kClassicUI;
    if      (scheme->Equals(L"neon"))   defaults = kNeonUI;
    else if (scheme->Equals(L"sunset")) defaults = kSunsetUI;
    else if (scheme->Equals(L"ocean"))  defaults = kOceanUI;
    else if (scheme->Equals(L"nebula")) defaults = kNebulaUI;

    if (Color0 != nullptr) {
        Color0->SetSwatches(kShapeSwatches, kShapeSwatchCount);
        Windows::UI::Color c0 = defaults[0];
        if (scheme->Equals(L"custom")) {
            auto k = ref new Platform::String(kCustomKeys[0]);
            if (ls->HasKey(k)) c0 = BgHexToColor(safe_cast<Platform::String^>(ls->Lookup(k)), c0);
        }
        Color0->SelectColor(c0, false);
        Color0->ColorChanged += ref new SwatchColorChangedHandler(this, &SpheresSettingsControl::Color0_ColorChanged);
    }

    if (Color1 != nullptr) {
        Color1->SetSwatches(kBgSwatches, kBgSwatchCount);
        Windows::UI::Color c1 = defaults[1];
        if (scheme->Equals(L"custom")) {
            auto k = ref new Platform::String(kCustomKeys[1]);
            if (ls->HasKey(k)) c1 = BgHexToColor(safe_cast<Platform::String^>(ls->Lookup(k)), c1);
        }
        Color1->SelectColor(c1, false);
        Color1->ColorChanged += ref new SwatchColorChangedHandler(this, &SpheresSettingsControl::Color1_ColorChanged);
    }

    m_initialized = true;
}

void SpheresSettingsControl::UpdateCustomPanelVisibility()
{
    if (CustomPanel == nullptr || SchemeSelector == nullptr) return;
    auto item = dynamic_cast<ComboBoxItem^>(SchemeSelector->SelectedItem);
    bool isCustom = (item != nullptr && item->DataContext != nullptr &&
                     item->DataContext->ToString()->Equals(L"custom"));
    CustomPanel->Visibility = isCustom ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
}

void SpheresSettingsControl::SchemeSelector_SelectionChanged(Platform::Object^, SelectionChangedEventArgs^)
{
    if (!m_initialized) return;
    auto item = dynamic_cast<ComboBoxItem^>(SchemeSelector->SelectedItem);
    if (item == nullptr) return;
    auto key = item->DataContext->ToString();

    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert("spheres.scheme", key);

    if (!key->Equals(L"custom")) {
        Windows::UI::Color* defaults = kClassicUI;
        if      (key->Equals(L"neon"))   defaults = kNeonUI;
        else if (key->Equals(L"sunset")) defaults = kSunsetUI;
        else if (key->Equals(L"ocean"))  defaults = kOceanUI;
        else if (key->Equals(L"nebula")) defaults = kNebulaUI;
        if (Color0 != nullptr) Color0->SelectColor(defaults[0], false);
        if (Color1 != nullptr) Color1->SelectColor(defaults[1], false);
    }

    UpdateCustomPanelVisibility();
    try { if (m_host != nullptr) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void SpheresSettingsControl::ShapeSelector_SelectionChanged(Platform::Object^, SelectionChangedEventArgs^)
{
    if (!m_initialized) return;
    auto item = dynamic_cast<ComboBoxItem^>(ShapeSelector->SelectedItem);
    if (item == nullptr) return;
    auto key = item->DataContext->ToString();

    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert("spheres.shape", key);

    try { if (m_host != nullptr) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void SpheresSettingsControl::Color0_ColorChanged(Platform::Object^, Windows::UI::Color color, bool)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert(ref new Platform::String(kCustomKeys[0]), BgColorToHex(color));
    try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void SpheresSettingsControl::Color1_ColorChanged(Platform::Object^, Windows::UI::Color color, bool)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Insert(ref new Platform::String(kCustomKeys[1]), BgColorToHex(color));
    try { if (m_host) m_host->ReloadBackgroundColors(); } catch (...) {}
}

void SpheresSettingsControl::ResetButton_Click(Platform::Object^, RoutedEventArgs^)
{
    auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    ls->Remove("spheres.scheme");
    ls->Remove("spheres.shape");
    ls->Remove(ref new Platform::String(kCustomKeys[0]));
    ls->Remove(ref new Platform::String(kCustomKeys[1]));

    SchemeSelector->SelectedIndex = 0;
    ShapeSelector->SelectedIndex  = 0;
    if (Color0 != nullptr) Color0->SelectColor(kClassicUI[0], false);
    if (Color1 != nullptr) Color1->SelectColor(kClassicUI[1], false);
    CustomPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;

    try { if (m_host != nullptr) m_host->ReloadBackgroundColors(); } catch (...) {}
}
