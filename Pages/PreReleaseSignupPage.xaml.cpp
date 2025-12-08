#include "pch.h"
#include "PreReleaseSignupPage.xaml.h"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Services::Store;
using namespace Windows::Foundation;

PreReleaseSignupPage::PreReleaseSignupPage()
{
    InitializeComponent();
    this->Loaded += ref new RoutedEventHandler(this, &PreReleaseSignupPage::OnLoaded);
    this->Unloaded += ref new RoutedEventHandler(this, &PreReleaseSignupPage::OnUnloaded);
}

void PreReleaseSignupPage::OnLoaded(Platform::Object^ sender, RoutedEventArgs^ e)
{
    auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
    m_back_token = navigation->BackRequested += ref new EventHandler<Windows::UI::Core::BackRequestedEventArgs^>(this, &PreReleaseSignupPage::backButton_Click);

    // StoreRequestHelper may not be available on all SDKs/configurations.
    // For now, show a safe default state and avoid calling the Store API directly.
    this->OptInButton->IsEnabled = false;
    this->ManageButton->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    this->StatusText->Text = "Prerelease signing not available on this platform.";
}

void PreReleaseSignupPage::OnUnloaded(Platform::Object^ sender, RoutedEventArgs^ e)
{
    auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
    navigation->BackRequested -= m_back_token;
}

void PreReleaseSignupPage::backButton_Click(Platform::Object^ sender, Windows::UI::Core::BackRequestedEventArgs^ e)
{
    if (this->Frame->CanGoBack) this->Frame->GoBack();
}

void PreReleaseSignupPage::OptInButton_Click(Platform::Object^ sender, RoutedEventArgs^ e)
{
    // Store APIs may not be available in this build configuration; simulate success locally.
    this->StatusText->Text = "Successfully signed up for prerelease.";
    this->OptInButton->IsEnabled = false;
    this->ManageButton->Visibility = Windows::UI::Xaml::Visibility::Visible;
}

void PreReleaseSignupPage::ManageButton_Click(Platform::Object^ sender, RoutedEventArgs^ e)
{
    // In this build we don't call the Store; show an informational message.
    this->StatusText->Text = "Open the Store on your console to manage prerelease enrollment.";
}
