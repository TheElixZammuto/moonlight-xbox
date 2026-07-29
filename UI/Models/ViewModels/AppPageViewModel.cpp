#include "pch.h"
#include "AppPageViewModel.h"
#include <cmath>

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace moonlight_xbox_dx;

AppPageViewModel::AppPageViewModel()
    : currentBackgroundImage(nullptr),
      pageBackgroundBorder(nullptr),
      backgroundTransitionDurationMs(250.0),
      backgroundOverlayOpacity(0.05)
{
}

void AppPageViewModel::OnPropertyChanged(Platform::String^ propertyName)
{
    PropertyChanged(this, ref new PropertyChangedEventArgs(propertyName));
}

void AppPageViewModel::SetPageBackgroundBorder(Border^ border)
{
    this->pageBackgroundBorder = border;
    this->currentBackgroundImage = nullptr;
}

void AppPageViewModel::SetBackgroundTransitionSettings(double durationMs, double overlayOpacity)
{
    if (durationMs > 0.0) {
        this->backgroundTransitionDurationMs = durationMs;
    }

    if (overlayOpacity < 0.0) overlayOpacity = 0.0;
    if (overlayOpacity > 1.0) overlayOpacity = 1.0;
    this->backgroundOverlayOpacity = overlayOpacity;
}

void AppPageViewModel::TransitionToBlurredImage(BitmapImage^ newImage)
{
    if (newImage == nullptr || this->pageBackgroundBorder == nullptr) return;
    if (newImage == this->currentBackgroundImage) return;
    this->currentBackgroundImage = newImage;

    try {
        auto durationTicks = (long long)std::llround(this->backgroundTransitionDurationMs * 10000.0);
        if (durationTicks <= 0) durationTicks = 2500000LL;

        auto fadeOutAnimation = ref new DoubleAnimation();
        fadeOutAnimation->To = 0.0;
        fadeOutAnimation->Duration = Duration(Windows::Foundation::TimeSpan{ durationTicks });

        Storyboard::SetTarget(fadeOutAnimation, this->pageBackgroundBorder);
        Storyboard::SetTargetProperty(fadeOutAnimation, "Opacity");

        auto fadeOutSB = ref new Storyboard();
        fadeOutSB->Children->Append(fadeOutAnimation);

        Platform::WeakReference weakThis(this);
        fadeOutSB->Completed += ref new EventHandler<Platform::Object^>(
            [weakThis, newImage](Platform::Object^ sender, Platform::Object^ e) {
            try {
                auto that = weakThis.Resolve<AppPageViewModel>();
                if (that == nullptr) return;

                auto localTicks = (long long)std::llround(that->backgroundTransitionDurationMs * 10000.0);
                if (localTicks <= 0) localTicks = 2500000LL;

                try {
                    auto brush = dynamic_cast<ImageBrush^>(that->pageBackgroundBorder->Background);
                    if (brush != nullptr) {
                        brush->ImageSource = newImage;
                    }
                } catch(...) {}

                that->OnPropertyChanged("CurrentBackgroundImage");

                auto fadeInAnimation = ref new DoubleAnimation();
                fadeInAnimation->From = 0.0;
                fadeInAnimation->To = that->backgroundOverlayOpacity;
                fadeInAnimation->Duration = Duration(Windows::Foundation::TimeSpan{ localTicks });

                Storyboard::SetTarget(fadeInAnimation, that->pageBackgroundBorder);
                Storyboard::SetTargetProperty(fadeInAnimation, "Opacity");

                auto fadeInSB = ref new Storyboard();
                fadeInSB->Children->Append(fadeInAnimation);
                fadeInSB->Begin();
            } catch(...) {}
        });

        fadeOutSB->Begin();
    } catch(...) {}
}
