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
			hostJson["macaddress"] = Utils::PlatformStringToStdString(host->MacAddress);
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

bool moonlight_xbox_dx::ApplicationState::WakeHost(MoonlightHost^ host)
{
	if (host == nullptr || host->Connected)
	{
		Utils::Log("Host is already connected.\n");
		throw std::runtime_error("Host is already connected.");
	}

	if (host->NotPaired) {
		Utils::Log("The client and host must be paired.\n");
		throw std::runtime_error("The client and host must be paired.");
	}

	if (host->MacAddress == nullptr || host->MacAddress->IsEmpty() || host->MacAddress == "00:00:00:00:00:00") {
		Utils::Log("No recorded Mac address.\n");
		throw std::runtime_error("No recorded Mac address.");
	}

	std::string macAddress = Utils::PlatformStringToStdString(host->MacAddress);
	std::string etherAddr;

	for (size_t i = 0; i < macAddress.length();) {

		unsigned hex{ 0 };

		for (size_t j = 0; j < macAddress.substr(i, 2).length(); ++j) {
			hex <<= 4;
			std::string s = macAddress.substr(i, 2);
			if (isdigit(s[j])) {
				hex |= s[j] - '0';
			}
			else if (s[j] >= 'a' && s[j] <= 'f') {
				hex |= s[j] - 'a' + 10;
			}
			else if (s[j] >= 'A' && s[j] <= 'F') {
				hex |= s[j] - 'A' + 10;
			}
			else {
				throw std::runtime_error("Failed to parse hexadecimal " + s);
			}
		}

		unsigned hex1 = hex;
		i += 2;

		etherAddr += static_cast<char>(hex1 & 0xFF);

		if (macAddress[i] == ':')
			++i;
	}

	if (etherAddr.length() != 6)
		throw std::runtime_error(macAddress + " not a valid ether address");

	int descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (descriptor < 0)
		throw std::runtime_error("Failed to open socket");

	std::string message(6, 0xFF);
	for (size_t i = 0; i < 16; ++i) {
		message += etherAddr;
	}

	const int optval{ 1 };
	if (setsockopt(descriptor, SOL_SOCKET, SO_BROADCAST, (char *) & optval, sizeof(optval)) < 0) {
		throw std::runtime_error("Failed to set socket options");
	}

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = 0xFFFFFFFF;
	addr.sin_port = htons(60000);

	if (sendto(descriptor, message.c_str(), message.length(), 0,
		reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
		closesocket(descriptor);
		return false;
	}

	closesocket(descriptor);
	return true;
}