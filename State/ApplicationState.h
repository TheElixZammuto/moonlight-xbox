#pragma once
#include "State\MoonlightHost.h"
#include <ppltasks.h>
namespace moonlight_xbox_dx {
	public ref class ApplicationState sealed
	{
	private:
		Windows::Foundation::Collections::IVector<MoonlightHost^>^ hosts;
	internal:
		Concurrency::task<void> Init();
	public:
		ApplicationState()
		{
			MoonlightHost ^host = ref new MoonlightHost();
			host->CurrentlyRunningAppId = 0;
			host->InstanceId = L"1234";
			host->LastHostname = L"10.1.0.1";
			host->Paired = false;
			SavedHosts->Append(host);
			host = ref new MoonlightHost();
			host->CurrentlyRunningAppId = 0;
			host->InstanceId = L"1234";
			host->LastHostname = L"10.1.0.10";
			host->Paired = false;
			SavedHosts->Append(host);
			host = ref new MoonlightHost();
			host->CurrentlyRunningAppId = 0;
			host->InstanceId = L"1234";
			host->LastHostname = L"10.1.0.12";
			host->Paired = false;
			SavedHosts->Append(host);
			host = ref new MoonlightHost();
			host->CurrentlyRunningAppId = 0;
			host->InstanceId = L"1234";
			host->LastHostname = L"10.1.0.13";
			host->Paired = false;
			SavedHosts->Append(host);
			Init();
		}
		property Windows::Foundation::Collections::IVector<MoonlightHost^>^ SavedHosts
		{
			Windows::Foundation::Collections::IVector<MoonlightHost^>^ get()
			{
				if (this->hosts == nullptr)
				{
					this->hosts = ref new Platform::Collections::Vector<MoonlightHost^>();
				}
				return this->hosts;
			};
		}
	};
}