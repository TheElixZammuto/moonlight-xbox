#include "pch.h"
#include "MoonlightHost.h"
#include "Client\MoonlightClient.h"
#include <ppltasks.h>

namespace moonlight_xbox_dx {
	void MoonlightHost::UpdateStats() {
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High,ref new Windows::UI::Core::DispatchedHandler([this](){
			this->Loading = true;
		}));
		bool status = this->Connect() == 0;
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([this,status]() {
			this->Connected = status;
			this->Paired = client->IsPaired();
			this->CurrentlyRunningAppId = client->GetRunningAppID();
			this->Loading = false;
		}));
	}

	int MoonlightHost::Connect()
	{
		client = new MoonlightClient();
		Platform::String^ ipAddress = this->lastHostname;
		char ipAddressStr[2048];
		wcstombs_s(NULL, ipAddressStr, ipAddress->Data(), 2047);
		return client->Connect(ipAddressStr);
	}

	void MoonlightHost::Unpair()
	{
		client->Unpair();
	}

	
	void MoonlightHost::OnPropertyChanged(Platform::String^ propertyName)
	{
		PropertyChanged(this, ref new  Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
	}
}