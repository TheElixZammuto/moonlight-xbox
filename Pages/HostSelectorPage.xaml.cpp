//
// HostSelectorPage.xaml.cpp
// Implementation of the HostSelectorPage class
//

#include "pch.h"
#include "HostSelectorPage.xaml.h"
#include "AppPage.xaml.h"
#include <Client\MoonlightClient.h>

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
	state = GetApplicationState();
	auto that = this;
	state->Init();
}


void moonlight_xbox_dx::HostSelectorPage::NewHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	dialogHostnameTextBox = ref new TextBox();
	dialogHostnameTextBox->AcceptsReturn = false;
	ContentDialog^ dialog = ref new ContentDialog();
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
	Platform::String^ hostname = dialogHostnameTextBox->Text;
	MoonlightHost^ host = ref new MoonlightHost();
	host->LastHostname = hostname;
	state->SavedHosts->Append(host);
	state->UpdateFile();
	Concurrency::create_task([host]() {
		host->UpdateStats();
	});
}


void moonlight_xbox_dx::HostSelectorPage::GridView_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e)
{
	MoonlightHost^ host = (MoonlightHost^)e->ClickedItem;
	if (!host->Paired) {
		StartPairing(host);
		return;
	}
	bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(AppPage::typeid), host);
}

void moonlight_xbox_dx::HostSelectorPage::StartPairing(MoonlightHost^ host) {
	MoonlightClient* client = new MoonlightClient();
	char ipAddressStr[2048];
	wcstombs_s(NULL, ipAddressStr, host->LastHostname->Data(), 2047);
	int status = client->Connect(ipAddressStr);
	if (status != 0)return;
	char* pin = client->GeneratePIN();
	ContentDialog^ dialog = ref new ContentDialog();
	dialog->Content = "Hello";
	wchar_t msg[4096];
	swprintf(msg, 4096, L"We need to pair the host before continuing. Type %S on your host to continue", pin);
	dialog->Content = ref new Platform::String(msg);
	dialog->PrimaryButtonText = "Ok";
	dialog->ShowAsync();
	Concurrency::create_task([dialog, host, client, pin]() {
		int a = client->Pair();
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([dialog, host]()
			{
				dialog->Hide();
				host->UpdateStats();
			}
		));
	});
}



void moonlight_xbox_dx::HostSelectorPage::removeHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	State->RemoveHost(currentHost);
}


void moonlight_xbox_dx::HostSelectorPage::HostsGrid_RightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e)
{
	FrameworkElement^ senderElement = (FrameworkElement^)e->OriginalSource;
	currentHost = (MoonlightHost^)senderElement->DataContext;
	this->ActionsFlyout->ShowAt(senderElement);
}
