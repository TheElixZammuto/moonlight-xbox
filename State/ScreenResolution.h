#pragma once
#include "pch.h"
namespace moonlight_xbox_dx {

    [Windows::UI::Xaml::Data::Bindable]
    public ref class ScreenResolution sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        int width;
        int height;
    public:
        //Thanks to https://phsucharee.wordpress.com/2013/06/19/data-binding-and-ccx-inotifypropertychanged/
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        void OnPropertyChanged(Platform::String^ propertyName);
        
        ScreenResolution(int width, int height) {
            Width = width;
            Height = height;
        }
        property int Width
        {
            int get() { return this->width; }
            void set(int value) {
                this->width = value;
                OnPropertyChanged("Width");
            }
        }

        property int Height
        {
            int get() { return this->height; }
            void set(int value) {
                this->height = value;
                OnPropertyChanged("Height");
            }
        }
    };
}