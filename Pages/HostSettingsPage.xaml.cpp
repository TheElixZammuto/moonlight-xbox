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
	InitializeComponent();
	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseVisible);
	this->Loaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &HostSettingsPage::OnLoaded);
	this->Unloaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &HostSettingsPage::OnUnloaded);
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
	//Old Xbox One can only use H264, remove from settings everything else
	if ((info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT && info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE)) {
		CodecComboBox->IsEnabled = false;
		CodecComboBox->SelectedIndex = 0;
		EnableHDRCheckbox->IsChecked = false;
		EnableHDRCheckbox->IsEnabled = false;
	}
}

void HostSettingsPage::backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
}

void HostSettingsPage::OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args)
{
	// UWP on Xbox One triggers a back request whenever the B
	// button is pressed which can result in the app being
	// suspended if unhandled
	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
	args->Handled = true;

}

void HostSettingsPage::StreamResolutionSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	CurrentStreamResolutionIndex = this->StreamResolutionSelector->SelectedIndex;
	host->StreamResolution = AvailableStreamResolutions->GetAt(CurrentStreamResolutionIndex);
}

void HostSettingsPage::DisplayResolutionSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	CurrentDisplayResolutionIndex = this->DisplayResolutionSelector->SelectedIndex;
	host->HdmiDisplayMode = FilterMode();
	HDRAvailable = IsHDRAvailable();
	AvailableFPS = UpdateFPS();
	OnPropertyChanged("CurrentFPSIndex");
}

void HostSettingsPage::FPSSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	CurrentFPSIndex = this->FPSComboBox->SelectedIndex >= 0 ? this->FPSComboBox->SelectedIndex : UpdateFPSIndex(AvailableFPS, host->HdmiDisplayMode->HdmiDisplayMode->RefreshRate);;
	host->HdmiDisplayMode = FilterMode();
	HDRAvailable = IsHDRAvailable();
}

void HostSettingsPage::EnableHDR_Checked(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	host->HdmiDisplayMode = FilterMode();
}

void HostSettingsPage::AutoStartSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	int index = AutoStartSelector->SelectedIndex - 1;
	if (index >= 0 && host->Apps->Size > index) {
		host->AutostartID = host->Apps->GetAt(index)->Id;
	}
	else {
		host->AutostartID = -1;
	}
}


void HostSettingsPage::GlobalSettingsOption_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightSettings::typeid));
}

void HostSettingsPage::BitrateInput_KeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
	if (e->Key == Windows::System::VirtualKey::Enter) {
		CoreInputView::GetForCurrentView()->TryHide();
	}
}
<<<<<<< HEAD
=======

HdmiDisplayModeWrapper^ HostSettingsPage::FilterMode()
{
	HdmiDisplayModeWrapper^ mode;
	bool hdrAvailable = false;
	std::vector<std::string> hdrRefreshRates;

	for (int i = 0; i < availableModes->Size; i++)
	{
		if (availableModes->GetAt(i)->HdmiDisplayMode->ResolutionHeightInRawPixels == AvailableDisplayResolutions->GetAt(CurrentDisplayResolutionIndex)->Height
			&& availableModes->GetAt(i)->HdmiDisplayMode->ResolutionWidthInRawPixels == AvailableDisplayResolutions->GetAt(CurrentDisplayResolutionIndex)->Width
			&& availableModes->GetAt(i)->HdmiDisplayMode->RefreshRate == AvailableFPS->GetAt(CurrentFPSIndex))
		{
			if (availableModes->GetAt(i)->IsHdr == host->EnableHDR)
			{
				mode = availableModes->GetAt(i);
			}

			if (availableModes->GetAt(i)->IsHdr)
			{
				hdrAvailable = true;
			}
		}

		if (availableModes->GetAt(i)->IsHdr)
		{
			hdrRefreshRates.push_back(Utils::PlatformStringToStdString(availableModes->GetAt(i)->HdmiDisplayMode->ResolutionWidthInRawPixels + "x" + availableModes->GetAt(i)->HdmiDisplayMode->ResolutionHeightInRawPixels +
				"@" + availableModes->GetAt(i)->HdmiDisplayMode->RefreshRate + "hz"));
		}
	}
	
	if (!mode)
	{
		mode = availableModes->GetAt(0);
	}

	std::string hdrWarning = "";
	if (mode->HdmiDisplayMode->ResolutionWidthInRawPixels >= 3840 
		&& mode->HdmiDisplayMode->RefreshRate >= 119
		&& mode->IsHdr)
	{
		hdrWarning = "Warning: HDR at 4k@120hz has been known to be unstable.";
	}
	else if (!hdrAvailable)
	{
		std::string refreshRateString = "";
		for (int i = 0; i < hdrRefreshRates.size(); i++)
		{
			if (i > 0 && hdrRefreshRates.size() > 2)
			{
				refreshRateString += ", ";
			}

			if (i == hdrRefreshRates.size() - 1)
			{
				refreshRateString += "and ";
			}

			refreshRateString += hdrRefreshRates[i];
		}

		hdrWarning = "HDR is currently available at: " + refreshRateString;

		if (hdrRefreshRates.empty())
		{
			hdrWarning = "HDR is currently not available on this device";
		}
	}
	else
	{
		hdrWarning = "";
	}
	
	HDRWarning->Text = Utils::StringFromStdString(hdrWarning);

	return mode;
}

Platform::Collections::Vector<double>^ HostSettingsPage::UpdateFPS()
{
	auto availableFPS = ref new Platform::Collections::Vector<double>();
	auto selectedResolution = AvailableDisplayResolutions->GetAt(currentDisplayResolutionIndex);
	int width = selectedResolution->Width;
	int height = selectedResolution->Height;

	for (int i = 0; i < availableModes->Size; i++)
	{
		auto availableMode = availableModes->GetAt(i)->HdmiDisplayMode;
		if (selectedResolution->Width == availableMode->ResolutionWidthInRawPixels
			&& selectedResolution->Height == availableMode->ResolutionHeightInRawPixels)
		{
			bool contains = false;
			for (int j = 0; j < availableFPS->Size; j++)
			{
				if (availableFPS->GetAt(j) == availableMode->RefreshRate)
				{
					contains = true;
				}
			}

			if (!contains)
			{
				auto res = availableMode->RefreshRate;
				availableFPS->Append(availableMode->RefreshRate);
			}
		}
	}

	return availableFPS;
}

bool HostSettingsPage::IsHDRAvailable()
{
	bool isAvailable = false;
	for (int i = 0; i < availableModes->Size; i++)
	{
		if (availableModes->GetAt(i)->HdmiDisplayMode->ResolutionWidthInRawPixels == AvailableDisplayResolutions->GetAt(currentDisplayResolutionIndex)->Width
			&& availableModes->GetAt(i)->HdmiDisplayMode->ResolutionHeightInRawPixels == AvailableDisplayResolutions->GetAt(currentDisplayResolutionIndex)->Height
			&& availableModes->GetAt(i)->HdmiDisplayMode->RefreshRate == AvailableFPS->GetAt(currentFPSIndex)
			&& availableModes->GetAt(i)->IsHdr)
		{
			isAvailable = true;
		}
	}

	if (!isAvailable)
	{
		host->EnableHDR = false;
	}

	return isAvailable;
}

void HostSettingsPage::OnPropertyChanged(Platform::String^ propertyName)
{
	Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([this, propertyName]() {
		PropertyChanged(this, ref new  Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
		}));
}

void HostSettingsPage::OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	m_back_cookie = navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &HostSettingsPage::OnBackRequested);
}

void HostSettingsPage::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_cookie;
}
