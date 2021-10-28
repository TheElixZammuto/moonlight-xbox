#include "pch.h"
#include "ApplicationState.h"
#include <Utils.hpp>
#include <nlohmann/json.hpp>

using namespace Windows::Storage;
Concurrency::task<void> moonlight_xbox_dx::ApplicationState::Init()
{
	auto that = this;	
	StorageFolder^ storageFolder = ApplicationData::Current->LocalFolder;
	return concurrency::create_task(storageFolder->CreateFileAsync("state.json", CreationCollisionOption::OpenIfExists)).then([](StorageFile^ file) {
		moonlight_xbox_dx::Utils::Log("ciao");
		return FileIO::ReadTextAsync(file);
	}).then([this](Concurrency::task<Platform::String^> jsonTask) {
			Platform::String ^jsonFile = jsonTask.get();
			if (jsonFile != nullptr && jsonFile->Length() > 0) {
				nlohmann::json stateJson = nlohmann::json::parse(jsonFile);
				for (auto a : stateJson["hosts"]) {
					MoonlightHost^ h = ref new MoonlightHost();
					h->LastHostname = Utils::StringFromStdString(a["hostname"].get<std::string>());
					this->SavedHosts->Append(h);
				}
				concurrency::create_task([this]() {
					for (auto a : this->SavedHosts) {
						a->UpdateStats();
					}
					});
			}
	});
}

Concurrency::task<void> moonlight_xbox_dx::ApplicationState::UpdateFile()
{
	auto that = this;
	StorageFolder^ storageFolder = ApplicationData::Current->LocalFolder;
	return concurrency::create_task(storageFolder->CreateFileAsync("state.json", CreationCollisionOption::OpenIfExists)).then([that](StorageFile^ file) {
		nlohmann::json stateJson;
		stateJson["hosts"] = nlohmann::json::array();
		for (auto host : that->SavedHosts) {
			nlohmann::json hostJson;
			hostJson["hostname"] = Utils::PlatformStringToStdString(host->LastHostname);
			stateJson["hosts"].push_back(hostJson);
		}
		return FileIO::WriteTextAsync(file,Utils::StringFromStdString(stateJson.dump()));
	});
}

void moonlight_xbox_dx::ApplicationState::RemoveHost(MoonlightHost^ host) {
	unsigned int index;
	bool found = SavedHosts->IndexOf(host, &index);
	SavedHosts->RemoveAt(index);
	if (!host->Connected) {
		host->Connect();
	}
	if (host->Connected) {
		host->Unpair();
	}
	UpdateFile();
}
