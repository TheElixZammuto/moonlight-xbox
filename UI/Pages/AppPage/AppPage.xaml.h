#pragma once

#include "UI\Pages\AppPage\AppPage.g.h"
#include "State\MoonlightApp.h"
#include "UI\Models\ViewModels\AppPageViewModel.h"
#include "UI\Utilities\XamlHelpers.h"
#include <atomic>
#include <functional>
#include <ppltasks.h>
#include <unordered_set>

namespace moonlight_xbox_dx
{

static constexpr float  kBackgroundSaturation         = 1.25f;
static constexpr int    kBgPanDurationSec             =    10;
static constexpr float  kBlurAmountBackground         =  2.0f;
static constexpr float  kBlurGlowPaddingDip           = 60.0f;

static constexpr double kEntranceStartScale           =  0.25;
static constexpr int    kEntranceDurationMs           =   500;
static constexpr int    kEntranceStaggerMs            =   100;
static constexpr int    kEntranceMaxStaggerItems      =   100;

    namespace Controls { ref class SlidingMenu; }
    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class AppPage sealed
    {
    private:
        moonlight_xbox_dx::Controls::SlidingMenu^ m_leftMenu = nullptr;
        moonlight_xbox_dx::Controls::SlidingMenu^ GetLeftMenu();
        AppPageViewModel^ m_viewModel = nullptr;
        MoonlightHost^ host;
        MoonlightApp^ currentApp;
        Platform::Collections::Vector<MoonlightApp^>^ m_filteredApps;
        bool ApplyAppFilter(Platform::String^ filter);
        Windows::Foundation::EventRegistrationToken m_apps_changed_token;
        void OnHostAppsChanged(Windows::Foundation::Collections::IObservableVector<MoonlightApp^>^ sender, Windows::Foundation::Collections::IVectorChangedEventArgs^ args);
        Windows::Foundation::EventRegistrationToken m_back_cookie;
        std::atomic<bool> continueAppFetch{ false };
        std::atomic<bool> wasConnected{ false };
        MoonlightApp^ m_selectedApp = nullptr;
    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
        void Connect(int app);
    public:
        AppPage();

        property Windows::Foundation::Collections::IObservableVector<MoonlightApp^>^ FilteredApps {
            Windows::Foundation::Collections::IObservableVector<MoonlightApp^>^ get() {
                if (m_filteredApps == nullptr) m_filteredApps = ref new Platform::Collections::Vector<MoonlightApp^>();
                return m_filteredApps;
            }
        }
        property MoonlightHost^ Host {
            MoonlightHost^ get() { return this->host; }
        }
        property AppPageViewModel^ ViewModel {
            AppPageViewModel^ get() {
                if (m_viewModel == nullptr) m_viewModel = ref new AppPageViewModel();
                return m_viewModel;
            }
        }
        void OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args);

