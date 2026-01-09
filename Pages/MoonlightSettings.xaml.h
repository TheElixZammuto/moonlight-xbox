//
// MoonlightSettings.xaml.h
// Dichiarazione della classe MoonlightSettings
//

#pragma once

#include "Pages\MoonlightSettings.g.h"

namespace moonlight_xbox_dx
{
	/// <summary>
	/// Pagina vuota che può essere usata autonomamente oppure per l'esplorazione all'interno di un frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class MoonlightSettings sealed
	{
	private: 
		ApplicationState^ state;
		Windows::Foundation::EventRegistrationToken m_back_cookie;
	public:
		MoonlightSettings();
		property ApplicationState^ State {
			ApplicationState^ get() {
				return this->state;
			}
		}

		void OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args);
	private:
		void backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void HostSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void WelcomeButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void PreReleaseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void LayoutSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}
