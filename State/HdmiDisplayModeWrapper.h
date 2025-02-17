#pragma once
#include "pch.h"
namespace moonlight_xbox_dx {

    [Windows::UI::Xaml::Data::Bindable]
        public ref class HdmiDisplayModeWrapper sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        Windows::Graphics::Display::Core::HdmiDisplayMode^ displayMode;
    public:
        //Thanks to https://phsucharee.wordpress.com/2013/06/19/data-binding-and-ccx-inotifypropertychanged/
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        void OnPropertyChanged(Platform::String^ propertyName);

        HdmiDisplayModeWrapper(Windows::Graphics::Display::Core::HdmiDisplayMode^ mode) {
            displayMode = mode;
        }

        property Windows::Graphics::Display::Core::HdmiDisplayMode^ DisplayMode
        {
            Windows::Graphics::Display::Core::HdmiDisplayMode^ get() { return this->displayMode; }
            void set(Windows::Graphics::Display::Core::HdmiDisplayMode^ value) {
                this->displayMode = value;
                OnPropertyChanged("DisplayMode");
            }
        }

        property Platform::String^ FormattedString
        {
            Platform::String^ get() {

                Platform::String^ colorSpace = this->DisplayMode->Is2086MetadataSupported ? "HDR" : "SDR";
                
                Platform::String^ dolbyVision = "";
                if (this->DisplayMode->IsDolbyVisionLowLatencySupported)
                {
                    dolbyVision = " Dolby Vision";
                }

                int bitDepth = this->DisplayMode->BitsPerPixel / 3;

                return this->DisplayMode->ResolutionWidthInRawPixels + "x" + this->DisplayMode->ResolutionHeightInRawPixels + "@" +
                        this->DisplayMode->RefreshRate + "hz (" + colorSpace + dolbyVision + " " + bitDepth + "bit " + this->DisplayMode->ColorSpace.ToString() + ")";
            }
        }
    };
}