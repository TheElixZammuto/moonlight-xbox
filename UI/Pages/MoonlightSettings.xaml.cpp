
#include "pch.h"
#include "UI\Pages\MoonlightSettings.xaml.h"
#include "UI\Backgrounds\DynamicBackgroundHost.xaml.h"
#include "UI\Controls\TabsLayout.xaml.h"
#include "UI\Pages\MoonlightWelcome.xaml.h"
#include "UI\Utilities\XamlHelpers.h"
#define MLOG_TAG_OVERRIDE "MoonlightSettings"
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

MoonlightSettings::MoonlightSettings()
{
	InitializeComponent();
	state = GetApplicationState();

	auto item = ref new ComboBoxItem();
	item->Content = "Do not auto-connect";
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
	{

		static const wchar_t* logLevelLabels[] = { L"All", L"Verbose", L"Debug", L"Info", L"Warning", L"Error" };
		for (auto label : logLevelLabels) {
			auto item = ref new ComboBoxItem();
			item->Content = ref new Platform::String(label);
			LogLevelFilterSelector->Items->Append(item);
		}
		LogLevelFilterSelector->SelectedIndex = 0;
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
	this->Loaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MoonlightSettings::OnLoaded);
	this->Unloaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MoonlightSettings::OnUnloaded);
}

void MoonlightSettings::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	try {
		if (BackgroundHost != nullptr) {
			BackgroundHost->SetHosts(state->SavedHosts);
			BackgroundHost->Refresh();
			BackgroundHost->StartAnimations();
		}
	} catch (...) {}

	auto color = GetAppliedAccentColor();

	try {
		ApplyAccentColor(color);
		auto cur = this->ActualTheme;
		this->RequestedTheme = (cur != ElementTheme::Dark) ? ElementTheme::Dark : ElementTheme::Light;
		this->RequestedTheme = ElementTheme::Default;
	} catch (Platform::Exception^ ex) {
		MLOGF(Utils::LogLevel::Error, "OnNavigatedTo: ApplyAccentColor/RequestedTheme toggle threw: %ws", ex->Message->Data());
	} catch (...) {
		MLOG(Utils::LogLevel::Error, "OnNavigatedTo: ApplyAccentColor/RequestedTheme toggle threw unknown exception");
	}

	try {
		if (tabsLayout != nullptr) tabsLayout->AccentColor = color;

		auto brush = ref new SolidColorBrush(color);
		if (GeneralHeader != nullptr) GeneralHeader->Foreground = brush;
		if (DisplayHeader != nullptr) DisplayHeader->Foreground = brush;
		if (MouseHeader != nullptr) MouseHeader->Foreground = brush;
		if (KeyboardHeader != nullptr) KeyboardHeader->Foreground = brush;
		if (LogsHeader != nullptr) LogsHeader->Foreground = brush;
	} catch (Platform::Exception^ ex) {
		MLOGF(Utils::LogLevel::Error, "OnNavigatedTo: header Foreground assignment threw: %ws", ex->Message->Data());
	} catch (...) {
		MLOG(Utils::LogLevel::Error, "OnNavigatedTo: header Foreground assignment threw unknown exception");
	}
}

void MoonlightSettings::OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	try {
		if (BackgroundHost != nullptr) BackgroundHost->StopAnimations();
	} catch (...) {}
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

	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
	args->Handled = true;

}

void MoonlightSettings::WelcomeButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto slideForward = ref new Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionInfo();
	slideForward->Effect = Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionEffect::FromRight;
	this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightWelcome::typeid), nullptr, slideForward);
}

void MoonlightSettings::LayoutSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	ComboBoxItem^ item = (ComboBoxItem^)this->KeyboardLayoutSelector->SelectedItem;

	auto s = item->DataContext->ToString();
	state->KeyboardLayout = s;
}

void MoonlightSettings::OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	m_back_cookie = navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &MoonlightSettings::OnBackRequested);
	ScreenMarginPreviewBorder->Margin = state->ScreenMargin;
	if (tabsLayout != nullptr) tabsLayout->FocusFirstTab();
	RefreshLogView();
}

void MoonlightSettings::OnMarginSliderChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e)
{
	ScreenMarginPreviewBorder->Margin = state->ScreenMargin;
}

void MoonlightSettings::OnMarginSliderGotFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	m_marginSliderFocused = true;
	ShowMarginPreview->Begin();
}

