//
// HostSettingsPage.xaml.cpp
// Implementation of the HostSettingsPage class
//

#include "pch.h"
#include "HostSettingsPage.xaml.h"
#include "MoonlightSettings.xaml.h"
#include "Utils.hpp"
#include <gamingdeviceinformation.h>
#include <Limelight.h> // SCM_* server codec support bits
#include <cmath> // sqrtf, lround

using namespace Windows::UI::Core;

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display::Core;
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
	// PyroWave options only appear when the server advertised support in its
	// last serverinfo response AND this console can decode it. The Xbox One
	// generation cannot: UWP there never exposes compute shaders.
	if (!IsXboxOne() && (host->ServerCodecModeSupport & SCM_PYROWAVE)) {
		AvailableVideoCodecs->Append("PyroWave 4:2:0");
		if (host->ServerCodecModeSupport & SCM_PYROWAVE_444) {
			AvailableVideoCodecs->Append("PyroWave 4:4:4");
		}
	}
	// If the saved codec is no longer offered (e.g. PyroWave selected but the
	// server lost support), fall back so the ComboBox isn't blank.
	{
		bool codecAvailable = false;
		for (unsigned int i = 0; i < AvailableVideoCodecs->Size; i++) {
			if (AvailableVideoCodecs->GetAt(i) == host->VideoCodec) {
				codecAvailable = true;
				break;
			}
		}
		if (!codecAvailable) {
			host->VideoCodec = "HEVC (H.265)";
		}
	}
	AvailableAudioConfigs->Append("Stereo");
	AvailableAudioConfigs->Append("Surround 5.1");
	AvailableAudioConfigs->Append("Surround 7.1");
	AvailableFramePacing->Append("Immediate");
	AvailableFramePacing->Append("Display-locked");
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

	// Set frame pacing selection based on saved preference
	for (int i = 0; i < AvailableFramePacing->Size; i++) {
		if (host->FramePacing == AvailableFramePacing->GetAt(i)) {
			FramePacingComboBox->SelectedIndex = i;
			break;
		}
	}

	// Audio Buffer options
	AvailableAudioBuffers->Append("10 ms");
	AvailableAudioBuffers->Append("20 ms");
	AvailableAudioBuffers->Append("30 ms");
	AvailableAudioBuffers->Append("40 ms");
	AvailableAudioBuffers->Append("50 ms");
	for (int i = 0; i < AvailableAudioBuffers->Size; i++) {
		if (host->AudioBuffer == AvailableAudioBuffers->GetAt(i)) {
			AudioBuffersComboBox->SelectedIndex = i;
			break;
		}
	}

	AvailablePacketSizes->Append(1392);
	AvailablePacketSizes->Append(1024);
	for (int i = 0; i < AvailablePacketSizes->Size; i++) {
		if (host->PacketSize == AvailablePacketSizes->GetAt(i)) {
			PacketSizeSelector->SelectedIndex = i;
			break;
		}
	}

	if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT) {
		// Old Xbox One can only use H264, remove from settings everything else
		if (info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE) {
			CodecComboBox->IsEnabled = false;
			CodecComboBox->SelectedIndex = 0;
		}

		// Disable HDR if console is not set to 4K
		HdmiDisplayInformation ^ hdmi = HdmiDisplayInformation::GetForCurrentView();
		auto mode = hdmi->GetCurrentDisplayMode();
		auto height = mode->ResolutionHeightInRawPixels;
		bool has4K = height >= 2160;

		// Make sure an HDR mode exists, in case HDR is unchecked in Xbox video settings.
		bool hasHDR = false;
		for (auto mode : hdmi->GetSupportedDisplayModes()) {
			if (mode->IsSmpte2084Supported) {
				hasHDR = true;
				break;
			}
		}

		if (!has4K) {
			// HDR4KNote (not at 4K)
			EnableHDRCheckbox->IsEnabled = false;
			EnableHDRCheckbox->IsChecked = false;
			HDR4KNote->Visibility = Windows::UI::Xaml::Visibility::Visible;
			HDRNotAvailNote->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
			EnableHDRCheckbox->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
		} else if (!hasHDR) {
			// HDRNotAvailNote (no HDR modes)
			EnableHDRCheckbox->IsEnabled = false;
			EnableHDRCheckbox->IsChecked = false;
			HDR4KNote->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
			HDRNotAvailNote->Visibility = Windows::UI::Xaml::Visibility::Visible;
			EnableHDRCheckbox->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
		} else {
			// EnableHDRCheckboxDesc (explain 10-bit vs 8-bit)
			EnableHDRCheckbox->IsEnabled = true;
			HDR4KNote->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
			HDRNotAvailNote->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
			EnableHDRCheckbox->Visibility = Windows::UI::Xaml::Visibility::Visible;
		}

		// Disable graphs at 4K on Xbox One
		if (IsXboxOne() && height >= 2160) {
			EnableGraphsCheckbox->IsEnabled = false;
			EnableGraphsCheckbox->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
			XboxOneGraphsNote->Visibility = Windows::UI::Xaml::Visibility::Visible;
		} else {
			EnableGraphsCheckbox->IsEnabled = true;
			EnableGraphsCheckbox->Visibility = Windows::UI::Xaml::Visibility::Visible;
			XboxOneGraphsNote->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
		}
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

void HostSettingsPage::ResolutionSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	auto selectedResolution = AvailableResolutions->GetAt(this->ResolutionSelector->SelectedIndex);

	// Default to a new bitrate if a new resolution was chosen
	if (selectedResolution->Width != host->Resolution->Width) {
		host->Bitrate = getDefaultBitrate(selectedResolution->Width, selectedResolution->Height, host->FPS, host->VideoCodec);
	}

	host->Resolution = selectedResolution;
}

