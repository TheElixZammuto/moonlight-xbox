﻿//
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

static bool SortModes(HdmiDisplayMode^ a, HdmiDisplayMode^ b) {
	if (a->ResolutionHeightInRawPixels != b->ResolutionHeightInRawPixels) { return a->ResolutionHeightInRawPixels < b->ResolutionHeightInRawPixels; }
	else if (a->ResolutionWidthInRawPixels != b->ResolutionWidthInRawPixels) { return a->ResolutionWidthInRawPixels < b->ResolutionWidthInRawPixels; }
	else if (a->RefreshRate != b->RefreshRate) { return a->RefreshRate < b->RefreshRate; }
	else if (a->BitsPerPixel != b->BitsPerPixel) { return a->BitsPerPixel < b->BitsPerPixel; }
	return a->ColorSpace < b->ColorSpace;
}

int UpdateFPSIndex(Windows::Foundation::Collections::IVector<double>^ availableFPS, double fps)
{
	for (int i = 0; i < availableFPS->Size; i++)
	{
		if (availableFPS->GetAt(i) == fps)
		{
			return i;
		}
	}
	return 0;
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
		auto mode = ref new HdmiDisplayModeWrapper(modes[i]);
		if (mode->IsHdr || mode->IsSdr)
		{
			AvailableModes->Append(mode);
		}
	}

	AvailableVideoCodecs->Append("H.264");
	AvailableVideoCodecs->Append("HEVC (H.265)");
	
	AvailableAudioConfigs->Append("Stereo");
	AvailableAudioConfigs->Append("Surround 5.1");
	AvailableAudioConfigs->Append("Surround 7.1");

	CurrentResolutionIndex = 0;
	if (AvailableModes->Size > 0 && host->HdmiDisplayMode != nullptr)
	{
		for (int i = 0; i < AvailableResolutions->Size; i++) {
			if (AvailableResolutions->GetAt(i)->Height == host->HdmiDisplayMode->HdmiDisplayMode->ResolutionHeightInRawPixels
				&& AvailableResolutions->GetAt(i)->Width == host->HdmiDisplayMode->HdmiDisplayMode->ResolutionWidthInRawPixels) {
				CurrentResolutionIndex = i;
				break;
			}
		}
	}

	AvailableFPS = UpdateFPS();
	CurrentFPSIndex = 0;
	if (AvailableModes->Size > 0 && host->HdmiDisplayMode != nullptr)
	{
		for (int i = 0; i < AvailableFPS->Size; i++) {
			if (AvailableFPS->GetAt(i) == host->HdmiDisplayMode->HdmiDisplayMode->RefreshRate) {
				CurrentFPSIndex = i;
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
		EnableHDRCheckbox->IsChecked = false;
		EnableHDRCheckbox->IsEnabled = false;
		HDRWarning->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
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
	CurrentResolutionIndex = this->ResolutionSelector->SelectedIndex;
	host->HdmiDisplayMode = FilterMode();
	HDRAvailable = IsHDRAvailable();
	AvailableFPS = UpdateFPS();
	OnPropertyChanged("CurrentFPSIndex");
}

void moonlight_xbox_dx::HostSettingsPage::FPSSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	CurrentFPSIndex = this->FPSComboBox->SelectedIndex >= 0 ? this->FPSComboBox->SelectedIndex : UpdateFPSIndex(AvailableFPS, host->HdmiDisplayMode->HdmiDisplayMode->RefreshRate);;
	host->HdmiDisplayMode = FilterMode();
	HDRAvailable = IsHDRAvailable();
}

void moonlight_xbox_dx::HostSettingsPage::EnableHDR_Checked(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	host->HdmiDisplayMode = FilterMode();
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

HdmiDisplayModeWrapper^ moonlight_xbox_dx::HostSettingsPage::FilterMode()
{
	HdmiDisplayModeWrapper^ mode;
	bool hdrAvailable = false;
	for (int i = 0; i < availableModes->Size; i++)
	{
		if (availableModes->GetAt(i)->HdmiDisplayMode->ResolutionHeightInRawPixels == AvailableResolutions->GetAt(CurrentResolutionIndex)->Height
			&& availableModes->GetAt(i)->HdmiDisplayMode->ResolutionWidthInRawPixels == AvailableResolutions->GetAt(CurrentResolutionIndex)->Width
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
	}
	
	if (!mode)
	{
		mode = availableModes->GetAt(0);
	}

	if (!hdrAvailable)
	{
		HDRWarning->Visibility = Windows::UI::Xaml::Visibility::Visible;
	}
	else
	{
		HDRWarning->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	}

	if (mode->HdmiDisplayMode->ResolutionWidthInRawPixels >= 3840 
		&& mode->HdmiDisplayMode->RefreshRate >= 119
		&& mode->IsHdr)
	{
		FourKHDRWarning->Visibility = Windows::UI::Xaml::Visibility::Visible;
	}
	else 
	{
		FourKHDRWarning->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	}

	return mode;
}

Platform::Collections::Vector<double>^ moonlight_xbox_dx::HostSettingsPage::UpdateFPS()
{
	auto availableFPS = ref new Platform::Collections::Vector<double>();
	auto selectedResolution = AvailableResolutions->GetAt(currentResolutionIndex);
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

bool moonlight_xbox_dx::HostSettingsPage::IsHDRAvailable()
{
	bool isAvailable = false;
	for (int i = 0; i < availableModes->Size; i++)
	{
		if (availableModes->GetAt(i)->HdmiDisplayMode->ResolutionWidthInRawPixels == AvailableResolutions->GetAt(currentResolutionIndex)->Width
			&& availableModes->GetAt(i)->HdmiDisplayMode->ResolutionHeightInRawPixels == AvailableResolutions->GetAt(currentResolutionIndex)->Height
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

void moonlight_xbox_dx::HostSettingsPage::OnPropertyChanged(Platform::String^ propertyName)
{
	Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([this, propertyName]() {
		PropertyChanged(this, ref new  Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
		}));
}