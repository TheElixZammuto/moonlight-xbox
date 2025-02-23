#include "pch.h"
#include "ApplicationState.h"
#include <Utils.hpp>
#include <nlohmann/json.hpp>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <sstream>
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
					if (a.contains("serverAddress")) h->ServerAddress = Utils::StringFromStdString(a["serverAddress"].get<std::string>());
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
		9, // Standard WOL port (privileged port)
		47009, // Port opened by Moonlight Internet Hosting Tool for WoL (non-privileged port)
		47998, 47999, 48000, 48002, 48010 // Ports opened by GFE
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
		Throw_Error("No recorded address was found");
	}

	if (host->MacAddress == nullptr || host->MacAddress->IsEmpty() || host->MacAddress == "00:00:00:00:00:00") {
		Throw_Error("No recorded Mac address.");
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
		auto hostnameSplit = Split_IP_Port(Utils::PlatformStringToStdString(host->LastHostname));
		std::string hostIp = hostnameSplit.first;
		addresses.push_back(hostIp);
	}

	if (inet_addr(Utils::PlatformStringToStdString(host->ServerAddress).c_str()) != -1)
	{
		std::string broadcastIP = Get_Broadcast_IP(Utils::PlatformStringToStdString(host->ServerAddress));
		addresses.push_back(broadcastIP);
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

std::string moonlight_xbox_dx::ApplicationState::Get_Broadcast_IP(std::string ipAddress)
{
	// Get Subnet Mask
	struct in_addr ip_addr;
	if (inet_pton(AF_INET, ipAddress.c_str(), &ip_addr) != 1) {
		WSACleanup();
		Throw_Error("The given IP address was invalid.");
	}

	uint32_t ipAddress_int = ntohl(ip_addr.s_addr);
	uint32_t subnetMask;

	if ((ipAddress_int & 0xF0000000) == 0x00000000) { // Class A
		subnetMask = 4278190080; // 255.0.0.0
	}
	else if ((ipAddress_int & 0xE0000000) == 0x80000000) { // Class B
		subnetMask = 4294901760; // 255.255.0.0
	}
	else if ((ipAddress_int & 0xC0000000) == 0xC0000000) { // Class C
		subnetMask = 4294967040; // 255.255.255.0
	}
	else {
		Utils::Log("Could not determine subnet mask from IP address.\n");
		WSACleanup();
		throw std::runtime_error("Could not determine subnet mask from IP address.");
	}

	auto broadcastInt = ipAddress_int | (~subnetMask);

	std::stringstream ss3;
	for (int i = 3; i >= 0; --i) {
		ss3 << ((broadcastInt >> (8 * i)) & 0xFF);
		if (i > 0) {
			ss3 << ".";
		}
	}
	return ss3.str();
}

std::pair<std::string, int> moonlight_xbox_dx::ApplicationState::Split_IP_Port(const std::string& address)
{
	std::stringstream ss(address);
	std::string ip_address;
	std::string port_str;

	if (std::getline(ss, ip_address, ':') && std::getline(ss, port_str)) {
		try {
			int port = std::stoi(port_str);
			return { ip_address, port };
		}
		catch (const std::invalid_argument& e) {
			throw std::invalid_argument("Invalid port number format");
		}
		catch (const std::out_of_range& e) {
			throw std::out_of_range("Port number out of range");
		}
	}
	else {
		throw std::invalid_argument("Invalid address format");
	}
}

void moonlight_xbox_dx::ApplicationState::Throw_Error(std::string message)
{
	std::string msg = std::string() + message + "\n";
	Utils::Log(msg.c_str());
	throw std::runtime_error(message);
}
