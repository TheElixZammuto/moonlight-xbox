//
// HostSettingsPage.xaml.cpp
// Implementation of the HostSettingsPage class
//

#include "pch.h"
#include "HostSettingsPage.xaml.h"
#include "MoonlightSettings.xaml.h"
#include "Utils.hpp"
#include <gamingdeviceinformation.h>
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
using namespace Windows::UI::ViewManagement::Core;

HostSettingsPage::HostSettingsPage()
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &HostSettingsPage::OnBackRequested);
	InitializeComponent();
	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseVisible);

}


void HostSettingsPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	MoonlightHost^ mhost = dynamic_cast<MoonlightHost^>(e->Parameter);
	if (mhost == nullptr)return;
	GAMING_DEVICE_MODEL_INFORMATION info = {};
	GetGamingDeviceModelInformation(&info);
	host = mhost;
	AvailableResolutions->Append(ref new ScreenResolution(1280, 720));
	AvailableResolutions->Append(ref new ScreenResolution(1920, 1080));
	//No 4K for Old Xbox One
	if (!(info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT && info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE)) {
		AvailableResolutions->Append(ref new ScreenResolution(2560, 1440));
		AvailableResolutions->Append(ref new ScreenResolution(3840, 2160));
	}
	AvailableFPS->Append(30);
	AvailableFPS->Append(60);
	AvailableFPS->Append(120);
	AvailableVideoCodecs->Append("H.264");
	AvailableVideoCodecs->Append("HEVC (H.265)");
	AvailableAudioConfigs->Append("Stereo");
	AvailableAudioConfigs->Append("Surround 5.1");
	AvailableAudioConfigs->Append("Surround 7.1");
	CurrentResolutionIndex = 0;
	for (int i = 0; i < AvailableResolutions->Size; i++) {
		if (host->Resolution->Width == AvailableResolutions->GetAt(i)->Width &&
			host->Resolution->Height == AvailableResolutions->GetAt(i)->Height
			) {
			CurrentResolutionIndex = i;
			break;
		}
	}
	CurrentAppIndex = 0;
	auto item = ref new ComboBoxItem();
	item->Content = L"No App";
	AutoStartSelector->Items->Append(item);
	for (int i = 0; i < Host->Apps->Size; i++) {
		auto item = ref new ComboBoxItem();
		item->Content = Host->Apps->GetAt(i)->Name;
		AutoStartSelector->Items->Append(item);
		if (host->AutostartID == host->Apps->GetAt(i)->Id) {
			CurrentAppIndex = i + 1;
		}
	}
	AutoStartSelector->SelectedIndex = CurrentAppIndex;
	BitrateInput->Text = host->Bitrate.ToString();
	//Old Xbox One can only use H264, remove from settings everything else
	if ((info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT && info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE)) {
		CodecComboBox->IsEnabled = false;
		CodecComboBox->SelectedIndex = 0;
		EnableHDRCheckbox->IsChecked = false;
		EnableHDRCheckbox->IsEnabled = false;
	}
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


void moonlight_xbox_dx::HostSettingsPage::AutoStartSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	int index = AutoStartSelector->SelectedIndex - 1;
	if (index >= 0 && host->Apps->Size > index) {
		host->AutostartID = host->Apps->GetAt(index)->Id;
	}
	else {
		host->AutostartID = -1;
	}
}


void moonlight_xbox_dx::HostSettingsPage::GlobalSettingsOption_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightSettings::typeid));
}


void moonlight_xbox_dx::HostSettingsPage::BitrateInput_KeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
	if (e->Key == Windows::System::VirtualKey::Enter) {
		CoreInputView::GetForCurrentView()->TryHide();
	}
}
