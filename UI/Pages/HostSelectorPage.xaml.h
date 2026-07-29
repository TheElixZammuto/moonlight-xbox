
#pragma once

#include "UI\Pages\HostSelectorPage.g.h"
#include "UI\Backgrounds\DynamicBackgroundHost.xaml.h"
#include "UI\Modals\HostActionsDialog.xaml.h"
#include "UI\Modals\AddHostDialog.xaml.h"
#include "State\ApplicationState.h"
#include "UI\Controls\LunarPhaseControl.xaml.h"

#include <atomic>

using namespace Windows::UI::Core;
namespace moonlight_xbox_dx
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class HostSelectorPage sealed
	{
	public:
		HostSelectorPage();
		property ApplicationState^ State {
			ApplicationState^ get() {
				return this->state;
			}
		}
		void OnStateLoaded();
		void Connect(MoonlightHost^ host);
	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
		virtual void OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
	private:
		ApplicationState ^state;
		void NewHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void GridView_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
		void HostsGrid_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void HostsGrid_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
		void HostsGrid_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void CenterSelectedHost(bool immediate = false);
		bool EnsureCenteringPadding();
		void WaitForHostContainerThenCenter(bool immediate);
		void WaitForHostScrollViewerThenCenter(bool immediate);
		void PreRealizeAllHostContainers();
		void StartPairing(MoonlightHost^ host);
		void HostsGrid_RightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e);
		MoonlightHost^ currentHost;
		Windows::UI::Xaml::Controls::ScrollViewer^ m_hostsScrollViewer;
		bool m_adjustingCenterPadding = false;
		double m_lastCenterPadding = -1.0;
		bool m_paddingDirty = false;
		bool m_hostsPreRealized = false;
		void SettingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		std::atomic<bool> continueFetch;
		std::atomic<bool> m_isNavigatedAway;
		std::atomic<int> m_pollActiveCount;
		Windows::System::Threading::ThreadPoolTimer^ m_pollTimer;
		void OnKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		void ShowHostActions(MoonlightHost^ host);
		HostActionsDialog^ m_hostActionsDialog;
		void wakeHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void testConnectionButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void UpdateAllMoonPhases(bool animated, int attempts = 0);
		void FocusFirstHostItem(int attempts = 4);
		LunarPhaseControl^ FindLunarControl(Windows::UI::Xaml::DependencyObject^ root);
		void SubscribeMoonPhaseRefreshOnNextLayout(bool alsoFocusFirst);
		void PollTick(Windows::System::Threading::ThreadPoolTimer^ timer);
		void PollHostAfterWake(MoonlightHost^ host);
		void StartPollTimerIfNeeded();
		unsigned int m_hostTextAnimVersion = 0;
		Windows::Foundation::EventRegistrationToken m_hostsGrid_ccc_token;
		Windows::Foundation::EventRegistrationToken m_hostsContainerWait_token;
		Windows::Foundation::EventRegistrationToken m_hostsScrollViewerWait_token;
	};
}
