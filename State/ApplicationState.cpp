#include "pch.h"
#include "ApplicationState.h"
#include <Utils.hpp>
#include <nlohmann/json.hpp>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>


using namespace Windows::Storage;
Concurrency::task<void> moonlight_xbox_dx::ApplicationState::Init()
{
	auto that = this;
	this->SavedHosts->Clear();
	StorageFolder^ storageFolder = ApplicationData::Current->LocalFolder;
	return concurrency::create_task(storageFolder->CreateFileAsync("state.json", CreationCollisionOption::OpenIfExists)).then([](StorageFile^ file) {
		return FileIO::ReadTextAsync(file);
		}).then([this](Concurrency::task<Platform::String^> jsonTask) {
			this->FirstTime = true;
			Platform::String^ jsonFile = jsonTask.get();
			if (jsonFile != nullptr && jsonFile->Length() > 0) {
				nlohmann::json stateJson = nlohmann::json::parse(jsonFile);
				if (stateJson.contains("firstTime"))this->FirstTime = stateJson["firstTime"];
				if (stateJson.contains("enableKeyboard"))this->EnableKeyboard = stateJson["enableKeyboard"];
				if (stateJson.contains("keyboardLayout"))this->KeyboardLayout = Utils::StringFromStdString(stateJson["keyboardLayout"]);
				if (stateJson.contains("autostartInstance"))this->autostartInstance = stateJson["autostartInstance"];
				if (stateJson.contains("marginWidth"))this->ScreenMarginWidth = stateJson["marginWidth"];
				if (stateJson.contains("marginHeight"))this->ScreenMarginHeight = stateJson["marginHeight"];
				if (stateJson.contains("mouseSensitivity"))this->MouseSensitivity = stateJson["mouseSensitivity"];
				if (stateJson.contains("alternateCombination")) this->AlternateCombination = stateJson["alternateCombination"].get<bool>();
				for (auto a : stateJson["hosts"]) {
					MoonlightHost^ h = ref new MoonlightHost(Utils::StringFromStdString(a["hostname"].get<std::string>()));
					if (a.contains("instance_id")) h->InstanceId = Utils::StringFromStdString(a["instance_id"].get<std::string>());
					if (a.contains("width") && a.contains("height")) {
						h->Resolution = ref new ScreenResolution(a["width"], a["height"]);
					}
					if (a.contains("bitrate"))h->Bitrate = a["bitrate"];
					if (a.contains("fps"))h->FPS = a["fps"];
					if (a.contains("audioConfig"))h->AudioConfig = Utils::StringFromStdString(a["audioConfig"].get<std::string>());
					if (a.contains("videoCodec"))h->VideoCodec = Utils::StringFromStdString(a["videoCodec"].get<std::string>());
					if (a.contains("autoStartID"))h->AutostartID = a["autoStartID"];
					if (a.contains("computername")) h->ComputerName = Utils::StringFromStdString(a["computername"].get<std::string>());
					if (a.contains("playaudioonpc")) h->PlayAudioOnPC = a["playaudioonpc"].get<bool>();
					if (a.contains("enable_hdr")) h->EnableHDR = a["enable_hdr"].get<bool>();
					if (a.contains("macaddress")) h->MacAddress = Utils::StringFromStdString(a["macaddress"].get<std::string>());
					else h->ComputerName = h->LastHostname;
					this->SavedHosts->Append(h);
				}
			}
		});
}

bool moonlight_xbox_dx::ApplicationState::AddHost(Platform::String^ hostname) {
	MoonlightHost^ host = ref new MoonlightHost(hostname);
	host->UpdateStats();
	if (!host->Connected)return false;
	for (auto h : SavedHosts) {
		if (host->InstanceId == h->InstanceId) {
			h->LastHostname = host->LastHostname;
			UpdateFile();
			return true;
		}
	}
	Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([this,host]() {
		SavedHosts->Append(host);
	}));
	UpdateFile();
	return true;
}

