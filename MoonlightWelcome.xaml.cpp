//
// MoonlightWelcome.xaml.cpp
// Implementazione della classe MoonlightWelcome
//

#include "pch.h"
#include "MoonlightWelcome.xaml.h"

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

// Il modello di elemento Pagina vuota è documentato all'indirizzo https://go.microsoft.com/fwlink/?LinkId=234238

MoonlightWelcome::MoonlightWelcome()
{
	InitializeComponent();
}


void moonlight_xbox_dx::MoonlightWelcome::GoForward(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->FlipView->SelectedIndex = this->FlipView->SelectedIndex + 1;
}

void moonlight_xbox_dx::MoonlightWelcome::GoBack(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->FlipView->SelectedIndex = this->FlipView->SelectedIndex - 1;
}

void moonlight_xbox_dx::MoonlightWelcome::CloseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->Frame->GoBack();
}
