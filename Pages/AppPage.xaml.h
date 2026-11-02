//
// AppPage.xaml.h
// Declaration of the AppPage class
//

#pragma once

#include "Pages\AppPage.g.h"
#include "State\MoonlightApp.h"
#include <atomic>

namespace moonlight_xbox_dx
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class AppPage sealed
	{
	private:
		MoonlightHost^ host;
		MoonlightApp^ currentApp;
		Windows::Foundation::EventRegistrationToken m_back_cookie;
		std::atomic<bool> continueAppFetch{ false };
		std::atomic<bool> wasConnected{ false };
		Windows::Foundation::Collections::IVector<MoonlightApp^>^ filteredApps;
		Windows::Foundation::EventRegistrationToken m_appsChangedToken;
		bool m_filterUpdatePending = false;
		void UpdateFilteredApps();
	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
		virtual void OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
 		void Connect(int app);
	public:
		AppPage();
		property MoonlightHost^ Host {
			MoonlightHost^ get() {
				return this->host;
			}
		}
		property Windows::Foundation::Collections::IVector<MoonlightApp^>^ FilteredApps {
			Windows::Foundation::Collections::IVector<MoonlightApp^>^ get() {
				if (this->filteredApps == nullptr) {
					this->filteredApps = ref new Platform::Collections::Vector<MoonlightApp^>();
				}
				return this->filteredApps;
			}
		}
		void OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args);
	private:
		void AppsGrid_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
		void AppsGrid_RightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e);
		void resumeAppButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void closeAppButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void closeAndStartButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void hideAppButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void showHiddenButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void sortAppsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void SearchBox_TextChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e);
		void Page_PreviewKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		void OnAppsChanged(Windows::Foundation::Collections::IObservableVector<MoonlightApp^>^ sender, Windows::Foundation::Collections::IVectorChangedEventArgs^ e);
		void ExecuteCloseAndStart();
		void backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void settingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
 	 };
 }
