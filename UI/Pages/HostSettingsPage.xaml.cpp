
#include "pch.h"
#include "HostSettingsPage.xaml.h"
#include "UI\Backgrounds\DynamicBackgroundHost.xaml.h"
#include "UI\Backgrounds\BackgroundRegistry.h"
#include "UI\Backgrounds\Particles\ParticleSettingsControl.xaml.h"
#include "UI\Backgrounds\Streaks\StreaksSettingsControl.xaml.h"
#include "UI\Backgrounds\Spheres\SpheresSettingsControl.xaml.h"
#include "UI\Backgrounds\Blobs\BlobsSettingsControl.xaml.h"
#include "UI\Backgrounds\Orbs\OrbsSettingsControl.xaml.h"
#include "UI\Controls\TabsLayout.xaml.h"
#include "UI\Controls\SwatchPicker.xaml.h"
#include "UI\Pages\MoonlightSettings.xaml.h"
#include "Utils.hpp"
#include "UI\Utilities\XamlHelpers.h"
#include "UI\Utilities\ToastService.h"
#include <gamingdeviceinformation.h>
#include <cmath>
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
using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::ViewManagement::Core;

HostSettingsPage::HostSettingsPage()
{
	InitializeComponent();
	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);
	this->Loaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &HostSettingsPage::OnLoaded);
	this->Unloaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &HostSettingsPage::OnUnloaded);
}

void HostSettingsPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	MoonlightHost^ mhost = dynamic_cast<MoonlightHost^>(e->Parameter);
	if (mhost == nullptr) return;
	host = mhost;

	m_savedGlobalBg = nullptr;
	auto localSettings = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
	if (localSettings->HasKey("background")) {
		m_savedGlobalBg = safe_cast<Platform::String^>(localSettings->Lookup("background"));
	}
	auto hostBg = host->Personalization->Background;
	if (hostBg != nullptr && !hostBg->IsEmpty()) {
		localSettings->Insert("background", hostBg);
	}

	try {
		if (BackgroundHost != nullptr) {
			auto singleHost = ref new Platform::Collections::Vector<MoonlightHost^>();
			singleHost->Append(host);
			BackgroundHost->SetHosts(singleHost);
			BackgroundHost->Refresh();
			BackgroundHost->StartAnimations();
		}
	} catch (...) {}

	GAMING_DEVICE_MODEL_INFORMATION info = {};
	GetGamingDeviceModelInformation(&info);
	AvailableResolutions->Append(ref new ScreenResolution(1280, 720));
	AvailableResolutions->Append(ref new ScreenResolution(1920, 1080));

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

	m_initializingBg = true;
	auto hostBgKey = host->Personalization->Background;
	bool foundBg = false;
	for (int i = 0; i < kBackgroundCount; ++i) {
		auto bgItem = ref new ComboBoxItem();
		bgItem->Content = ref new Platform::String(kBackgrounds[i].displayName);
		bgItem->DataContext = ref new Platform::String(kBackgrounds[i].key);
		HostBackgroundSelector->Items->Append(bgItem);
		if (hostBgKey != nullptr && hostBgKey->Equals(ref new Platform::String(kBackgrounds[i].key))) {
			HostBackgroundSelector->SelectedIndex = i;
			foundBg = true;
		}
	}
	if (!foundBg) HostBackgroundSelector->SelectedIndex = 0;
	m_initializingBg = false;

	{
		auto selItem = dynamic_cast<ComboBoxItem^>(HostBackgroundSelector->SelectedItem);
		Platform::String^ selKey = (selItem != nullptr && selItem->DataContext != nullptr)
			? selItem->DataContext->ToString() : nullptr;
		UpdateBackgroundSettingsContent(selKey);
	}

	AccentColorPicker->ColorChanged += ref new SwatchColorChangedHandler(
		this, &HostSettingsPage::AccentColorPicker_ColorChanged);
	AccentColorPicker->SelectColor(
		host->Personalization->AccentColor,
		host->Personalization->UseSystemAccent);

	{
		Windows::UI::Color accentColor = host->Personalization->UseSystemAccent
			? (ref new UISettings())->GetColorValue(UIColorType::Accent)
			: host->Personalization->AccentColor;
		ApplyAccentColor(accentColor);
		auto cur = this->ActualTheme;
		this->RequestedTheme = (cur != ElementTheme::Dark) ? ElementTheme::Dark : ElementTheme::Light;
		this->RequestedTheme = ElementTheme::Default;
		if (rightTabs != nullptr) rightTabs->AccentColor = accentColor;
	}

	if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT) {

		if (info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE) {
			CodecComboBox->IsEnabled = false;
			CodecComboBox->SelectedIndex = 0;
		}

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
	}
}