Concurrency::task<void> moonlight_xbox_dx::ApplicationState::UpdateFile()
{
	auto that = this;
	StorageFolder^ storageFolder = ApplicationData::Current->LocalFolder;
	return concurrency::create_task(storageFolder->CreateFileAsync("state.json", CreationCollisionOption::OpenIfExists)).then([that](StorageFile^ file) {
		nlohmann::json stateJson;
		stateJson["hosts"] = nlohmann::json::array();
		stateJson["autostartInstance"] = that->autostartInstance;
		stateJson["marginWidth"] = max(0, min(that->ScreenMarginWidth, 250));
		stateJson["marginHeight"] = max(0, min(that->ScreenMarginHeight, 250));
		stateJson["mouseSensitivity"] = max(1, min(that->MouseSensitivity, 16));
		stateJson["firstTime"] = that->FirstTime;
		stateJson["enableKeyboard"] = that->EnableKeyboard;
		stateJson["keyboardLayout"] = Utils::PlatformStringToStdString(that->KeyboardLayout);
		stateJson["alternateCombination"] = that->AlternateCombination;
		for (auto host : that->SavedHosts) {
			nlohmann::json hostJson;
			hostJson["hostname"] = Utils::PlatformStringToStdString(host->LastHostname);
			hostJson["instance_id"] = Utils::PlatformStringToStdString(host->InstanceId);
			hostJson["computername"] = Utils::PlatformStringToStdString(host->ComputerName);
			hostJson["width"] = host->Resolution->Width;
			hostJson["height"] = host->Resolution->Height;
			hostJson["bitrate"] = host->Bitrate;
			hostJson["fps"] = host->FPS;
			hostJson["audioConfig"] = Utils::PlatformStringToStdString(host->AudioConfig);
			hostJson["videoCodec"] = Utils::PlatformStringToStdString(host->VideoCodec);
			hostJson["autoStartID"] = host->AutostartID;
			hostJson["playaudioonpc"] = host->PlayAudioOnPC;
			hostJson["enable_hdr"] = host->EnableHDR;
			stateJson["hosts"].push_back(hostJson);
		}
		auto jsonString = stateJson.dump();
		return FileIO::WriteTextAsync(file, Utils::StringFromStdString(jsonString));
		});
}

void moonlight_xbox_dx::ApplicationState::RemoveHost(MoonlightHost^ host) {
	if (host == nullptr)return;
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

void moonlight_xbox_dx::ApplicationState::OnPropertyChanged(Platform::String^ propertyName)
{
	Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([this,propertyName]() {
		PropertyChanged(this, ref new  Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
	}));
}

moonlight_xbox_dx::ApplicationState^ __stateInstance;

moonlight_xbox_dx::ApplicationState^ moonlight_xbox_dx::GetApplicationState() {
	if (__stateInstance == nullptr)__stateInstance = ref new moonlight_xbox_dx::ApplicationState();
	return __stateInstance;
}

void moonlight_xbox_dx::ApplicationState::WakeHost(MoonlightHost^ host)
{
	if (host == nullptr || host->Connected) return;

	if (host->MacAddress == nullptr || host->MacAddress == "00:00:00:00:00:00") {
		Utils::Log("No recorded Mac address, the client and host must be connected at least once.\n");
		return;
	}

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		Utils::Log("Websocket startup failed.\n");
		return;
	}

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == SOCKET_ERROR || s == INVALID_SOCKET) {
		Utils::Log("Websocket creation failed.\n");
		WSACleanup();
		return;
	}

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9);
	addr.sin_addr.s_addr = inet_addr(Utils::PlatformStringToStdString(host->LastHostname).data());

	const char* macStr = Utils::PlatformStringToStdString(host->MacAddress).data();
	char macHex[6]{};
	for (int i = 0; i < 6; i++) {
		char dummy[3] = "00";
		for (int j = 0; j < 2; j++) {
			dummy[j] = macStr[j + (i * 3)];
		}

		int num = strtol(dummy, NULL, 16);
		                                                                                
		macHex[i] = num;
	}

	char data[102];
	memset(data, 0xff, 6);

	for (int i = 1; i < 17; i++) {
		memcpy(data + (i * 6), macHex, 6);
	}

	int status = sendto(s, data, (int)strlen(data) - 1, 0, (sockaddr*)&addr, sizeof(addr));
	if (status == SOCKET_ERROR)
		Utils::Log("Error sending Wake-On-Lan packet.\n");
	else
		Utils::Log("Wake-On-Lan packet sent.\n");

	WSACleanup();
}
