//
// AppPage.xaml.cpp
// Implementation of the AppPage class
//

#include "pch.h"
#include "AppPage.xaml.h"
#include "Client\MoonlightClient.h"
#include "StreamPage.xaml.h"

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
	auto that = this;
	Concurrency::create_async([that,mhost]() {
		auto client = new MoonlightClient();
		char ipAddressStr[2048];
		wcstombs_s(NULL, ipAddressStr, mhost->LastHostname->Data(), 2047);
		int status = client->Connect(ipAddressStr);
		if (status != 0)return;
		auto apps = client->GetApplications();
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
			Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([that,apps]() {
				for (auto a : apps) {
					that->Apps->Append(a);
				}
			}));
		});
}

void moonlight_xbox_dx::AppPage::AppsGrid_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e)
{
	MoonlightApp^ app = (MoonlightApp^)e->ClickedItem;
	StreamConfiguration^ config = ref new StreamConfiguration();
	config->hostname = host->LastHostname;
	config->appID = app->Id;
	config->width = 1280;
	config->height = 720;
	bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(StreamPage::typeid), config);
}
