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
		bool isMoveModeActive = false;
		MoonlightApp^ movingApp;
		Platform::String^ moveModeSnapshot;
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
		void OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args);
	private:
		void AppsGrid_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
		void AppsGrid_RightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e);
		void AppsGrid_ContextRequested(Platform::Object^ sender, Windows::UI::Xaml::Input::ContextRequestedEventArgs^ e);
		void ShowActionsMenu(Platform::Object^ originalSource, Windows::UI::Xaml::FrameworkElement^ fallbackAnchor);
		void resumeAppButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void closeAppButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void closeAndStartButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void toggleFavoriteButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void moveFavoriteButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Page_PreviewKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		void EnterMoveMode(MoonlightApp^ app);
		void ExitMoveMode(bool commit);
		void FocusAppTile(MoonlightApp^ app);
		void ExecuteCloseAndStart();
		void backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void settingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
 	 };
 }
