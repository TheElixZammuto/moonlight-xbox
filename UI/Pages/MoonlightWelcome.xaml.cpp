
#include "pch.h"
#include "MoonlightWelcome.xaml.h"

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

MoonlightWelcome::MoonlightWelcome()
{
	InitializeComponent();
	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);

	this->Loaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MoonlightWelcome::OnLoaded);
	this->Unloaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MoonlightWelcome::OnUnloaded);
}

void MoonlightWelcome::OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args)
{

	if (this->FlipView->SelectedIndex > 0) {
		args->Handled = true;
		this->FlipView->SelectedIndex = this->FlipView->SelectedIndex - 1;
		return;
	}
	if (this->Frame->CanGoBack && !GetApplicationState()->FirstTime) {
		this->Frame->GoBack();
		args->Handled = true;
		return;
	}
}

void MoonlightWelcome::GoForward(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->FlipView->SelectedIndex = this->FlipView->SelectedIndex + 1;
}

void MoonlightWelcome::GoBack(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->FlipView->SelectedIndex = this->FlipView->SelectedIndex - 1;
}

void MoonlightWelcome::CloseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	GetApplicationState()->FirstTime = false;
	GetApplicationState()->UpdateFile();
	this->Frame->GoBack();
}

void MoonlightWelcome::Page_KeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
	if (e->OriginalKey != Windows::System::VirtualKey::GamepadA)return;
	if (this->FlipView->Items->Size == (this->FlipView->SelectedIndex + 1)) {
		GetApplicationState()->FirstTime = false;
		GetApplicationState()->UpdateFile();
		this->Frame->GoBack();
	}
	else {
		this->FlipView->SelectedIndex = this->FlipView->SelectedIndex + 1;
	}
}

void MoonlightWelcome::OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
    m_back_token = navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &MoonlightWelcome::OnBackRequested);
    BackgroundControl->StartAnimations();
}

void MoonlightWelcome::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_token;
    BackgroundControl->StopAnimations();
}
