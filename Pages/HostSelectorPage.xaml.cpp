//
// HostSelectorPage.xaml.cpp
// Implementation of the HostSelectorPage class
//

#include "pch.h"
#include "HostSelectorPage.xaml.h"

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

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=234238

HostSelectorPage::HostSelectorPage()
{
	InitializeComponent();
	state = ref new ApplicationState();
}


void moonlight_xbox_dx::HostSelectorPage::NewHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    dialogHostnameTextBox = ref new TextBox();
    dialogHostnameTextBox->AcceptsReturn = false;
    ContentDialog ^dialog = ref new ContentDialog();
    dialog->Content = dialogHostnameTextBox;
    dialog->Title = L"Add new Host";
    dialog->IsSecondaryButtonEnabled = true;
    dialog->PrimaryButtonText = "Ok";
    dialog->SecondaryButtonText = "Cancel";
    dialog->ShowAsync();
    dialog->PrimaryButtonClick += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Xaml::Controls::ContentDialog^, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs^>(this, &moonlight_xbox_dx::HostSelectorPage::OnNewHostDialogPrimaryClick);
}


void moonlight_xbox_dx::HostSelectorPage::OnNewHostDialogPrimaryClick(Windows::UI::Xaml::Controls::ContentDialog^ sender, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs^ args)
{
    Platform::String ^hostname = dialogHostnameTextBox->Text;
    MoonlightHost ^host = ref new MoonlightHost();
    host->LastHostname = hostname;
    state->SavedHosts->Append(host);
    Concurrency::create_task([host]() {
        host->UpdateStats();
    });
}
