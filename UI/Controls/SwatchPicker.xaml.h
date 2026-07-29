#pragma once
#include "UI\Controls\SwatchPicker.g.h"

namespace moonlight_xbox_dx {

[Windows::UI::Xaml::Data::Bindable]
public ref class SwatchEntry sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
{
private:
    bool m_isSelected     = false;
    bool m_isSystemAccent = false;
    bool m_isCustom       = false;
    Windows::UI::Color m_color = {};

public:
    virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;

    SwatchEntry(Windows::UI::Color color, bool isSystem, bool isCustom)
        : m_color(color), m_isSystemAccent(isSystem), m_isCustom(isCustom) {}

    void UpdateColor(Windows::UI::Color color) {
        m_color = color;
        PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs("DisplayColor"));
    }

    property Windows::UI::Color DisplayColor {
        Windows::UI::Color get() { return m_color; }
    }

    property bool IsSystemAccent {
        bool get() { return m_isSystemAccent; }
    }

    property bool IsCustom {
        bool get() { return m_isCustom; }
    }

    property bool IsSelected {
        bool get() { return m_isSelected; }
        void set(bool v) {
            if (m_isSelected == v) return;
            m_isSelected = v;
            PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs("IsSelected"));
            PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs("CheckVisibility"));
        }
    }

    property Windows::UI::Xaml::Visibility CheckVisibility {
        Windows::UI::Xaml::Visibility get() {
            return m_isSelected ? Windows::UI::Xaml::Visibility::Visible
                               : Windows::UI::Xaml::Visibility::Collapsed;
        }
    }

    property Windows::UI::Xaml::Visibility SystemIndicatorVisibility {
        Windows::UI::Xaml::Visibility get() {
            return m_isSystemAccent ? Windows::UI::Xaml::Visibility::Visible
                                   : Windows::UI::Xaml::Visibility::Collapsed;
        }
    }

    property Windows::UI::Xaml::Visibility CustomIndicatorVisibility {
        Windows::UI::Xaml::Visibility get() {
            return m_isCustom ? Windows::UI::Xaml::Visibility::Visible
                              : Windows::UI::Xaml::Visibility::Collapsed;
        }
    }
};

public delegate void SwatchColorChangedHandler(
    Platform::Object^ sender,
    Windows::UI::Color color,
    bool useSystemAccent);

[Windows::UI::Xaml::Data::Bindable]
public ref class SwatchPicker sealed
{
public:
    SwatchPicker();

    property Windows::UI::Color SelectedColor {
        Windows::UI::Color get() { return m_selectedColor; }
    }

    property bool UseSystemAccent {
        bool get() { return m_useSystemAccent; }
    }

    void SetSwatches(Windows::Foundation::Collections::IVector<Windows::UI::Color>^ colors);
    void SetSwatches(Windows::UI::Color* colors, int count);

    void SelectColor(Windows::UI::Color color, bool useSystem);

    event SwatchColorChangedHandler^ ColorChanged;

private:
    Windows::UI::Color m_selectedColor;
    bool m_useSystemAccent = true;
    Platform::Collections::Vector<SwatchEntry^>^ m_entries;
    SwatchEntry^ m_selectedEntry = nullptr;
    SwatchEntry^ m_customEntry   = nullptr;

    void SwatchGrid_ItemClick(Platform::Object^ sender,
                              Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
    void BuildDefaultSwatches();
    void SelectEntry(SwatchEntry^ entry, bool fireEvent);
    void OpenCustomColorPicker(SwatchEntry^ entry);
    Windows::UI::Color GetSystemAccentColor();
};

}
