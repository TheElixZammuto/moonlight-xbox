//
// HostSettingsPage.xaml.cpp
// Implementation of the HostSettingsPage class
//

#include "pch.h"
#include "HostSettingsPage.xaml.h"
#include "MoonlightSettings.xaml.h"
#include "Utils.hpp"
#include <vector>
#include <gamingdeviceinformation.h>
using namespace Windows::UI::Core;
using namespace Windows::Graphics::Display::Core;
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

bool SortModes(HdmiDisplayMode^ a, HdmiDisplayMode^ b) {
	if (a->ResolutionHeightInRawPixels != b->ResolutionHeightInRawPixels) { return a->ResolutionHeightInRawPixels < b->ResolutionHeightInRawPixels; }
	else if (a->ResolutionWidthInRawPixels != b->ResolutionWidthInRawPixels) { return a->ResolutionWidthInRawPixels < b->ResolutionWidthInRawPixels; }
	else if (a->RefreshRate!= b->RefreshRate) { return a->RefreshRate < b->RefreshRate;	}
	else if (a->BitsPerPixel!= b->BitsPerPixel) { return a->BitsPerPixel < b->BitsPerPixel;	}
	return a->ColorSpace < b->ColorSpace;
}

void HostSettingsPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	MoonlightHost^ mhost = dynamic_cast<MoonlightHost^>(e->Parameter);
	if (mhost == nullptr)return;
	GAMING_DEVICE_MODEL_INFORMATION info = {};
	GetGamingDeviceModelInformation(&info);
	host = mhost;

	HdmiDisplayInformation^ hdi = HdmiDisplayInformation::GetForCurrentView();
	auto modes = to_vector(hdi->GetSupportedDisplayModes());
	std::sort(modes.begin(), modes.end(), SortModes);

	for (int i = 0; i < modes.size(); i++)
	{
		AvailableResolutions->Append(ref new HdmiDisplayModeWrapper(modes[i]));
	}

	AvailableVideoCodecs->Append("H.264");
	AvailableVideoCodecs->Append("HEVC (H.265)");
	
	AvailableAudioConfigs->Append("Stereo");
	AvailableAudioConfigs->Append("Surround 5.1");
	AvailableAudioConfigs->Append("Surround 7.1");

	// TODO: Fix Current
	CurrentResolutionIndex = 0;
	if (AvailableResolutions->Size > 0 && host->Resolution != nullptr)
	{
		for (int i = 0; i < AvailableResolutions->Size; i++) {
			if (host->Resolution->ResolutionWidthInRawPixels == AvailableResolutions->GetAt(i)->DisplayMode->ResolutionWidthInRawPixels &&
				host->Resolution->ResolutionWidthInRawPixels == AvailableResolutions->GetAt(i)->DisplayMode->ResolutionWidthInRawPixels &&
				host->Resolution->RefreshRate == AvailableResolutions->GetAt(i)->DisplayMode->RefreshRate &&
				host->Resolution->ColorSpace == AvailableResolutions->GetAt(i)->DisplayMode->ColorSpace) {
				CurrentResolutionIndex = i;
				break;
			}
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
	//Old Xbox One can only use H264, remove from settings everything else
	if ((info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT && info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE)) {
		CodecComboBox->IsEnabled = false;
		CodecComboBox->SelectedIndex = 0;
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
	host->Resolution = AvailableResolutions->GetAt(this->ResolutionSelector->SelectedIndex)->DisplayMode;
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