#include "pch.h"
#include "MoonlightHost.h"
#include "State\MoonlightClient.h"
#include <nlohmann/json.hpp>
#include <Utils.hpp>
#include <algorithm>
#include <vector>

namespace moonlight_xbox_dx {
	MoonlightHost::MoonlightHost(Platform::String ^host) {
		lastHostname = host;
		resolution = ref new ScreenResolution(1920, 1080);
		loading = true;

		if (framePacing == "") {
			// default initial value based on system type
			framePacing = IsXboxOne() ? "Display-locked" : "Immediate";
		}
	}

	void MoonlightHost::UpdateHostInfo(bool showLoading) {
		if (showLoading) this->Loading = true;
		if (client == nullptr) {
			client = new MoonlightClient();
		}
		// Held across the whole connect+read sequence so a concurrent Connect() from another
		// thread (e.g. HostSelectorPage's poll loop) can't mutate serverData/hostname mid-read.
		auto lock = client->LockState();
		bool status = this->Connect() == 0;
		this->Connected = status;
		if (status) {
			this->Paired = client->IsPaired();
		    this->CurrentlyRunningAppId = client->GetRunningAppID();
			this->InstanceId = client->GetInstanceID();
			if (this->Connected) {
				this->ComputerName = client->GetComputerName();
				this->ServerAddress = client->GetServerAddress();
				this->MacAddress = client->GetServerMacAddress();
			}
		}
		if (showLoading) this->Loading = false;
	}

	int MoonlightHost::Connect()
	{
		if (client == nullptr) {
			client = new MoonlightClient();
		}
		Platform::String^ ipAddress = this->lastHostname;
		char ipAddressStr[2048];
		wcstombs_s(NULL, ipAddressStr, ipAddress->Data(), 2047);
		return client->Connect(ipAddressStr);
	}

	void MoonlightHost::UpdateApps() {
		// Same rationale as UpdateHostInfo: hold the lock across the read so a concurrent
		// Connect() elsewhere can't mutate serverData mid-fetch.
		auto lock = client->LockState();
	    auto apps = client->GetApplications();
	    Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([this, apps]() {
			Apps->Clear();
			for (auto a : apps) {
				if (a->Id == CurrentlyRunningAppId) a->CurrentlyRunning = true;
				Apps->Append(a);
			}
			ApplyFavoritesToApps();
		}));
    }

	bool MoonlightHost::IsAppFavorite(int appId) {
		return favoriteOrders.find(appId) != favoriteOrders.end();
	}

	void MoonlightHost::SetFavorite(int appId, bool favorite) {
		if (favorite) {
			if (favoriteOrders.find(appId) == favoriteOrders.end()) {
				int maxOrder = -1;
				for (auto& kv : favoriteOrders) maxOrder = std::max(maxOrder, kv.second);
				favoriteOrders[appId] = maxOrder + 1;
			}
		} else {
			favoriteOrders.erase(appId);
		}
		ApplyFavoritesToApps();
	}

	void MoonlightHost::ApplyFavoritesToApps() {
		if (this->apps == nullptr) return;
		std::vector<MoonlightApp^> favs;
		std::vector<MoonlightApp^> rest;
		for (unsigned int i = 0; i < apps->Size; ++i) {
			auto a = apps->GetAt(i);
			auto it = favoriteOrders.find(a->Id);
			if (it != favoriteOrders.end()) {
				a->IsFavorite = true;
				a->SortOrder = it->second;
				favs.push_back(a);
			} else {
				a->IsFavorite = false;
				a->SortOrder = -1;
				rest.push_back(a);
			}
		}
		if (favs.empty()) return; // nothing to reorder, keep host-provided ordering as-is
		std::sort(favs.begin(), favs.end(), [](MoonlightApp^ l, MoonlightApp^ r) { return l->SortOrder < r->SortOrder; });
		apps->Clear();
		for (auto a : favs) apps->Append(a);
		for (auto a : rest) apps->Append(a);
	}

	void MoonlightHost::MoveFavorite(int appId, int direction) {
		auto it = favoriteOrders.find(appId);
		if (it == favoriteOrders.end()) return; // not a favorite, nothing to reorder
		int currentOrder = it->second;
		int targetOrder = currentOrder + direction;
		int swapAppId = -1;
		for (auto& kv : favoriteOrders) {
			if (kv.second == targetOrder) { swapAppId = kv.first; break; }
		}
		if (swapAppId == -1) return; // already at the first/last favorite position
		favoriteOrders[swapAppId] = currentOrder;
		favoriteOrders[appId] = targetOrder;
		ApplyFavoritesToApps();
	}

	Platform::String^ MoonlightHost::SerializeFavorites() {
		nlohmann::json j = nlohmann::json::object();
		for (auto& kv : favoriteOrders) {
			j[std::to_string(kv.first)] = kv.second;
		}
		return Utils::StringFromStdString(j.dump());
	}

	void MoonlightHost::DeserializeFavorites(Platform::String^ json) {
		favoriteOrders.clear();
		if (json != nullptr && json->Length() > 0) {
			try {
				std::string s = Utils::PlatformStringToStdString(json);
				nlohmann::json j = nlohmann::json::parse(s);
				for (auto it = j.begin(); it != j.end(); ++it) {
					favoriteOrders[std::stoi(it.key())] = it.value().get<int>();
				}
			} catch (...) {
				favoriteOrders.clear();
			}
		}
		ApplyFavoritesToApps();
	}

	void MoonlightHost::UpdateAppRunningStates() {
		try {
			UpdateHostInfo(false);
		} catch (...) { }

		int runningId = this->CurrentlyRunningAppId;

		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
			Windows::UI::Core::CoreDispatcherPriority::High,
			ref new Windows::UI::Core::DispatchedHandler([this, runningId]() {
				for (unsigned int i = 0; i < Apps->Size; ++i) {
					auto existing = Apps->GetAt(i);
					bool shouldRun = (existing->Id == runningId);
					if (existing->CurrentlyRunning != shouldRun) existing->CurrentlyRunning = shouldRun;
				}
			}));
	}

	void MoonlightHost::Unpair()
	{
		client->Unpair();
	}


	void MoonlightHost::OnPropertyChanged(Platform::String^ propertyName)
	{
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([this, propertyName]() {
			PropertyChanged(this, ref new  Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
		}));
	}
}
