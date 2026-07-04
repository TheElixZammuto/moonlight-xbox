#pragma once
#include "pch.h"
#include "State\MoonlightClient.h"
#include "State\ScreenResolution.h"
#include <map>
namespace moonlight_xbox_dx {

    [Windows::UI::Xaml::Data::Bindable]
    public ref class MoonlightHost sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    private:
        Platform::String^ instanceId;
        Platform::String^ lastHostname;
        Platform::String^ computerName;
        Platform::String^ serverAddress;
        Platform::String^ macAddress;
        bool paired;
        bool connected;
        bool loading = true;
        bool wolPolling = false;
        bool playAudioOnPC = false;
        MoonlightClient* client;
        int currentlyRunningAppId;
        int bitrate = 20000;
        ScreenResolution^ resolution;
        int fps = 60;
        int autostartID = -1;
        Platform::String^ videoCodec = "H.265";
        Platform::String^ audioConfig = "Stereo";
        Platform::String^ framePacing = "";
        bool enableHDR = false;
        bool enableSOPS = false;
        bool enableStats = false;
        bool enableGraphs = true;
        bool enableStatsLite = false;
        Platform::String^ statsColor = "Yellow"; // "Yellow", "White", "Green", "Cyan"
        Platform::String^ statsFont = "ModeSeven"; // "ModeSeven" (retro, default), "Consolas", "CascadiaMono"
        Windows::Foundation::Collections::IVector<MoonlightApp^>^ apps;
        std::map<int, int> favoriteOrders; // appId -> position within Favorites row; absent = not favorited
    public:
        //Thanks to https://phsucharee.wordpress.com/2013/06/19/data-binding-and-ccx-inotifypropertychanged/
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        void OnPropertyChanged(Platform::String^ propertyName);
        MoonlightHost(Platform::String ^host);

        void UpdateHostInfo(bool showLoading);
        int Connect();
        void Unpair();
        void UpdateApps();
    void UpdateAppRunningStates();

        // Favorites: appId -> position within the Favorites row.
        bool IsAppFavorite(int appId);
        void SetFavorite(int appId, bool favorite);
        // Moves appId one slot toward -1 (earlier) or +1 (later); no-op at ends or if not favorited.
        void MoveFavorite(int appId, int direction);
        // Re-stamps IsFavorite/SortOrder onto Apps and re-sorts (favorites first, then the rest).
        void ApplyFavoritesToApps();
        // JSON blob (e.g. {"12345":0,"67890":1}) used by ApplicationState for persistence.
        Platform::String^ SerializeFavorites();
        void DeserializeFavorites(Platform::String^ json);
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

        property Platform::String^ ServerAddress
        {
            Platform::String^ get() { return this->serverAddress; }
            void set(Platform::String^ value) {
                this->serverAddress = value;
                OnPropertyChanged("ServerAddress");
            }
        }

        property Platform::String^ MacAddress
        {
            Platform::String^ get() { return this->macAddress; }
            void set(Platform::String^ value) {
                this->macAddress = value;
                OnPropertyChanged("MacAddress");
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
                OnPropertyChanged("LoadingVisibility");
                OnPropertyChanged("Connected");
                OnPropertyChanged("NotConnected");
                OnPropertyChanged("NotPaired");
                OnPropertyChanged("Paired");
            }
        }

        property Windows::UI::Xaml::Visibility LoadingVisibility
        {
            Windows::UI::Xaml::Visibility get() { return this->loading ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed; }
        }

        property bool WolPolling
        {
            bool get() { return this->wolPolling; }
            void set(bool value) {
                this->wolPolling = value;
                OnPropertyChanged("WolPolling");
                OnPropertyChanged("WolPollingVisibility");
            }
        }

        property Windows::UI::Xaml::Visibility WolPollingVisibility
        {
            Windows::UI::Xaml::Visibility get() { return this->wolPolling ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed; }
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

        property Platform::String^ FramePacing
        {
            Platform::String^ get() { return this->framePacing; }
            void set(Platform::String^ value) {
                if (framePacing == value)return;
                this->framePacing = value;
                OnPropertyChanged("FramePacing");
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

        property bool EnableSOPS
        {
            bool get() { return this->enableSOPS; }
            void set(bool value) {
                this->enableSOPS = value;
                OnPropertyChanged("EnableSOPS");
            }
        }

        property bool EnableStats
        {
            bool get() { return this->enableStats; }
            void set(bool value) {
                this->enableStats = value;
                OnPropertyChanged("EnableStats");
            }
        }

        property bool EnableGraphs
        {
            bool get() { return this->enableGraphs; }
            void set(bool value) {
                this->enableGraphs = value;
                OnPropertyChanged("EnableGraphs");
            }
        }

        // Compact single-line overlay (Artemis "Lite mode"). Only meaningful when EnableStats is true.
        property bool EnableStatsLite
        {
            bool get() { return this->enableStatsLite; }
            void set(bool value) {
                this->enableStatsLite = value;
                OnPropertyChanged("EnableStatsLite");
            }
        }

        // Text color for the in-stream performance overlay (Full and Lite).
        property Platform::String^ StatsColor
        {
            Platform::String^ get() { return this->statsColor; }
            void set(Platform::String^ value) {
                this->statsColor = value;
                OnPropertyChanged("StatsColor");
            }
        }

        // Font family for the in-stream performance overlay (Full and Lite).
        property Platform::String^ StatsFont
        {
            Platform::String^ get() { return this->statsFont; }
            void set(Platform::String^ value) {
                this->statsFont = value;
                OnPropertyChanged("StatsFont");
            }
        }
    };
}
