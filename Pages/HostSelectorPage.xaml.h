﻿//
// HostSelectorPage.xaml.h
// Declaration of the HostSelectorPage class
//

#pragma once

#include "Pages\HostSelectorPage.g.h"
#include "State\ApplicationState.h"

using namespace Windows::UI::Core;
namespace moonlight_xbox_dx
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
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
	private:
		ApplicationState ^state;
		void NewHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void GridView_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
		void StartPairing(MoonlightHost^ host);
		void removeHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void HostsGrid_RightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e);
		MoonlightHost^ currentHost;
		void hostSettingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void SettingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		bool continueFetch = false;
		void OnKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		void wakeHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}
