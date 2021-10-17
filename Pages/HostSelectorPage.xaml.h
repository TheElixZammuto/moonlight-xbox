//
// HostSelectorPage.xaml.h
// Declaration of the HostSelectorPage class
//

#pragma once

#include "Pages\HostSelectorPage.g.h"
#include "State\ApplicationState.h"

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
	private:
		ApplicationState ^state;
		void NewHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnNewHostDialogPrimaryClick(Windows::UI::Xaml::Controls::ContentDialog^ sender, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs^ args);
		Windows::UI::Xaml::Controls::TextBox ^dialogHostnameTextBox;
	};
}
