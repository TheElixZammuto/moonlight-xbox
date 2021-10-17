#pragma once
namespace moonlight_xbox_dx {
    
    [Windows::UI::Xaml::Data::Bindable]
    public ref class MoonlightHost sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        Platform::String^ instanceId;
        Platform::String^ lastHostname;
        bool paired;
        bool loading = false;
        int currentlyRunningAppId;
    public:
        //Thanks to https://phsucharee.wordpress.com/2013/06/19/data-binding-and-ccx-inotifypropertychanged/
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        
        void UpdateStats();
        void OnPropertyChanged(Platform::String^ propertyName);
        property Platform::String^ InstanceId
        {
            Platform::String^ get() { return this->instanceId; }
            void set(Platform::String^ value) {
                this->instanceId = instanceId;
                OnPropertyChanged("InstanceId");
            }
        }
        property Platform::String^ LastHostname
        {
            Platform::String^ get() { return this->lastHostname; }
            void set(Platform::String^ value) {
                this->lastHostname = value;
                OnPropertyChanged("LastHostname");
            }
        }

        property bool Paired
        {
            bool get() { return this->paired; }
            void set(bool value) {
                this->paired = value;
                OnPropertyChanged("Paired");
            }
        }

        property bool Loading
        {
            bool get() { return this->loading; }
            void set(bool value) {
                this->loading = value;
                OnPropertyChanged("Loading");
            }
        }

        property int CurrentlyRunningAppId
        {
            int get() { return this->currentlyRunningAppId; }
            void set(int value) {
                this->currentlyRunningAppId = value;
                OnPropertyChanged("CurrentlyRunningAppId");
            }
        } 
    };
}