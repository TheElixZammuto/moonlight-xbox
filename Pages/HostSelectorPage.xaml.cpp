//
// HostSelectorPage.xaml.cpp
// Implementation of the HostSelectorPage class
//

#include "pch.h"
#include "HostSelectorPage.xaml.h"
#include "AppPage.xaml.h"
#include <State\MoonlightClient.h>
#include "HostSettingsPage.xaml.h"
#include "Utils.hpp"
#include "MoonlightSettings.xaml.h"
#include "State\MDNSHandler.h"

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


// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=234238

HostSelectorPage::HostSelectorPage()
{
	state = GetApplicationState();
	InitializeComponent();
}


void moonlight_xbox_dx::HostSelectorPage::NewHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	dialogHostnameTextBox = ref new TextBox();
	dialogHostnameTextBox->AcceptsReturn = false;
	dialogHostnameTextBox->KeyDown += ref new Windows::UI::Xaml::Input::KeyEventHandler(this, &moonlight_xbox_dx::HostSelectorPage::OnKeyDown);
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
	sender->IsPrimaryButtonEnabled = false;
	Platform::String^ hostname = dialogHostnameTextBox->Text;
	auto def = args->GetDeferral();
	Concurrency::create_task([def,hostname,this,args,sender]() {
		bool status = state->AddHost(hostname);
		if (!status) {
			Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([sender, this,hostname,def,args]() {
				args->Cancel = true;
				sender->Content = L"Failed to Connect to " + hostname;
				def->Complete();
			}));
			return;
		}
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([def]() {
			def->Complete();
		}));
	});
}


void moonlight_xbox_dx::HostSelectorPage::GridView_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e)
{
	MoonlightHost^ host = (MoonlightHost^)e->ClickedItem;
	this->Connect(host);
}

void moonlight_xbox_dx::HostSelectorPage::StartPairing(MoonlightHost^ host) {
	MoonlightClient* client = new MoonlightClient();
	char ipAddressStr[2048];
	wcstombs_s(NULL, ipAddressStr, host->LastHostname->Data(), 2047);
	int status = client->Connect(ipAddressStr);
	if (status != 0)return;
	char* pin = client->GeneratePIN();
	ContentDialog^ dialog = ref new ContentDialog();
	wchar_t msg[4096];
	swprintf(msg, 4096, L"We need to pair the host before continuing. Type %S on your host to continue", pin);
	dialog->Content = ref new Platform::String(msg);
	dialog->PrimaryButtonText = "Ok";
	dialog->ShowAsync();
	Concurrency::create_task([dialog, host, client, pin]() {
		int a = client->Pair();
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([a,dialog, host]()
			{
				if (a == 0) {
					dialog->Hide();
				}
				else {
					//dialog->Content = Utils::StringFromStdString(std::string(gs_error));
				}
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
	//Thanks to https://stackoverflow.com/questions/62878368/uwp-gridview-listview-get-the-righttapped-item-supporting-both-mouse-and-xbox-co
	FrameworkElement^ senderElement = (FrameworkElement^)e->OriginalSource;

	if (senderElement->GetType()->FullName->Equals(GridViewItem::typeid->FullName)) {
		auto gi = (GridViewItem^)senderElement;
		currentHost = (MoonlightHost^)(gi->Content);
	}
	else {
		currentHost = (MoonlightHost^)(senderElement->DataContext);
	}
	if (currentHost == nullptr) {
		return;
	}
	this->ActionsFlyout->ShowAt(senderElement);
}

void moonlight_xbox_dx::HostSelectorPage::hostSettingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSettingsPage::typeid), currentHost);
}


void moonlight_xbox_dx::HostSelectorPage::SettingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightSettings::typeid));
}

void moonlight_xbox_dx::HostSelectorPage::OnStateLoaded() {
	for (auto a : GetApplicationState()->SavedHosts) {
		a->UpdateStats();
	}
	if (state->autostartInstance.size() > 0) {
		auto pii = Utils::StringFromStdString(state->autostartInstance);
		for (int i = 0; i < state->SavedHosts->Size; i++) {
			auto host = state->SavedHosts->GetAt(i);
			if (host->InstanceId->Equals(pii)) {
				auto that = this;
				Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([that, host]() {
						that->Connect(host);
				}));
				break;
			}
		}
	}
}

void moonlight_xbox_dx::HostSelectorPage::Connect(MoonlightHost^ host) {
	if (!host->Connected)return;
	if (!host->Paired) {
		StartPairing(host);
		return;
	}
	state->shouldAutoConnect = true;
	bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(AppPage::typeid), host);
	if (result) {
		continueFetch = false;
	}
}


void HostSelectorPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseVisible);
	continueFetch = true;
	Concurrency::create_task([this] {
		int a = init_mdns();
		while (continueFetch) {
			if (a != 0)query_mdns(a);
			for (auto a : GetApplicationState()->SavedHosts) {
				a->UpdateStats();
			}
			Sleep(5000);
		}
		});
}

void moonlight_xbox_dx::HostSelectorPage::OnKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
	if (e->Key == Windows::System::VirtualKey::Enter) {
		CoreInputView::GetForCurrentView()->TryHide();
	}
}
