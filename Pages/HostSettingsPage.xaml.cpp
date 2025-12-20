//
// HostSettingsPage.xaml.cpp
// Implementation of the HostSettingsPage class
//

#include "pch.h"
#include "HostSettingsPage.xaml.h"
#include "MoonlightSettings.xaml.h"
#include "Utils.hpp"
#include <gamingdeviceinformation.h>
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

	if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT) {
		// Old Xbox One can only use H264, remove from settings everything else
		if (info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE) {
			CodecComboBox->IsEnabled = false;
			CodecComboBox->SelectedIndex = 0;
		}

		// Disable HDR if console is not set to 4K
		auto mode = HdmiDisplayInformation::GetForCurrentView()->GetCurrentDisplayMode();
		auto height = mode->ResolutionHeightInRawPixels;
		if (height < 2160) {
			EnableHDRCheckbox->IsEnabled = false;
			EnableHDRCheckbox->IsChecked = false;
			EnableHDRCheckbox->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
			HDR4KNote->Visibility = Windows::UI::Xaml::Visibility::Visible;
		} else {
			EnableHDRCheckbox->IsEnabled = true;
			EnableHDRCheckbox->Visibility = Windows::UI::Xaml::Visibility::Visible;
			HDR4KNote->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
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
		host->Bitrate = getDefaultBitrate(selectedResolution->Width, selectedResolution->Height, host->FPS);
	}

	host->Resolution = selectedResolution;
}

void HostSettingsPage::FPSSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	if (e->AddedItems->Size == 0) return;
	int selectedFPS = (int)e->AddedItems->GetAt(0);

	// Default to a new bitrate if a new FPS was chosen
	if (selectedFPS != host->FPS) {
		host->Bitrate = getDefaultBitrate(host->Resolution->Width, host->Resolution->Height, selectedFPS);
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

void HostSettingsPage::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_cookie;
}

int HostSettingsPage::getDefaultBitrate(int width, int height, int fps)
{
    // Don't scale bitrate linearly beyond 60 FPS. It's definitely not a linear
    // bitrate increase for frame rate once we get to values that high.
    float frameRateFactor = (fps <= 60 ? fps : (std::sqrtf(fps / 60.f) * 60.f)) / 30.f;

    // TODO: Collect some empirical data to see if these defaults make sense.
    // We're just using the values that the Shield used, as we have for years.
    static const struct resTable {
        int pixels;
        float factor;
    } resTable[] {
        { 1280 * 720, 5.0f },
        { 1920 * 1080, 10.0f },
        { 2560 * 1440, 20.0f },
        { 3840 * 2160, 40.0f },
        { -1, -1.0f },
    };

    // Calculate the resolution factor by linear interpolation of the resolution table
    float resolutionFactor;
    int pixels = width * height;
    for (int i = 0;; i++) {
        if (pixels == resTable[i].pixels) {
            // We can bail immediately for exact matches
            resolutionFactor = resTable[i].factor;
            break;
        }
        else if (pixels < resTable[i].pixels) {
            if (i == 0) {
                // Never go below the lowest resolution entry
                resolutionFactor = resTable[i].factor;
            }
            else {
                // Interpolate between the entry greater than the chosen resolution (i) and the entry less than the chosen resolution (i-1)
                resolutionFactor = ((float)(pixels - resTable[i-1].pixels) / (resTable[i].pixels - resTable[i-1].pixels)) * (resTable[i].factor - resTable[i-1].factor) + resTable[i-1].factor;
            }
            break;
        }
        else if (resTable[i].pixels == -1) {
            // Never go above the highest resolution entry
            resolutionFactor = resTable[i-1].factor;
            break;
        }
    }

    return std::lround(resolutionFactor * frameRateFactor) * 1000;
}
