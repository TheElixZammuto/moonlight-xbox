//
// MoonlightSettings.xaml.cpp
// Implementazione della classe MoonlightSettings
//

#include "pch.h"
#include "MoonlightSettings.xaml.h"
#include "MoonlightWelcome.xaml.h"
#include "Utils.hpp"
#include "Keyboard/KeyboardCommon.h"
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
	state->ApplyBackgroundTo(this);
	auto item = ref new ComboBoxItem();
	item->Content = "Don't autoconnect";
	item->DataContext = "";
	HostSelector->Items->Append(item);
	
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
	int k = 0;
	for (auto l : keyboardLayouts) {
		auto item = ref new ComboBoxItem();
		auto s = Utils::StringFromStdString(l.first);
		item->Content = s;
		item->DataContext = s;
		KeyboardLayoutSelector->Items->Append(item);
		if (state->KeyboardLayout->Equals(s)) {
			KeyboardLayoutSelector->SelectedIndex = k;
		}
		k++;
	}
	// App grid tile size selector (Small / Medium / Large -> index maps to TileSize).
	const wchar_t* tileSizeLabels[] = { L"Small", L"Medium", L"Large" };
	for (auto label : tileSizeLabels) {
		auto tItem = ref new ComboBoxItem();
		tItem->Content = ref new Platform::String(label);
		TileSizeSelector->Items->Append(tItem);
	}
	TileSizeSelector->SelectedIndex = std::max(0, std::min(state->TileSize, 2));

	// Background selector. DataContext holds the persisted value; Content is the label.
	struct BackgroundOption { const wchar_t* label; const wchar_t* value; };
	BackgroundOption backgroundOptions[] = {
		{ L"System default", L"System" },
		{ L"Navy", L"Navy" },
		{ L"Black", L"Black" }
	};
	int bgIndex = 0, bgSelected = 1; // default to Navy
	for (auto opt : backgroundOptions) {
		auto bItem = ref new ComboBoxItem();
		bItem->Content = ref new Platform::String(opt.label);
		bItem->DataContext = ref new Platform::String(opt.value);
		BackgroundSelector->Items->Append(bItem);
		if (state->BackgroundTheme != nullptr && state->BackgroundTheme->Equals(ref new Platform::String(opt.value))) {
			bgSelected = bgIndex;
		}
		bgIndex++;
	}
	BackgroundSelector->SelectedIndex = bgSelected;

	this->Loaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MoonlightSettings::OnLoaded);
	this->Unloaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MoonlightSettings::OnUnloaded);
}

void MoonlightSettings::backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
}

void MoonlightSettings::HostSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	ComboBoxItem^ item = (ComboBoxItem^)this->HostSelector->SelectedItem;

	auto s = Utils::PlatformStringToStdString(item->DataContext->ToString());
	state->autostartInstance = s;

}

void MoonlightSettings::OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args)
{
	// UWP on Xbox One triggers a back request whenever the B
	// button is pressed which can result in the app being
	// suspended if unhandled
	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
	args->Handled = true;

}

void MoonlightSettings::WelcomeButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightWelcome::typeid));
}

void MoonlightSettings::LayoutSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	ComboBoxItem^ item = (ComboBoxItem^)this->KeyboardLayoutSelector->SelectedItem;

	auto s = item->DataContext->ToString();
	state->KeyboardLayout = s;
}

void MoonlightSettings::TileSizeSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	int index = this->TileSizeSelector->SelectedIndex;
	if (index < 0) return;
	state->TileSize = index;
}

void MoonlightSettings::BackgroundSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	ComboBoxItem^ item = (ComboBoxItem^)this->BackgroundSelector->SelectedItem;
	if (item == nullptr) return;
	state->BackgroundTheme = item->DataContext->ToString();
	// Live preview without navigating away.
	state->ApplyBackgroundTo(this);
}

void MoonlightSettings::OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	m_back_cookie = navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &MoonlightSettings::OnBackRequested);
}


void MoonlightSettings::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_cookie;
}

void MoonlightSettings::MarginSlider_GotFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	// Only show the calibration border while actively adjusting a margin slider.
	CalibrationBorderPanel->BorderBrush = ref new SolidColorBrush(Windows::UI::Colors::Green);
}

void MoonlightSettings::MarginSlider_LostFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	CalibrationBorderPanel->BorderBrush = ref new SolidColorBrush(Windows::UI::Colors::Transparent);
}
