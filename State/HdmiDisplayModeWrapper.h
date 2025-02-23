#pragma once
#include "pch.h"
namespace moonlight_xbox_dx {

    [Windows::UI::Xaml::Data::Bindable]
    public ref class HdmiDisplayModeWrapper sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
        private:
            Windows::Graphics::Display::Core::HdmiDisplayMode^ hdmiDisplayMode;
        public:
            //Thanks to https://phsucharee.wordpress.com/2013/06/19/data-binding-and-ccx-inotifypropertychanged/
            virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
            void OnPropertyChanged(Platform::String^ propertyName);

            HdmiDisplayModeWrapper(Windows::Graphics::Display::Core::HdmiDisplayMode^ mode) {
                hdmiDisplayMode = mode;
            }

            property Windows::Graphics::Display::Core::HdmiDisplayMode^ HdmiDisplayMode
            {
                Windows::Graphics::Display::Core::HdmiDisplayMode^ get() { return this->hdmiDisplayMode; }
                void set(Windows::Graphics::Display::Core::HdmiDisplayMode^ value) {
                    this->hdmiDisplayMode = value;
                    OnPropertyChanged("HdmiDisplayMode");
                }
            }

            property bool IsHdr
            {
                bool get() {
                    return this->HdmiDisplayMode->Is2086MetadataSupported
                        && this->HdmiDisplayMode->BitsPerPixel > 24
                        && this->HdmiDisplayMode->ColorSpace == Windows::Graphics::Display::Core::HdmiDisplayColorSpace::BT2020;
                }
            }

            property bool IsSdr
            {
                bool get() {
                    return this->HdmiDisplayMode->BitsPerPixel <= 24
                        && this->HdmiDisplayMode->ColorSpace == Windows::Graphics::Display::Core::HdmiDisplayColorSpace::RgbLimited;
                }
            }
    };
}