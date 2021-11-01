//
// HostSettingsPage.xaml.cpp
// Implementation of the HostSettingsPage class
//

#include "pch.h"
#include "HostSettingsPage.xaml.h"
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

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=234238

HostSettingsPage::HostSettingsPage()
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &HostSettingsPage::OnBackRequested);
	InitializeComponent();

}


void HostSettingsPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	MoonlightHost^ mhost = dynamic_cast<MoonlightHost^>(e->Parameter);
	if (mhost == nullptr)return;
	host = mhost;
	AvailableResolutions->Append(ref new ScreenResolution(1280, 720));
	AvailableResolutions->Append(ref new ScreenResolution(1920, 1080));
	for (int i = 0; i < AvailableResolutions->Size; i++) {
		if (host->Resolution->Width == AvailableResolutions->GetAt(i)->Width &&
			host->Resolution->Height == AvailableResolutions->GetAt(i)->Height
			) {
			CurrentResolutionIndex = i;
			break;
		}
	}
	BitrateInput->Text = host->Bitrate.ToString();
}

void moonlight_xbox_dx::HostSettingsPage::backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
}

void moonlight_xbox_dx::HostSettingsPage::OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args)
{
	// UWP on Xbox One triggers a back request whenever the B
	// button is pressed which can result in the app being
	// suspended if unhandled
	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
	args->Handled = true;

}

void moonlight_xbox_dx::HostSettingsPage::ResolutionSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	host->Resolution = AvailableResolutions->GetAt(this->ResolutionSelector->SelectedIndex);
}


void moonlight_xbox_dx::HostSettingsPage::BitrateInput_TextChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e)
{
	host->Bitrate = std::stoi(Utils::PlatformStringToStdString(this->BitrateInput->Text));
}
