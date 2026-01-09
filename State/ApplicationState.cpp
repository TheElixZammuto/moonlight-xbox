#include "pch.h"
#include "ApplicationState.h"
#include "../Utils/NetworkUtils.h"
#include <Utils.hpp>
#include <nlohmann/json.hpp>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <algorithm>

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
				    if (a.contains("mdns_instance_name")) h->MdnsInstanceName = Utils::StringFromStdString(a["mdns_instance_name"].get<std::string>());
					if (a.contains("width") && a.contains("height")) {
						h->Resolution = ref new ScreenResolution(a["width"], a["height"]);
					}
					if (a.contains("bitrate"))h->Bitrate = a["bitrate"];
					if (a.contains("fps"))h->FPS = a["fps"];
					if (a.contains("audioConfig"))h->AudioConfig = Utils::StringFromStdString(a["audioConfig"].get<std::string>());
					if (a.contains("videoCodec"))h->VideoCodec = Utils::StringFromStdString(a["videoCodec"].get<std::string>());
					if (a.contains("framePacing"))h->FramePacing = Utils::StringFromStdString(a["framePacing"].get<std::string>());
					if (a.contains("autoStartID"))h->AutostartID = a["autoStartID"];
					if (a.contains("computername")) h->ComputerName = Utils::StringFromStdString(a["computername"].get<std::string>());
					if (a.contains("playaudioonpc")) h->PlayAudioOnPC = a["playaudioonpc"].get<bool>();
					if (a.contains("enable_hdr")) h->EnableHDR = a["enable_hdr"].get<bool>();
					if (a.contains("enable_sops")) h->EnableSOPS = a["enable_sops"].get<bool>();
					if (a.contains("enable_stats")) h->EnableStats = a["enable_stats"].get<bool>();
					if (a.contains("enable_graphs")) h->EnableGraphs = a["enable_graphs"].get<bool>();
					if (a.contains("serverAddress")) h->ServerAddress = Utils::StringFromStdString(a["serverAddress"].get<std::string>());
					if (a.contains("macaddress")) h->MacAddress = Utils::StringFromStdString(a["macaddress"].get<std::string>());
					else h->ComputerName = h->LastHostname;
					this->SavedHosts->Append(h);
				}
			}
		});
}

bool moonlight_xbox_dx::ApplicationState::AddHost(
    Platform::String ^ hostname,
    Platform::String ^ mdnsInstanceName) {
	MoonlightHost ^ host = ref new MoonlightHost(hostname);

	// Carry over the mDNS instance identity if provided
	host->MdnsInstanceName = mdnsInstanceName;

	host->UpdateHostInfo(false);
	if (!host->Connected)
		return false;

	// Deduplicate by mDNS instance name if present, otherwise by InstanceId
	std::string newInstanceId = Utils::PlatformStringToStdString(host->InstanceId);
	std::string newMdnsInstance =
	    mdnsInstanceName != nullptr ? Utils::PlatformStringToStdString(mdnsInstanceName) : "";

	for (auto h : SavedHosts) {
		std::string existingId =
		    Utils::PlatformStringToStdString(h->InstanceId);
		std::string existingMdns =
		    (h->MdnsInstanceName != nullptr)
		        ? Utils::PlatformStringToStdString(h->MdnsInstanceName)
		        : "";

		bool sameByMdns =
		    !newMdnsInstance.empty() && !existingMdns.empty() &&
		    existingMdns == newMdnsInstance;

		bool sameById =
		    !newInstanceId.empty() && !existingId.empty() &&
		    existingId == newInstanceId;

		if (sameByMdns || sameById) {
			// Existing host: update LastHostname, and ensure MdnsInstanceName is set
			h->LastHostname = host->LastHostname;
			h->MdnsInstanceName = host->MdnsInstanceName;
			UpdateFile();
			return true;
		}
	}

	Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
	    Windows::UI::Core::CoreDispatcherPriority::High,
	    ref new Windows::UI::Core::DispatchedHandler([this, host]() {

		    Utils::Logf("[AddHost] NEW HOST: InstanceId='%s' MdnsInstance='%s' LastHostname='%s'\n",
		                Utils::PlatformStringToStdString(host->InstanceId).c_str(),
		                host->MdnsInstanceName ? Utils::PlatformStringToStdString(host->MdnsInstanceName).c_str() : "(null)",
		                Utils::PlatformStringToStdString(host->LastHostname).c_str());

		    SavedHosts->Append(host);
	    }));

	UpdateFile();
	return true;
}

