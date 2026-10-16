//
// HostSelectorPage.xaml.h
// Declaration of the HostSelectorPage class
//

#pragma once

// clang-format off
#include "pch.h"
// clang-format on
#include "Common/DeviceWatcherHelper.h"
#include "Pages/HostSelectorPage.g.h"
#include "State/ApplicationState.h"

#include <atomic>

using namespace Windows::UI::Core;
namespace moonlight_xbox_dx {

	// Coalesce repeated watcher notifications without making one endpoint wait for another.
	class DiscoveryProbeState {
	public:
		bool TryBegin(const std::string& endpoint, int64_t nowMs) {
			std::lock_guard<std::mutex> lock(mutex);
			auto& entry = endpoints[endpoint];
			if (entry.inFlight || nowMs < entry.retryAtMs) return false;
			entry.inFlight = true;
			return true;
		}

		void Complete(const std::string& endpoint, bool succeeded, int64_t nowMs) {
			std::lock_guard<std::mutex> lock(mutex);
			auto& entry = endpoints[endpoint];
			entry.inFlight = false;
			entry.retryAtMs = nowMs + (succeeded ? 30000 : 5000);
		}

	private:
		struct Entry {
			bool inFlight = false;
			int64_t retryAtMs = 0;
		};
		std::mutex mutex;
		std::map<std::string, Entry> endpoints;
	};

	[Windows::Foundation::Metadata::WebHostHidden] public ref class HostSelectorPage sealed {
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
		ApplicationState^ state;
		void NewHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnNewHostDialogPrimaryClick(Windows::UI::Xaml::Controls::ContentDialog^ sender, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs^ args);
		Windows::UI::Xaml::Controls::TextBox^ dialogHostnameTextBox;
		void GridView_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
		void StartPairing(MoonlightHost^ host);
		void removeHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void HostsGrid_RightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e);
		MoonlightHost^ currentHost;
		void hostSettingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void hostDetailsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void SettingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		void wakeHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void testConnectionButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ShowHostActions(Windows::UI::Xaml::FrameworkElement^ anchor, MoonlightHost^ host);

		void OnDeviceAdded(Windows::Devices::Enumeration::DeviceWatcher^ sender, Windows::Devices::Enumeration::DeviceInformation^ info);
		void OnDeviceUpdated(Windows::Devices::Enumeration::DeviceWatcher^ sender, Windows::Devices::Enumeration::DeviceInformationUpdate^ info);
		void OnDeviceRemoved(Windows::Devices::Enumeration::DeviceWatcher^ sender, Windows::Devices::Enumeration::DeviceInformationUpdate^ info);
		void OnDeviceEnumerationCompleted(Windows::Devices::Enumeration::DeviceWatcher^ sender, Platform::Object^ args);
		void OnDeviceWatcherStopped(Windows::Devices::Enumeration::DeviceWatcher^ sender, Platform::Object^ args);
		void TryAddDiscoveredHost(Windows::Devices::Enumeration::DeviceInformation^ info);
		Windows::Foundation::Collections::IMap<Platform::String^, Windows::Devices::Enumeration::DeviceInformation^>^ discoveredDevices;
		std::shared_ptr<DiscoveryProbeState> discoveryProbes = std::make_shared<DiscoveryProbeState>();
		DeviceWatcherHelper^ deviceWatcherHelper;
	};
} // namespace moonlight_xbox_dx
