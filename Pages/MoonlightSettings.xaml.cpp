//
// MoonlightSettings.xaml.cpp
// Implementazione della classe MoonlightSettings
//

#include "pch.h"
#include "MoonlightSettings.xaml.h"
#include "Utils.hpp"
using namespace Windows::UI::Core;

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

MoonlightSettings::MoonlightSettings()
{
	InitializeComponent();
	state = GetApplicationState();
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &MoonlightSettings::OnBackRequested);
	auto item = ref new ComboBoxItem();
	item->Content = "Don't autoconnect";
	item->DataContext = "";
	HostSelector->Items->Append(item);
	AvailableCompositionScale->Append("Normal");
	AvailableCompositionScale->Append("4k Xbox Series S/Xbox One S");
	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);
	auto iid = Utils::StringFromStdString(state->autostartInstance);
	for (int i = 0; i < state->SavedHosts->Size;i++) {
		auto host = state->SavedHosts->GetAt(i);
		auto item = ref new ComboBoxItem();
		item->Content = host->LastHostname;
		item->DataContext = host->InstanceId;
		HostSelector->Items->Append(item);
		if (host->InstanceId->Equals(iid)) {
			HostSelector->SelectedIndex = i+1;
		}
	}
}


void moonlight_xbox_dx::MoonlightSettings::backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
}


void moonlight_xbox_dx::MoonlightSettings::HostSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	ComboBoxItem^ item = (ComboBoxItem^)this->HostSelector->SelectedItem;

	auto s = Utils::PlatformStringToStdString(item->DataContext->ToString());
	state->autostartInstance = s;

}

void moonlight_xbox_dx::MoonlightSettings::CompositionScaleSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	state->compositionScale = AvailableCompositionScale->GetAt(this->CompositionScaleSelector->SelectedIndex);
}


void moonlight_xbox_dx::MoonlightSettings::OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args)
{
	// UWP on Xbox One triggers a back request whenever the B
	// button is pressed which can result in the app being
	// suspended if unhandled
	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
	args->Handled = true;

}