bool moonlight_xbox_dx::ApplicationState::UpdateHostAddressForInstance(
    Platform::String ^ mdnsInstanceName,
    Platform::String ^ newHostPort) {
	if (mdnsInstanceName == nullptr || newHostPort == nullptr)
		return false;

	std::string instanceStd = Utils::PlatformStringToStdString(mdnsInstanceName);
	std::string newHostPortStd = Utils::PlatformStringToStdString(newHostPort);
	std::string newIp = NetworkUtils::ExtractIpFromHostPort(newHostPortStd);

	// Find existing host by MdnsInstanceName
	MoonlightHost ^ target = nullptr;
	for (auto h : SavedHosts) {
		if (h->MdnsInstanceName != nullptr &&
		    Utils::PlatformStringToStdString(h->MdnsInstanceName) == instanceStd) {
			target = h;
			break;
		}
	}

	if (target == nullptr) {
		// No match; let AddHost handle new host scenarios.
		return false;
	}

	std::string oldHostPortStd = Utils::PlatformStringToStdString(target->LastHostname);
	std::string oldIp = NetworkUtils::ExtractIpFromHostPort(oldHostPortStd);

	int oldScore = NetworkUtils::IpPriority(oldIp);
	int newScore = NetworkUtils::IpPriority(newIp);

	if (newScore <= oldScore) {
		// Do not regress or change to equal-priority IP
		return false;
	}

	// Silent upgrade: update stored address and IP, no reconnect

	Utils::Logf("[UpdateHostAddress] UPDATING HOST: existing LastHostname='%s' NewHostName='%s'\n",
	            Utils::PlatformStringToStdString(target->LastHostname).c_str(),
	            Utils::PlatformStringToStdString(newHostPort).c_str());

	target->LastHostname = newHostPort;
	target->ServerAddress = Utils::StringFromStdString(newIp);

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
		stateJson["marginWidth"] = std::max(0, std::min(that->ScreenMarginWidth, 250));
		stateJson["marginHeight"] = std::max(0, std::min(that->ScreenMarginHeight, 250));
		stateJson["mouseSensitivity"] = std::max(1, std::min(that->MouseSensitivity, 16));
		stateJson["firstTime"] = that->FirstTime;
		stateJson["enableKeyboard"] = that->EnableKeyboard;
		stateJson["keyboardLayout"] = Utils::PlatformStringToStdString(that->KeyboardLayout);
		stateJson["alternateCombination"] = that->AlternateCombination;
		for (auto host : that->SavedHosts) {
			nlohmann::json hostJson;
			hostJson["hostname"] = Utils::PlatformStringToStdString(host->LastHostname);
			hostJson["instance_id"] = Utils::PlatformStringToStdString(host->InstanceId);
			hostJson["mdns_instance_name"] = Utils::PlatformStringToStdString(host->MdnsInstanceName);
			hostJson["computername"] = Utils::PlatformStringToStdString(host->ComputerName);
			hostJson["width"] = host->Resolution->Width;
			hostJson["height"] = host->Resolution->Height;
			hostJson["bitrate"] = host->Bitrate;
			hostJson["fps"] = host->FPS;
			hostJson["audioConfig"] = Utils::PlatformStringToStdString(host->AudioConfig);
			hostJson["videoCodec"] = Utils::PlatformStringToStdString(host->VideoCodec);
			hostJson["framePacing"] = Utils::PlatformStringToStdString(host->FramePacing);
			hostJson["autoStartID"] = host->AutostartID;
			hostJson["playaudioonpc"] = host->PlayAudioOnPC;
			hostJson["enable_hdr"] = host->EnableHDR;
			hostJson["enable_sops"] = host->EnableSOPS;
			hostJson["enable_stats"] = host->EnableStats;
			hostJson["enable_graphs"] = host->EnableGraphs;
			hostJson["serverAddress"] = Utils::PlatformStringToStdString(host->ServerAddress);

			std::string macAddr = Utils::PlatformStringToStdString(host->MacAddress);
			if (macAddr != "00:00:00:00:00:00" && macAddr != "")
			{
				hostJson["macaddress"] = macAddr;
			}

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
	Validate_WoL(host);
	std::string wolPayload = WoL_Payload(Utils::PlatformStringToStdString(host->MacAddress));
	int descriptor = Open_Socket();
	auto addressList = Build_Unique_Addresses(host);

	std::vector<int> ports = {
		9,									// Standard WOL port (privileged port)
		47009,								// Port opened by Moonlight Internet Hosting Tool for WoL (non-privileged port)
		47998, 47999, 48000, 48002, 48010	// Ports opened by GFE
	};

	bool result = Send_Payload(descriptor, wolPayload, "255.255.255.255", 0);

	for (int i = 0; i < addressList.size(); i++) {
		for (int j = 0; j < ports.size(); j++) {
			bool sendResult = Send_Payload(descriptor, wolPayload, addressList[i], ports[j]);

			if (sendResult)
			{
				result = true;
			}
		}
	}

	closesocket(descriptor);
	return result;
}

void moonlight_xbox_dx::ApplicationState::Validate_WoL(MoonlightHost^ host)
{
	if (host == nullptr || host->Connected)
	{
		Throw_Error("Host is already connected.");
	}

	if (host->NotPaired) {
		Throw_Error("The client and host must be paired.");
	}

	if (!host->ServerAddress) {
		Throw_Error("No recorded address was found. Connect to host to resolve.");
	}

	if (host->MacAddress == nullptr || host->MacAddress->IsEmpty() || host->MacAddress == "00:00:00:00:00:00") {
		Throw_Error("No recorded Mac address. Connect to host to resolve.");
	}

	return;
}

std::string moonlight_xbox_dx::ApplicationState::WoL_Payload(std::string macAddress)
{
	std::string etherAddr;
	std::string cleanMac;

	for (char c : macAddress) {
		if (isxdigit(c)) cleanMac += c;
	}

	if (cleanMac.length() != 12) {
		throw std::runtime_error(macAddress + " is not a valid ether address");
	}

	for (size_t i = 0; i < 12; i += 2) {
		char byte = static_cast<char>(std::stoi(cleanMac.substr(i, 2), nullptr, 16));
		etherAddr += byte;
	}

	if (etherAddr.length() != 6)
		throw std::runtime_error(macAddress + " is not a valid ether address");

	std::string payload(6, 0xFF);
	for (size_t i = 0; i < 16; ++i) {
		payload += etherAddr;
	}

	return payload;
}

int moonlight_xbox_dx::ApplicationState::Open_Socket()
{
	int descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (descriptor < 0)
		Throw_Error("Failed to open socket");

	const int optval{ 1 };
	if (setsockopt(descriptor, SOL_SOCKET, SO_BROADCAST, (char*)&optval, sizeof(optval)) < 0) {
		Throw_Error("Failed to set socket options");
	}

	return descriptor;
}

std::vector<std::string> moonlight_xbox_dx::ApplicationState::Build_Unique_Addresses(MoonlightHost^ host)
{
	std::vector<std::string> addresses;

	if (host->ServerAddress)
	{
		addresses.push_back(Utils::PlatformStringToStdString(host->ServerAddress));
	}

	if (host->LastHostname)
	{
		auto hostnameSplit = NetworkUtils::SplitIPAddress(Utils::PlatformStringToStdString(host->LastHostname), ':');
		std::string hostIp = hostnameSplit.first;
		addresses.push_back(hostIp);
	}

	if (inet_addr(Utils::PlatformStringToStdString(host->ServerAddress).c_str()) != -1)
	{
		std::string broadcastIP = NetworkUtils::GetBroadcastIP(Utils::PlatformStringToStdString(host->ServerAddress));
		if (broadcastIP != "") 
		{
			addresses.push_back(broadcastIP);
		} else {
			Utils::Log("Could not determine subnet mask from IP address.\n");
		}
	}

	sort(addresses.begin(), addresses.end());
	addresses.erase(unique(addresses.begin(), addresses.end()), addresses.end());

	return addresses;
}

bool moonlight_xbox_dx::ApplicationState::Send_Payload(int descriptor, std::string payload, std::string address, int port)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(address.c_str());
	addr.sin_port = htons(port);

	bind(descriptor, (struct sockaddr*)&addr, sizeof(addr));

	int status = sendto(descriptor, payload.c_str(), (int)payload.length(), 0, (struct sockaddr*)&addr, sizeof(addr));
	if (status == SOCKET_ERROR) {
		std::string msg = std::string() + "Error sending Wake-On-Lan packet to " + address.c_str() + ":" + Utils::PlatformStringToStdString(port.ToString()) + "\n";
		Utils::Log(msg.c_str());
		return false;
	}

	std::string msg = std::string() + "Wake-On-Lan packet sent to " + address.c_str() + ":" + Utils::PlatformStringToStdString(port.ToString()) + "\n";
	Utils::Log(msg.c_str());
	return true;
}

void moonlight_xbox_dx::ApplicationState::Throw_Error(std::string message)
{
	std::string msg = std::string() + message + "\n";
	Utils::Log(msg.c_str());
	throw std::runtime_error(message);
}
