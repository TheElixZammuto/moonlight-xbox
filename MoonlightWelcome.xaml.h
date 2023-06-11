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
	public:
		MoonlightWelcome();
	private:
		void GoForward(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void GoBack(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void CloseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}
