//
// MoonlightWelcome.xaml.h
// Dichiarazione della classe MoonlightWelcome
//

#pragma once

#include "MoonlightWelcome.g.h"

namespace moonlight_xbox_dx
{
	/// <summary>
	/// Pagina vuota che può essere usata autonomamente oppure per l'esplorazione all'interno di un frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class MoonlightWelcome sealed
	{
	private:
		Windows::Foundation::EventRegistrationToken m_back_token;
	public:
		MoonlightWelcome();
	private:
		void GoForward(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void GoBack(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void CloseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Page_KeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		void OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args);
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}
