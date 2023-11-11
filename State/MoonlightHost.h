#pragma once
#include "pch.h"
#include "State\MoonlightClient.h"
#include "State\ScreenResolution.h"
namespace moonlight_xbox_dx {
    
    [Windows::UI::Xaml::Data::Bindable]
    public ref class MoonlightHost sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        Platform::String^ instanceId;
        Platform::String^ lastHostname;
        Platform::String^ computerName;
        bool paired;
        bool connected;
        bool loading = true;
        bool playAudioOnPC = false;
        MoonlightClient* client;
        int currentlyRunningAppId;
        int bitrate = 8000;
        ScreenResolution^ resolution;
        int fps = 60;
        int autostartID = -1;
        Platform::String^ videoCodec = "H.264";
        Platform::String^ audioConfig = "Stereo";
        bool enableHDR = false;
        Windows::Foundation::Collections::IVector<MoonlightApp^>^ apps;
    public:
        //Thanks to https://phsucharee.wordpress.com/2013/06/19/data-binding-and-ccx-inotifypropertychanged/
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        void OnPropertyChanged(Platform::String^ propertyName);
        MoonlightHost(Platform::String ^host) {
            lastHostname = host;
            resolution = ref new ScreenResolution(1280, 720);
            loading = true;
        }
        void UpdateStats();
        int Connect();
        void Unpair();
        void UpdateApps();
        property Platform::String^ InstanceId
        {
            Platform::String^ get() { return this->instanceId; }
            void set(Platform::String^ value) {
                this->instanceId = value;
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

        property Platform::String^ ComputerName
        {
            Platform::String^ get() { return this->computerName; }
            void set(Platform::String^ value) {
                this->computerName = value;
                OnPropertyChanged("ComputerName");
            }
        }

        property bool Paired
        {
            bool get() { return this->paired; }
            void set(bool value) {
                this->paired = value;
                OnPropertyChanged("Paired");
                OnPropertyChanged("NotPaired");
                OnPropertyChanged("Connected");
                OnPropertyChanged("NotConnected");
            }
        }

        property bool Connected
        {
            bool get() { return this->connected; }
            void set(bool value) {
                this->connected = value;
                OnPropertyChanged("Connected");
                OnPropertyChanged("NotConnected");
                OnPropertyChanged("NotPaired");
            }
        }

        property bool NotConnected
        {
            bool get() { return !this->connected; }
        }

        property bool NotPaired
        {
            bool get() { return this->connected && !this->paired; }
        }

        property bool Loading
        {
            bool get() { return this->loading; }
            void set(bool value) {
                this->loading = value;
                OnPropertyChanged("Loading");
                OnPropertyChanged("Connected");
                OnPropertyChanged("NotConnected");
                OnPropertyChanged("NotPaired");
                OnPropertyChanged("Paired");
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

        property int AutostartID
        {
            int get() { return this->autostartID; }
            void set(int value) {
                this->autostartID = value;
                OnPropertyChanged("AutostartID");
            }
        }

        property ScreenResolution^ Resolution
        {
            ScreenResolution^ get() { return this->resolution; }
            void set(ScreenResolution^ value) {
                this->resolution = value;
                OnPropertyChanged("ScreenResolution");
            }
        }

        property int Bitrate
        {
            int get() { return this->bitrate; }
            void set(int value) {
                this->bitrate = value;
                OnPropertyChanged("Bitrate");
            }
        }

        property int FPS
        {
            int get() { return this->fps; }
            void set(int value) {
                if (fps == value)return;
                this->fps = value;
                OnPropertyChanged("FPS");
            }
        }

          property Platform::String^ VideoCodec
        {
            Platform::String^ get() { return this->videoCodec; }
            void set(Platform::String^ value) {
                if (videoCodec == value)return;
                this->videoCodec = value;
                OnPropertyChanged("VideoCodec");
            }
        }

        property Platform::String^ AudioConfig
        {
            Platform::String^ get() { return this->audioConfig; }
            void set(Platform::String^ value) {
                if (audioConfig == value)return;
                this->audioConfig = value;
                OnPropertyChanged("AudioConfig");
            }
        }

        property Windows::Foundation::Collections::IVector<MoonlightApp^>^ Apps {
            Windows::Foundation::Collections::IVector<MoonlightApp^>^ get() {
                if (this->apps == nullptr)
                {
                    this->apps = ref new Platform::Collections::Vector<MoonlightApp^>();
                }
                return this->apps;
            }
        }

        property bool PlayAudioOnPC
        {
            bool get() { return this->playAudioOnPC; }
            void set(bool value) {
                this->playAudioOnPC = value;
                OnPropertyChanged("PlayAudioOnPC");
            }
        }

        property bool EnableHDR
        {
            bool get() { return this->enableHDR; }
            void set(bool value) {
                this->enableHDR = value;
                OnPropertyChanged("EnableHDR");
            }
        }
    };
}