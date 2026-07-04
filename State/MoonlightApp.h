#pragma once
namespace moonlight_xbox_dx {

    [Windows::UI::Xaml::Data::Bindable]
    public ref class MoonlightApp sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        Platform::String^ name;
        Platform::String^ imagePath = "ms-appx:///Assets/gamepad.svg";
        int id;
        bool currentlyRunning;
        bool isFavorite = false;
        int sortOrder = -1;
        bool isBeingMoved = false;
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

        // Box-art tile dimensions, sourced from the global TileSize setting.
        property double TileWidth
        {
            double get();
        }

        property double TileHeight
        {
            double get();
        }

        // Gap between adjacent tiles, sourced from the global TileGap setting.
        property Windows::UI::Xaml::Thickness TilePadding
        {
            Windows::UI::Xaml::Thickness get();
        }

        // Set by MoonlightHost when applying persisted favorite state to a fetched app list.
        property bool IsFavorite
        {
            bool get() { return this->isFavorite; }
            void set(bool value) {
                this->isFavorite = value;
                OnPropertyChanged("IsFavorite");
            }
        }

        // Position within the Favorites row (0-based). -1 when not a favorite.
        property int SortOrder
        {
            int get() { return this->sortOrder; }
            void set(int value) {
                this->sortOrder = value;
                OnPropertyChanged("SortOrder");
            }
        }

        // True while this tile is the one actively being repositioned in Move mode.
        property bool IsBeingMoved
        {
            bool get() { return this->isBeingMoved; }
            void set(bool value) {
                this->isBeingMoved = value;
                OnPropertyChanged("IsBeingMoved");
            }
        }
    };
}