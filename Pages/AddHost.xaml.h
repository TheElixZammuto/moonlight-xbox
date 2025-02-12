//
// AddHost.xaml.h
// Declaration of the AddHost class
//

#pragma once

#include "Pages\AddHost.g.h"
#include "State\ApplicationState.h"

using namespace Windows::UI::Core;
namespace moonlight_xbox_dx
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class AddHost sealed
	{
	public:
		AddHost();
		property ApplicationState^ State {
			ApplicationState^ get() {
				return this->state;
			}
		}
	private:
		ApplicationState^ state;
		void OnNewHostDialogPrimaryClick(Windows::UI::Xaml::Controls::ContentDialog^ sender, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs^ args);
		void OnKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
	};
}
