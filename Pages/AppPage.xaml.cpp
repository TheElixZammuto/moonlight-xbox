//
// AppPage.xaml.cpp
// Implementation of the AppPage class
//

#include "pch.h"
#include "AppPage.xaml.h"
#include "Client\MoonlightClient.h"
#include "StreamPage.xaml.h"
#include "HostSettingsPage.xaml.h"

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

AppPage::AppPage()
{
	InitializeComponent();
}

void AppPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	MoonlightHost^ mhost = dynamic_cast<MoonlightHost^>(e->Parameter);
	if (mhost == nullptr)return;
	host = mhost;
	UpdateApps();
}

void AppPage::UpdateApps() {
	auto that = this;
	Concurrency::create_async([that]() {
		auto client = new MoonlightClient();
		char ipAddressStr[2048];
		wcstombs_s(NULL, ipAddressStr, that->Host->LastHostname->Data(), 2047);
		int status = client->Connect(ipAddressStr);
		if (status != 0)return;
		that->Host->UpdateStats();
		auto apps = client->GetApplications();
		
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
			Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([that, apps]() {
				that->Apps->Clear();
				for (auto a : apps) {
					if (a->Id == that->Host->CurrentlyRunningAppId)a->CurrentlyRunning = true;
					that->Apps->Append(a);
				}
			}));
		});
}

void moonlight_xbox_dx::AppPage::AppsGrid_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e)
{
	MoonlightApp^ app = (MoonlightApp^)e->ClickedItem;
	this->Connect(app);
}

void AppPage::Connect(MoonlightApp^ app) {
	StreamConfiguration^ config = ref new StreamConfiguration();
	config->hostname = host->LastHostname;
	config->appID = app->Id;
	config->width = host->Width;
	config->height = host->Height;
	bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(StreamPage::typeid), config);
}


void moonlight_xbox_dx::AppPage::HostsGrid_RightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e)
{
	FrameworkElement^ senderElement = (FrameworkElement^)e->OriginalSource;
	currentApp = (MoonlightApp^)senderElement->DataContext;
	this->ActionsFlyout->ShowAt(senderElement);
}


void moonlight_xbox_dx::AppPage::resumeAppButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->Connect(this->currentApp);
}


void moonlight_xbox_dx::AppPage::closeAppButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto client = new MoonlightClient();
	char ipAddressStr[2048];
	wcstombs_s(NULL, ipAddressStr, Host->LastHostname->Data(), 2047);
	Concurrency::create_async([client, this, ipAddressStr]() {
		int status = client->Connect(ipAddressStr);
		if (status == 0)client->StopApp();
		UpdateApps();
	});
}


void moonlight_xbox_dx::AppPage::backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->Frame->GoBack();
}


void moonlight_xbox_dx::AppPage::settingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSettingsPage::typeid), Host);
}
