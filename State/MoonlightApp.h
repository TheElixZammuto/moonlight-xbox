#pragma once
#include <windows.ui.xaml.media.imaging.h>
namespace moonlight_xbox_dx {

    [Windows::UI::Xaml::Data::Bindable]
    public ref class MoonlightApp sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        Platform::String^ name;
        Platform::String^ imagePath = "ms-appx:///Assets/gamepad.svg";
        int id;
        bool currentlyRunning;
        bool isGridLayout = false;
        bool isSelected = false;
        bool isFavorite = false;
        Windows::UI::Xaml::Media::Imaging::BitmapImage^ image;

        Windows::UI::Xaml::Media::Imaging::BitmapImage^ blurredImage;

        Windows::UI::Xaml::Media::Imaging::BitmapImage^ glowImage;
    public:
        //Thanks to https://phsucharee.wordpress.com/2013/06/19/data-binding-and-ccx-inotifypropertychanged/
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;

        void OnPropertyChanged(Platform::String^ propertyName);
        property Platform::String^ Name
        {
            Platform::String^ get() { return this->name; }
            void set(Platform::String^ value) {
                this->name = value;
                OnPropertyChanged("Name");
            }
        }

        property Platform::String^ ImagePath
        {
            Platform::String^ get() { 
                return imagePath;
            }
            void set(Platform::String^ path) {
                this->imagePath = path;
                OnPropertyChanged("ImagePath");

                if (path != nullptr && this->image == nullptr) {
				try {
                    auto uri = ref new Windows::Foundation::Uri(path);
                    Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
                        Windows::UI::Core::CoreDispatcherPriority::High,
                        ref new Windows::UI::Core::DispatchedHandler([this, uri]() {
                            try {
                                auto bitmap = ref new Windows::UI::Xaml::Media::Imaging::BitmapImage();
                                bitmap->UriSource = uri;
                                bitmap->DecodePixelWidth = 1024;
                                bitmap->CreateOptions = Windows::UI::Xaml::Media::Imaging::BitmapCreateOptions::IgnoreImageCache;
                                this->image = bitmap;
                                OnPropertyChanged("Image");
                            } catch(...) {}
                        })
                    );
            } catch (...) {
            }
                }
            }
        }
        
        property int Id
        {
            int get() { return this->id; }
            void set(int value) {
                this->id = value;
                OnPropertyChanged("Id");
            }
        }

        property bool CurrentlyRunning
        {
            bool get() { return this->currentlyRunning; }
            void set(bool value) {
                this->currentlyRunning = value;
                OnPropertyChanged("CurrentlyRunning");
            }
        }

        property bool IsGridLayout
        {
            bool get() { return this->isGridLayout; }
            void set(bool value) {
                if (this->isGridLayout == value) return;
                this->isGridLayout = value;
                OnPropertyChanged("IsGridLayout");
            }
        }

        property bool IsSelected
        {
            bool get() { return this->isSelected; }
            void set(bool value) {
                if (this->isSelected == value) return;
                this->isSelected = value;
                OnPropertyChanged("IsSelected");
            }
        }

        property bool IsFavorite
        {
            bool get() { return this->isFavorite; }
            void set(bool value) {
                if (this->isFavorite == value) return;
                this->isFavorite = value;
                OnPropertyChanged("IsFavorite");
            }
        }

        property Windows::UI::Xaml::Media::Imaging::BitmapImage^ Image
        {
            Windows::UI::Xaml::Media::Imaging::BitmapImage^ get() { return this->image; }
            void set(Windows::UI::Xaml::Media::Imaging::BitmapImage^ value) {
                if (this->image == value) return;
                this->image = value;
                try { OnPropertyChanged("Image"); } catch(...) {}
            }
        }

        property Windows::UI::Xaml::Media::Imaging::BitmapImage^ BlurredImage
        {
            Windows::UI::Xaml::Media::Imaging::BitmapImage^ get() { return this->blurredImage; }
            void set(Windows::UI::Xaml::Media::Imaging::BitmapImage^ value) {
                if (this->blurredImage == value) return;
                this->blurredImage = value;
                try { OnPropertyChanged("BlurredImage"); } catch(...) {}
            }
        }

        property Windows::UI::Xaml::Media::Imaging::BitmapImage^ GlowImage
        {
            Windows::UI::Xaml::Media::Imaging::BitmapImage^ get() { return this->glowImage; }
            void set(Windows::UI::Xaml::Media::Imaging::BitmapImage^ value) {
                if (this->glowImage == value) return;
                this->glowImage = value;
                try { OnPropertyChanged("GlowImage"); } catch(...) {}
            }
        }

    };
}