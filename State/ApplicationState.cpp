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
			nlohmann::json stateJson = nlohmann::json::parse(jsonFile);
			for (auto a : stateJson["hosts"]) {
				MoonlightHost^ h = ref new MoonlightHost();
				h->LastHostname = Utils::StringFromStdString(a["hostname"].get<std::string>());
				concurrency::create_task([h] (){
					h->UpdateStats();
				});
				this->SavedHosts->Append(h);
			}
	});
}