void HostSettingsPage::FPSSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	if (e->AddedItems->Size == 0) return;
	int selectedFPS = (int)e->AddedItems->GetAt(0);

	// Default to a new bitrate if a new FPS was chosen
	if (selectedFPS != host->FPS) {
		host->Bitrate = getDefaultBitrate(host->Resolution->Width, host->Resolution->Height, selectedFPS, host->VideoCodec);
	}

	host->FPS = selectedFPS;
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

void HostSettingsPage::FramePacing_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	auto selectedFramePacing = AvailableFramePacing->GetAt(this->FramePacingComboBox->SelectedIndex);

	if (selectedFramePacing == "Immediate") {
		FramePacingImmediateDesc->Visibility = Windows::UI::Xaml::Visibility::Visible;
		FramePacingDisplayLockedDesc->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	} else {
		FramePacingImmediateDesc->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
		FramePacingDisplayLockedDesc->Visibility = Windows::UI::Xaml::Visibility::Visible;
	}

	host->FramePacing = selectedFramePacing;
}

void HostSettingsPage::PacketSizeSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	if (host == nullptr || e->AddedItems->Size == 0) return;
	int selectedPacketSize = (int)e->AddedItems->GetAt(0);

	PacketSize1392Desc->Visibility = selectedPacketSize == 1392 ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
	PacketSize1024Desc->Visibility = selectedPacketSize == 1024 ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
	host->PacketSize = selectedPacketSize;
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

void HostSettingsPage::OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	m_back_cookie = navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &HostSettingsPage::OnBackRequested);
}

void HostSettingsPage::CodecComboBox_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	if (e->AddedItems->Size == 0) return;
	auto selectedCodec = (Platform::String^)e->AddedItems->GetAt(0);

	if (selectedCodec != host->VideoCodec) {
		// Default to a new bitrate if a new codec was chosen
		host->Bitrate = getDefaultBitrate(host->Resolution->Width, host->Resolution->Height, host->FPS, selectedCodec);
	}

	if (selectedCodec == "PyroWave 4:2:0" || selectedCodec == "PyroWave 4:4:4") {
		CodecDescPyroWave->Visibility = Windows::UI::Xaml::Visibility::Visible;
		CodecDesc->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	} else {
		CodecDescPyroWave->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
		CodecDesc->Visibility = Windows::UI::Xaml::Visibility::Visible;
	}

	host->VideoCodec = selectedCodec;
}

void HostSettingsPage::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_cookie;
}

int HostSettingsPage::getDefaultBitrate(int width, int height, int fps, Platform::String^ codec) {
	// Don't scale bitrate linearly beyond 60 FPS. It's definitely not a linear
	// bitrate increase for frame rate once we get to values that high.
	float frameRateFactor = (fps <= 60 ? fps : (std::sqrtf(fps / 60.f) * 60.f)) / 30.f;

	// TODO: Collect some empirical data to see if these defaults make sense.
	// We're just using the values that the Shield used, as we have for years.
	static const struct resTable {
		int pixels;
		float factor;
	} resTable[]{
	    {1280 * 720, 5.0f},
	    {1920 * 1080, 10.0f},
	    {2560 * 1440, 20.0f},
	    {3840 * 2160, 40.0f},
	    {-1, -1.0f},
	};

	// Calculate the resolution factor by linear interpolation of the resolution table
	float resolutionFactor;
	int pixels = width * height;
	for (int i = 0;; i++) {
		if (pixels == resTable[i].pixels) {
			// We can bail immediately for exact matches
			resolutionFactor = resTable[i].factor;
			break;
		} else if (pixels < resTable[i].pixels) {
			if (i == 0) {
				// Never go below the lowest resolution entry
				resolutionFactor = resTable[i].factor;
			} else {
				// Interpolate between the entry greater than the chosen resolution (i) and the entry less than the chosen resolution (i-1)
				resolutionFactor = ((float)(pixels - resTable[i - 1].pixels) / (resTable[i].pixels - resTable[i - 1].pixels)) * (resTable[i].factor - resTable[i - 1].factor) + resTable[i - 1].factor;
			}
			break;
		} else if (resTable[i].pixels == -1) {
			// Never go above the highest resolution entry
			resolutionFactor = resTable[i - 1].factor;
			break;
		}
	}

	// PyroWave needs far more bitrate than H.264/HEVC.
	// 8-bit vs 10-bit could also be considered but I'm just going to
	// assume most people will use 10-bit.
	//             4:2:0   4:4:4
	// 4K120:        700     800
	// 1440p120:     500     600
	// 1080p120:     400     500
	// 60fps:        / 2
	if (codec == L"PyroWave 4:2:0" || codec == L"PyroWave 4:4:4") {
		int bitrate = 0;
		if (height <= 1080) {
			bitrate = (codec == L"PyroWave 4:4:4") ? 500 : 400;
		} else if (height <= 1440) {
			bitrate = (codec == L"PyroWave 4:4:4") ? 600 : 500;
		} else if (height <= 2160) {
			bitrate = (codec == L"PyroWave 4:4:4") ? 800 : 700;
		}

		bitrate /= (120 / fps);
		return bitrate * 1000;
	}

	return std::lround(resolutionFactor * frameRateFactor) * 1000;
}
