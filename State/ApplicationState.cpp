#include "pch.h"
#include "ApplicationState.h"
#include <Utils.hpp>
#include <nlohmann/json.hpp>

using namespace Windows::Storage;
Concurrency::task<void> moonlight_xbox_dx::ApplicationState::Init()
{
	auto that = this;	
	this->SavedHosts->Clear();
	StorageFolder^ storageFolder = ApplicationData::Current->LocalFolder;
	return concurrency::create_task(storageFolder->CreateFileAsync("state.json", CreationCollisionOption::OpenIfExists)).then([](StorageFile^ file) {
		return FileIO::ReadTextAsync(file);
	}).then([this](Concurrency::task<Platform::String^> jsonTask) {
			Platform::String ^jsonFile = jsonTask.get();
			if (jsonFile != nullptr && jsonFile->Length() > 0) {
				nlohmann::json stateJson = nlohmann::json::parse(jsonFile);
				for (auto a : stateJson["hosts"]) {
					MoonlightHost^ h = ref new MoonlightHost();
					h->LastHostname = Utils::StringFromStdString(a["hostname"].get<std::string>());
					if (a.contains("width") && a.contains("height")) {
						h->Resolution = ref new ScreenResolution(a["width"],a["height"]);
					}
					if (a.contains("bitrate"))h->Bitrate = a["bitrate"];
					if (a.contains("fps"))h->FPS = a["fps"];
					if (a.contains("audioConfig"))h->AudioConfig = Utils::StringFromStdString(a["audioConfig"].get<std::string>());
					if (a.contains("autoStartID"))h->AutostartID = a["autoStartID"];
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
			hostJson["width"] = host->Resolution->Width;
			hostJson["height"] = host->Resolution->Height;
			hostJson["bitrate"] = host->Bitrate;
			hostJson["fps"] = host->FPS;
			hostJson["audioConfig"] = Utils::PlatformStringToStdString(host->AudioConfig);
			hostJson["autoStartID"] = host->AutostartID;
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

moonlight_xbox_dx::ApplicationState^ __stateInstance;

moonlight_xbox_dx::ApplicationState^ moonlight_xbox_dx::GetApplicationState() {
	if (__stateInstance == nullptr)__stateInstance = ref new moonlight_xbox_dx::ApplicationState();
	return __stateInstance;
}