#pragma once

#include "pch.h"

namespace moonlight_xbox_dx {

    [Windows::UI::Xaml::Data::Bindable]
    public ref class AppPageViewModel sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        Windows::UI::Xaml::Media::Imaging::BitmapImage^ currentBackgroundImage;
        Windows::UI::Xaml::Controls::Border^ pageBackgroundBorder;
        double backgroundTransitionDurationMs;
        double backgroundOverlayOpacity;

    public:
        AppPageViewModel();

        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        void OnPropertyChanged(Platform::String^ propertyName);

        property Windows::UI::Xaml::Media::Imaging::BitmapImage^ CurrentBackgroundImage
        {
            Windows::UI::Xaml::Media::Imaging::BitmapImage^ get() { return this->currentBackgroundImage; }
        }

        void TransitionToBlurredImage(Windows::UI::Xaml::Media::Imaging::BitmapImage^ newImage);

        void SetPageBackgroundBorder(Windows::UI::Xaml::Controls::Border^ border);
        void SetBackgroundTransitionSettings(double durationMs, double overlayOpacity);
    };
}