void MoonlightSettings::OnMarginSliderLostFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	m_marginSliderFocused = false;
	this->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Low,
		ref new Windows::UI::Core::DispatchedHandler([this]() {
			if (!m_marginSliderFocused) HideMarginPreview->Begin();
		}));
}

void MoonlightSettings::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_cookie;
}

void MoonlightSettings::LogViewTab_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto btn = safe_cast<Windows::UI::Xaml::Controls::Button^>(sender);
	m_showingLastRunLog = (btn == LastRunLogTab);
	RefreshLogView();
}

void MoonlightSettings::RefreshLogButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	RefreshLogView();
}

void MoonlightSettings::LogLevelFilterSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e)
{
	m_minLogLevel = LogLevelFilterSelector->SelectedIndex - 1;
	RefreshLogView();
}

static bool IsDescendantOf(DependencyObject^ element, DependencyObject^ ancestor)
{
	while (element != nullptr) {
		if (element == ancestor) return true;
		element = VisualTreeHelper::GetParent(element);
	}
	return false;
}

void MoonlightSettings::LogListView_LostFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	if (LogListView == nullptr) return;
	auto focused = dynamic_cast<DependencyObject^>(FocusManager::GetFocusedElement());
	if (IsDescendantOf(focused, LogListView)) return;
	ScrollLogViewToBottom();
}

void MoonlightSettings::LogListView_KeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
	using namespace Windows::System;
	if (LogListView == nullptr) return;

	auto lines = dynamic_cast<IVector<LogLineItem^>^>(LogListView->ItemsSource);
	if (lines == nullptr || lines->Size == 0) return;

	int targetIndex;
	if (e->Key == VirtualKey::GamepadLeftShoulder) targetIndex = 0;
	else if (e->Key == VirtualKey::GamepadRightShoulder) targetIndex = lines->Size - 1;
	else return;

	auto item = lines->GetAt(targetIndex);
	LogListView->ScrollIntoView(item);
	LogListView->UpdateLayout();
	auto container = dynamic_cast<ListViewItem^>(LogListView->ContainerFromItem(item));
	if (container != nullptr) container->Focus(Windows::UI::Xaml::FocusState::Keyboard);

	e->Handled = true;
}

void MoonlightSettings::ScrollLogViewToBottom()
{
	if (LogListView == nullptr) return;
	auto lines = dynamic_cast<IVector<LogLineItem^>^>(LogListView->ItemsSource);
	if (lines == nullptr || lines->Size == 0) return;
	LogListView->UpdateLayout();
	LogListView->ScrollIntoView(lines->GetAt(lines->Size - 1));
}

void MoonlightSettings::RefreshLogView()
{
	if (CurrentRunLogTab != nullptr) CurrentRunLogTab->Opacity = m_showingLastRunLog ? 0.6 : 1.0;
	if (LastRunLogTab != nullptr) LastRunLogTab->Opacity = m_showingLastRunLog ? 1.0 : 0.6;
	if (LogListView == nullptr) return;

	auto text = m_showingLastRunLog
		? Utils::ReadLastRunLogText()
		: Utils::ReadCurrentRunLogText();

	auto lines = ref new Platform::Collections::Vector<LogLineItem^>();
	auto errorBrush = ref new SolidColorBrush(Windows::UI::Colors::Red);
	auto warningBrush = ref new SolidColorBrush(Windows::UI::Colors::Orange);
	auto defaultBrush = ref new SolidColorBrush(Windows::UI::Colors::White);
	std::wstring content(text->Data());
	size_t start = 0;
	while (start <= content.size()) {
		size_t nl = content.find(L'\n', start);
		size_t end = (nl == std::wstring::npos) ? content.size() : nl;
		std::wstring line = content.substr(start, end - start);
		if (!line.empty() && line.back() == L'\r') line.pop_back();
		auto level = Utils::ParseLogLevelFromLine(line);
		if (m_minLogLevel == -1 || static_cast<int>(level) >= m_minLogLevel) {
			auto entry = ref new LogLineItem();
			entry->Text = ref new Platform::String(line.c_str());
			entry->Level = static_cast<int>(level);
			entry->Foreground = (level == Utils::LogLevel::Error) ? errorBrush
				: (level == Utils::LogLevel::Warning) ? warningBrush
				: defaultBrush;
			lines->Append(entry);
		}
		if (nl == std::wstring::npos) break;
		start = nl + 1;
	}

	LogListView->ItemsSource = lines;
	ScrollLogViewToBottom();
}