void HostSettingsPage::OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {

	if (m_savedGlobalBg != nullptr) {
		auto localSettings = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
		localSettings->Insert("background", m_savedGlobalBg);
		m_savedGlobalBg = nullptr;
	}
	try {
		if (BackgroundHost != nullptr) BackgroundHost->StopAnimations();
	} catch (...) {}
}

void HostSettingsPage::backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
}

void HostSettingsPage::OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args)
{

	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
	args->Handled = true;

}

void HostSettingsPage::ResolutionSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	auto selectedResolution = AvailableResolutions->GetAt(this->ResolutionSelector->SelectedIndex);

	if (selectedResolution->Width != host->Resolution->Width) {
		host->Bitrate = getDefaultBitrate(selectedResolution->Width, selectedResolution->Height, host->FPS);
	}

	host->Resolution = selectedResolution;
}

void HostSettingsPage::FPSSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	if (e->AddedItems->Size == 0) return;
	int selectedFPS = (int)e->AddedItems->GetAt(0);

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

void HostSettingsPage::ComboBox_DropDownOpened(Platform::Object^ sender, Platform::Object^ e)
{
	auto comboBox = safe_cast<ComboBox^>(sender);
	for (unsigned int i = 0; i < comboBox->Items->Size; i++) {
		auto container = comboBox->ContainerFromIndex(i);
		if (container == nullptr) continue;
		auto item = dynamic_cast<ComboBoxItem^>(container);
		if (item == nullptr) continue;
		VisualStateManager::GoToState(item, "Unfocused", false);
		VisualStateManager::GoToState(item, item->IsSelected ? "Selected" : "Unselected", false);
	}
}

void HostSettingsPage::BackgroundSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	if (host == nullptr || m_initializingBg) return;
	auto item = dynamic_cast<ComboBoxItem^>(HostBackgroundSelector->SelectedItem);
	if (item == nullptr) return;
	auto key = item->DataContext->ToString();
	host->Personalization->Background = key;

	auto localSettings = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
	if (key != nullptr && !key->IsEmpty()) {
		localSettings->Insert("background", key);
	}
	try {
		if (BackgroundHost != nullptr) {
			BackgroundHost->Refresh();
			BackgroundHost->StartAnimations();
		}
	} catch (...) {}
	UpdateBackgroundSettingsContent(key);
}

void HostSettingsPage::AccentColorPicker_ColorChanged(Platform::Object^ sender, Windows::UI::Color color, bool useSystemAccent)
{
	if (host == nullptr) return;
	host->Personalization->AccentColor = color;
	host->Personalization->UseSystemAccent = useSystemAccent;

	Windows::UI::Color effective = useSystemAccent
		? (ref new UISettings())->GetColorValue(UIColorType::Accent)
		: color;
	ApplyAccentColor(effective);

	auto cur = this->ActualTheme;
	this->RequestedTheme = (cur != ElementTheme::Dark) ? ElementTheme::Dark : ElementTheme::Light;
	this->RequestedTheme = ElementTheme::Default;

	if (rightTabs != nullptr) rightTabs->AccentColor = effective;
}

void HostSettingsPage::GlobalSettingsOption_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto slideForward = ref new Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionInfo();
	slideForward->Effect = Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionEffect::FromRight;
	this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightSettings::typeid), nullptr, slideForward);
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

	if (this->DisplayPanel != nullptr)
		this->DisplayPanel->Visibility = Windows::UI::Xaml::Visibility::Visible;
	if (this->AudioPanel != nullptr)
		this->AudioPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	if (this->StreamPanel != nullptr)
		this->StreamPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	if (this->DetailsPanel != nullptr)
		this->DetailsPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;

	if (this->rightTabs != nullptr) {

		auto tabsContent = this->rightTabs->TabsContent;
		if (tabsContent != nullptr) {
			auto panel = dynamic_cast<Windows::UI::Xaml::Controls::Panel^>(safe_cast<Windows::UI::Xaml::UIElement^>(tabsContent));
			if (panel != nullptr) {
				for (unsigned int i = 0; i < panel->Children->Size; i++) {
					auto child = panel->Children->GetAt(i);
					auto btn = dynamic_cast<Windows::UI::Xaml::Controls::Button^>(child);
					if (btn != nullptr) {
						auto content = btn->Content;
						auto text = dynamic_cast<Platform::Object^>(content);

						auto name = moonlight_xbox_dx::TabsLayout::GetTargetPanelName(btn);
						if (name != nullptr) {
							if (name == "DisplayPanel") {
								moonlight_xbox_dx::TabsLayout::SetTargetPanel(btn, this->DisplayPanel);
							}
							else if (name == "AudioPanel") {
								moonlight_xbox_dx::TabsLayout::SetTargetPanel(btn, this->AudioPanel);
							}
							else if (name == "StreamPanel") {
								moonlight_xbox_dx::TabsLayout::SetTargetPanel(btn, this->StreamPanel);
							}
							else if (name == "DetailsPanel") {
								moonlight_xbox_dx::TabsLayout::SetTargetPanel(btn, this->DetailsPanel);
							}
							else if (name == "PersonalizationPanel") {
								moonlight_xbox_dx::TabsLayout::SetTargetPanel(btn, this->PersonalizationPanel);
							}
						}
					}
				}
			}
		}
	}
	if (this->rightTabs != nullptr) rightTabs->FocusFirstTab();
}