    private:
        void AppsGrid_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
	    void BlurAppImage(MoonlightApp ^ selApp);
        void HandleBlurStreamReady(MoonlightApp^ selApp, Platform::WeakReference weakThis,
            Windows::Storage::Streams::IRandomAccessStream^ stream, bool isBackground);
        void AppsGrid_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void SearchBox_TextChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e);
        void AppsGrid_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
        void ApplySelectionVisuals(MoonlightApp^ app, bool animate, bool centerImmediate = false);
        void UpdateContainerSelectionState(MoonlightApp^ app, bool isSelected, bool animate);
        bool m_suppressSelectionVisuals = false;
        Windows::UI::Xaml::DispatcherTimer^ m_centerDebounceTimer = nullptr;
        Windows::Foundation::EventRegistrationToken m_centerDebounceTimer_token;
        void AppsGrid_RightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e);
        void AppsGrid_ContainerContentChanging(Windows::UI::Xaml::Controls::ListViewBase^ sender, Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs^ args);
        void PlayEntranceAnimation(Windows::UI::Xaml::Controls::ListViewItem^ container, int index);
        void ArmEntranceAnimations();
        std::unordered_set<int> m_listEntranceAnimatedIndices;
        std::unordered_set<int> m_gridEntranceAnimatedIndices;
        bool m_entranceAnimationsArmed = false;
        void closeAppButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void closeAndStartButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void ExecuteCloseAndStart();
        Platform::String^ FindRunningAppName();
        void CloseRunningAppOnHostAsync(
            const char* callerName,
            Windows::UI::Core::CoreDispatcherPriority uiPriority,
            std::function<void(AppPage^)> onClosedUIThread);
        void backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void UpdateItemHeights();
        void WireAppsGridEvents(
            Windows::UI::Xaml::Controls::ListView^ grid,
            Windows::Foundation::EventRegistrationToken& selectionToken,
            Windows::Foundation::EventRegistrationToken& itemClickToken,
            Windows::Foundation::EventRegistrationToken& rightTappedToken,
            Windows::Foundation::EventRegistrationToken& loadedToken,
            Windows::Foundation::EventRegistrationToken& cccToken);
        void UnwireAppsGridEvents(
            const char* gridName,
            Windows::UI::Xaml::Controls::ListView^ grid,
            Windows::Foundation::EventRegistrationToken selectionToken,
            Windows::Foundation::EventRegistrationToken itemClickToken,
            Windows::Foundation::EventRegistrationToken rightTappedToken,
            Windows::Foundation::EventRegistrationToken loadedToken,
            Windows::Foundation::EventRegistrationToken cccToken);

        Windows::UI::Xaml::Controls::ScrollViewer^ m_scrollViewer;
        Windows::UI::Xaml::Media::Animation::Storyboard^ m_bgPanStoryboard = nullptr;
        void StartBgPanAnimation();
        void StartBannerSlideInAnimation();

        bool m_isGridLayout = false;

        void LayoutToggleButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnGamepadKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);
        void HandleYButtonPress();
        void HandleXButtonPress();
        void FocusSelectedContainerOrGrid();
        Windows::UI::Xaml::Controls::ListView^ ActiveAppsGrid();
        void RevealActiveAppsGrid();
        bool m_appsGridRevealed = false;
        void CenterSelectedItem(bool immediate = false);
        void WaitForContainerThenCenter(bool immediate);
        void WaitForScrollViewerThenCenter(bool immediate);
	    void UpdateSelectedAppBox(MoonlightApp^ app, MoonlightApp^ prev, bool animate);

        Windows::Foundation::EventRegistrationToken m_keydown_cookie;

        Windows::Foundation::EventRegistrationToken m_appsListView_selection_token;
        Windows::Foundation::EventRegistrationToken m_appsListView_itemclick_token;
        Windows::Foundation::EventRegistrationToken m_appsListView_righttapped_token;
        Windows::Foundation::EventRegistrationToken m_appsListView_loaded_token;
        Windows::Foundation::EventRegistrationToken m_appsListView_ccc_token;
        Windows::Foundation::EventRegistrationToken m_appsGridView_selection_token;
        Windows::Foundation::EventRegistrationToken m_appsGridView_itemclick_token;
        Windows::Foundation::EventRegistrationToken m_appsGridView_righttapped_token;
        Windows::Foundation::EventRegistrationToken m_appsGridView_loaded_token;
        Windows::Foundation::EventRegistrationToken m_appsGridView_ccc_token;
        Windows::Foundation::EventRegistrationToken m_searchbox_gettingfocus_token;
        Windows::Foundation::EventRegistrationToken m_centerWaitContainer_token;
        Windows::UI::Xaml::Controls::ListViewBase^ m_centerWaitContainerTarget = nullptr;
        Windows::Foundation::EventRegistrationToken m_centerWaitScrollViewer_token;
        Windows::UI::Xaml::Controls::ListViewBase^ m_centerWaitScrollViewerTarget = nullptr;
        bool m_searchIsOpen = false;
        std::unordered_set<int> m_blurInProgressIds;

        bool m_initialFocusApplied = false;
        unsigned int m_appTextAnimVersion = 0;

        concurrency::task<Windows::Storage::Streams::IRandomAccessStream^> ApplyBlur(MoonlightApp^ app, float blurDip, float padDip = 0.0f);

        void FadeInBlurIfSelected(MoonlightApp^ app, Windows::UI::Xaml::Media::Imaging::BitmapImage^ img);
        void FadeInPollingIndicator();
        void FadeOutPollingIndicator();
        void PollAppRunningAndConnectivity();
        void ShowDisconnectedDialogAndNavigateBack();
    };
}
