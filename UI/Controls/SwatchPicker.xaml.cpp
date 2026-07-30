#include "pch.h"
#include "UI\Controls\SwatchPicker.xaml.h"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::ViewManagement;
using namespace Windows::Foundation::Collections;

static const Windows::UI::Color kDefaultSwatches[] = {
    { 255, 255, 185,   0 },
    { 255, 247,  99,  12 },
    { 255, 202,  80,  16 },
    { 255, 232,  17,  35 },
    { 255, 234,   0,  94 },
    { 255, 195,   0,  82 },
    { 255, 135, 100, 184 },
    { 255, 116,  77, 169 },
    { 255, 107, 105, 214 },
    { 255,   0, 120, 215 },
    { 255,   0, 153, 188 },
    { 255,   3, 131, 135 },
    { 255,  73, 130,   5 },
    { 255,  16, 137,  62 },
    { 255, 105, 121, 126 },
};
static const int kDefaultSwatchCount = sizeof(kDefaultSwatches) / sizeof(kDefaultSwatches[0]);

SwatchPicker::SwatchPicker()
{
    InitializeComponent();
    m_entries = ref new Vector<SwatchEntry^>();
    SwatchGrid->ItemsSource = m_entries;
    BuildDefaultSwatches();
}

Windows::UI::Color SwatchPicker::GetSystemAccentColor()
{
    auto uiSettings = ref new UISettings();
    return uiSettings->GetColorValue(UIColorType::Accent);
}

void SwatchPicker::BuildDefaultSwatches()
{
    m_entries->Clear();
    m_selectedEntry = nullptr;
    m_customEntry   = nullptr;

    auto sysColor = GetSystemAccentColor();
    auto sysEntry = ref new SwatchEntry(sysColor, true, false);
    m_entries->Append(sysEntry);

    for (int i = 0; i < kDefaultSwatchCount; ++i)
        m_entries->Append(ref new SwatchEntry(kDefaultSwatches[i], false, false));

    m_customEntry = ref new SwatchEntry(Windows::UI::Color{ 255, 108, 108, 108 }, false, true);
    m_entries->Append(m_customEntry);

    SelectEntry(sysEntry, false);
}

void SwatchPicker::SelectEntry(SwatchEntry^ entry, bool fireEvent)
{
    if (m_selectedEntry != nullptr)
        m_selectedEntry->IsSelected = false;

    m_selectedEntry = entry;

    if (entry != nullptr) {
        entry->IsSelected = true;
        m_useSystemAccent = entry->IsSystemAccent;
        m_selectedColor   = entry->DisplayColor;
    }

    if (fireEvent && entry != nullptr)
        ColorChanged(this, m_selectedColor, m_useSystemAccent);
}

void SwatchPicker::SwatchGrid_ItemClick(Platform::Object^ sender, ItemClickEventArgs^ e)
{
    auto entry = dynamic_cast<SwatchEntry^>(e->ClickedItem);
    if (entry == nullptr) return;

    if (entry->IsCustom)
        OpenCustomColorPicker(entry);
    else
        SelectEntry(entry, true);
}

static void RGBtoHSL(uint8_t r, uint8_t g, uint8_t b, double* outH, double* outS, double* outL)
{
    double rd = r / 255.0, gd = g / 255.0, bd = b / 255.0;
    double cmax = rd > gd ? (rd > bd ? rd : bd) : (gd > bd ? gd : bd);
    double cmin = rd < gd ? (rd < bd ? rd : bd) : (gd < bd ? gd : bd);
    double delta = cmax - cmin;
    double l = (cmax + cmin) * 0.5;
    double h = 0.0, s = 0.0;

    if (delta > 0.0001) {
        double denom = 1.0 - fabs(2.0 * l - 1.0);
        s = (denom > 0.0001) ? delta / denom : 0.0;

        if      (fabs(cmax - rd) < 0.0001) h = 60.0 * fmod((gd - bd) / delta, 6.0);
        else if (fabs(cmax - gd) < 0.0001) h = 60.0 * ((bd - rd) / delta + 2.0);
        else                               h = 60.0 * ((rd - gd) / delta + 4.0);

        if (h < 0.0) h += 360.0;
    }

    *outH = h;
    *outS = s * 100.0;
    *outL = l * 100.0;
}

static Windows::UI::Color HSLtoRGB(double h, double s, double l)
{
    h = fmod(h, 360.0);
    if (h < 0.0) h += 360.0;

    double c = (1.0 - fabs(2.0 * l - 1.0)) * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = l - c * 0.5;
    double rd = 0, gd = 0, bd = 0;

    if      (h <  60) { rd = c; gd = x; bd = 0; }
    else if (h < 120) { rd = x; gd = c; bd = 0; }
    else if (h < 180) { rd = 0; gd = c; bd = x; }
    else if (h < 240) { rd = 0; gd = x; bd = c; }
    else if (h < 300) { rd = x; gd = 0; bd = c; }
    else              { rd = c; gd = 0; bd = x; }

    Windows::UI::Color nc;
    nc.A = 255;
    nc.R = (uint8_t)((rd + m) * 255.0 + 0.5);
    nc.G = (uint8_t)((gd + m) * 255.0 + 0.5);
    nc.B = (uint8_t)((bd + m) * 255.0 + 0.5);
    return nc;
}