void HostSettingsPage::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_cookie;
}

int HostSettingsPage::getDefaultBitrate(int width, int height, int fps)
{

    float frameRateFactor = (fps <= 60 ? fps : (std::sqrtf(fps / 60.f) * 60.f)) / 30.f;

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

    float resolutionFactor;
    int pixels = width * height;
    for (int i = 0;; i++) {
        if (pixels == resTable[i].pixels) {

            resolutionFactor = resTable[i].factor;
            break;
        }
        else if (pixels < resTable[i].pixels) {
            if (i == 0) {

                resolutionFactor = resTable[i].factor;
            }
            else {

                resolutionFactor = ((float)(pixels - resTable[i-1].pixels) / (resTable[i].pixels - resTable[i-1].pixels)) * (resTable[i].factor - resTable[i-1].factor) + resTable[i-1].factor;
            }
            break;
        }
        else if (resTable[i].pixels == -1) {

            resolutionFactor = resTable[i-1].factor;
            break;
        }
    }

    return std::lround(resolutionFactor * frameRateFactor) * 1000;
}

void HostSettingsPage::UpdateBackgroundSettingsContent(Platform::String^ key)
{
    if (BackgroundSettingsContent == nullptr) return;
    Windows::UI::Xaml::UIElement^ ctrl = nullptr;
    if (key != nullptr) {
        if (key->Equals(L"particles")) {
            auto c = ref new ParticleSettingsControl();
            c->Initialize(BackgroundHost);
            ctrl = c;
        } else if (key->Equals(L"streaks")) {
            auto c = ref new StreaksSettingsControl();
            c->Initialize(BackgroundHost);
            ctrl = c;
        } else if (key->Equals(L"spheres")) {
            auto c = ref new SpheresSettingsControl();
            c->Initialize(BackgroundHost);
            ctrl = c;
        } else if (key->Equals(L"blobs")) {
            auto c = ref new BlobsSettingsControl();
            c->Initialize(BackgroundHost);
            ctrl = c;
        } else if (key->Equals(L"orbs")) {
            auto c = ref new OrbsSettingsControl();
            c->Initialize(BackgroundHost);
            ctrl = c;
        }
    }
    BackgroundSettingsContent->Content = ctrl;
}

void HostSettingsPage::ClearAppImageCacheButton_Click(Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^)
{
    ClearAppImageCacheButton->IsEnabled = false;
    auto host = this->host;
    Platform::WeakReference weakThis(this);
    Concurrency::create_task([host]() {
        if (host == nullptr || host->InstanceId == nullptr || host->InstanceId->IsEmpty()) return;
        std::wstring dir = std::wstring(
            Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data())
            + L"\\images\\" + host->InstanceId->Data() + L"\\";
        std::wstring pattern = dir + L"*.png";
        WIN32_FIND_DATA fd;
        HANDLE h = FindFirstFile(pattern.c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    DeleteFile((dir + fd.cFileName).c_str());
            } while (FindNextFile(h, &fd));
            FindClose(h);
        }
        if (host != nullptr) {
            try { host->UpdateApps(); } catch (...) {}
        }
    }).then([weakThis]() {
        DISPATCH_UI([weakThis]() {
            auto that = weakThis.Resolve<HostSettingsPage>();
            if (that != nullptr) {
                try { that->ClearAppImageCacheButton->IsEnabled = true; } catch (...) {}
                ShowToast(L"App image cache cleared");
            }
        });
    });
}
