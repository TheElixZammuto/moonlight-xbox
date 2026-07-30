#pragma once
#include "pch.h"

namespace moonlight_xbox_dx {

public enum class AppHostView : int {
    List = 0,
    Grid  = 1,
};

[Windows::UI::Xaml::Data::Bindable]
public ref class UIPersonalization sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
{
private:
    Platform::String^ background = "";
    AppHostView appView = AppHostView::List;
    Windows::UI::Color accentColor = Windows::UI::Color{ 255, 0, 120, 215 };
    bool useSystemAccent = true;

public:
    virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;

    void OnPropertyChanged(Platform::String^ propertyName)
    {
        PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
    }

    property Platform::String^ Background
    {
        Platform::String^ get() { return this->background; }
        void set(Platform::String^ value) {
            if (background == value) return;
            this->background = value;
            OnPropertyChanged("Background");
        }
    }

    property AppHostView AppView
    {
        AppHostView get() { return this->appView; }
        void set(AppHostView value) {
            if (appView == value) return;
            this->appView = value;
            OnPropertyChanged("AppView");
        }
    }

    property Windows::UI::Color AccentColor
    {
        Windows::UI::Color get() { return this->accentColor; }
        void set(Windows::UI::Color value) {
            this->accentColor = value;
            OnPropertyChanged("AccentColor");
        }
    }

    property bool UseSystemAccent
    {
        bool get() { return this->useSystemAccent; }
        void set(bool value) {
            if (useSystemAccent == value) return;
            this->useSystemAccent = value;
            OnPropertyChanged("UseSystemAccent");
        }
    }
};

}