void SwatchPicker::OpenCustomColorPicker(SwatchEntry^ entry)
{
    auto c = entry->DisplayColor;
    double hVal, sVal, lVal;
    RGBtoHSL(c.R, c.G, c.B, &hVal, &sVal, &lVal);

    auto previewBrush = ref new Windows::UI::Xaml::Media::SolidColorBrush(c);
    auto preview = ref new Windows::UI::Xaml::Shapes::Rectangle();
    preview->Height = 52;
    preview->RadiusX = 8;
    preview->RadiusY = 8;
    preview->Margin = Thickness(0, 0, 0, 16);
    preview->Fill = previewBrush;

    auto makeSlider = [](Platform::String^ label, double val, double maxVal) -> Slider^ {
        auto sl = ref new Slider();
        sl->Header = label;
        sl->Minimum = 0;
        sl->Maximum = maxVal;
        sl->Value = val;
        sl->StepFrequency = 1;
        sl->SnapsTo = Windows::UI::Xaml::Controls::Primitives::SliderSnapsTo::StepValues;
        sl->Margin = Thickness(0, 0, 0, 8);
        return sl;
    };

    auto hSlider = makeSlider("Hue",        hVal, 360);
    auto sSlider = makeSlider("Saturation", sVal, 100);
    auto lSlider = makeSlider("Lightness",  lVal, 100);

    auto onChanged = ref new Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventHandler(
        [previewBrush, hSlider, sSlider, lSlider]
        (Platform::Object^, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^) {
            previewBrush->Color = HSLtoRGB(
                hSlider->Value,
                sSlider->Value / 100.0,
                lSlider->Value / 100.0);
        });
    hSlider->ValueChanged += onChanged;
    sSlider->ValueChanged += onChanged;
    lSlider->ValueChanged += onChanged;

    auto content = ref new StackPanel();
    content->Width = 320;
    content->Children->Append(preview);
    content->Children->Append(hSlider);
    content->Children->Append(sSlider);
    content->Children->Append(lSlider);

    auto dialog = ref new ContentDialog();
    dialog->Title = "Custom Color";
    dialog->Content = content;
    dialog->PrimaryButtonText = "Apply";
    dialog->CloseButtonText = "Cancel";
    dialog->XamlRoot = this->XamlRoot;

    auto that = this;
    dialog->PrimaryButtonClick += ref new Windows::Foundation::TypedEventHandler<ContentDialog^, ContentDialogButtonClickEventArgs^>(
        [that, entry, hSlider, sSlider, lSlider](ContentDialog^, ContentDialogButtonClickEventArgs^) {
            entry->UpdateColor(HSLtoRGB(
                hSlider->Value,
                sSlider->Value / 100.0,
                lSlider->Value / 100.0));
            that->SelectEntry(entry, true);
        });

    dialog->ShowAsync();
}

void SwatchPicker::SetSwatches(IVector<Windows::UI::Color>^ colors)
{
    m_entries->Clear();
    m_selectedEntry = nullptr;
    m_customEntry   = nullptr;

    auto sysColor = GetSystemAccentColor();
    auto sysEntry = ref new SwatchEntry(sysColor, true, false);
    m_entries->Append(sysEntry);

    for (auto c : colors)
        m_entries->Append(ref new SwatchEntry(c, false, false));

    m_customEntry = ref new SwatchEntry(Windows::UI::Color{ 255, 108, 108, 108 }, false, true);
    m_entries->Append(m_customEntry);

    SelectEntry(sysEntry, false);
}

void SwatchPicker::SetSwatches(Windows::UI::Color* colors, int count)
{
    m_entries->Clear();
    m_selectedEntry = nullptr;
    m_customEntry   = nullptr;

    auto sysColor = GetSystemAccentColor();
    auto sysEntry = ref new SwatchEntry(sysColor, true, false);
    m_entries->Append(sysEntry);

    for (int i = 0; i < count; ++i)
        m_entries->Append(ref new SwatchEntry(colors[i], false, false));

    m_customEntry = ref new SwatchEntry(Windows::UI::Color{ 255, 108, 108, 108 }, false, true);
    m_entries->Append(m_customEntry);

    SelectEntry(sysEntry, false);
}

void SwatchPicker::SelectColor(Windows::UI::Color color, bool useSystem)
{
    if (useSystem) {
        if (m_entries->Size > 0)
            SelectEntry(m_entries->GetAt(0), false);
        return;
    }

    for (unsigned int i = 0; i < m_entries->Size; ++i) {
        auto entry = m_entries->GetAt(i);
        if (entry->IsSystemAccent || entry->IsCustom) continue;
        if (entry->DisplayColor.R == color.R
            && entry->DisplayColor.G == color.G
            && entry->DisplayColor.B == color.B)
        {
            SelectEntry(entry, false);
            return;
        }
    }

    if (m_customEntry != nullptr) {
        m_customEntry->UpdateColor(color);
        SelectEntry(m_customEntry, false);
    }
}
