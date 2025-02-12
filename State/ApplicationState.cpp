#include "pch.h"
#include "ApplicationState.h"
#include <Utils.hpp>
#include <nlohmann/json.hpp>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <sstream>

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
					
					std::string jsonIPAddress;
					std::string jsonPort;
					
					if (a.contains("lastHostName") && !a.contains("ipAddress")) {
						std::pair<std::string, int> parseResult = split_ip_port(a["lastHostName"]);
						jsonIPAddress = parseResult.first;
						jsonPort = parseResult.second;
					}
					else
					{
						jsonIPAddress = a["ipAddress"].get<std::string>();
						jsonPort = a.contains("port") ? a["port"].get<std::string>() : "";
					}

					MoonlightHost^ h = ref new MoonlightHost(Utils::StringFromStdString(jsonIPAddress), 
					 										 Utils::StringFromStdString(jsonPort));

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
					else h->ComputerName = h->Hostname;
					this->SavedHosts->Append(h);
				}
			}
		});
}

bool moonlight_xbox_dx::ApplicationState::AddHost(Platform::String^ ipAddress, Platform::String^ port) {
	MoonlightHost^ host = ref new MoonlightHost(ipAddress, port);
	host->UpdateStats();
	if (!host->Connected)return false;
	for (auto h : SavedHosts) {
		if (host->InstanceId == h->InstanceId) {
			h->IPAddress = host->IPAddress;
			h->Port = host->Port;
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
			hostJson["ipAddress"] = Utils::PlatformStringToStdString(host->IPAddress);
			hostJson["port"] = Utils::PlatformStringToStdString(host->Port);
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
	/*
	if (host == nullptr || host->Connected)
	{
		Utils::Log("Host is already connected.\n");
		throw std::runtime_error("Host is already connected.");
	}
	*/

	if (host->NotPaired) {
		Utils::Log("The client and host must be paired.\n");
		throw std::runtime_error("The client and host must be paired.");
	}

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		Utils::Log("Socket startup failed.\n");
		throw std::runtime_error("Socket startup failed.\n");
	}

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == SOCKET_ERROR || s == INVALID_SOCKET) {
		Utils::Log("Socket creation failed.\n");
		WSACleanup();
		throw std::runtime_error("Socket creation failed.\n");
	}
	
	std::string hostIP = Utils::PlatformStringToStdString(host->IPAddress);
	if (hostIP.empty())
	{
		Utils::Log("No IPAddress was found with host.\n");
		WSACleanup();
		throw std::runtime_error("No IPAddress was found with host.\n");
	}
	
	if (inet_addr(hostIP.c_str()) == -1)
	{
		Utils::Log("Given IP Address is not Ipv4. Resolving...\n");
		struct hostent* he = gethostbyname(hostIP.c_str());
		
		if (!he) {
			Utils::Log("gethostbyname failed. Could not resolve the hostname.\n");
			WSACleanup();
			throw std::runtime_error("gethostbyname failed. Could not resolve the hostname.\n");

		}

		hostIP = inet_ntoa(*(struct in_addr*)he->h_addr_list[0]);

		if (inet_addr(hostIP.c_str()) == -1)
		{
			Utils::Log("IPAddress could not be resolved from host name.\n");
			WSACleanup();
			throw std::runtime_error("IPAddress could not be resolved from host name.\n");
		}
	}

	// Get Subnet Mask
	struct in_addr ip_addr;
	if (inet_pton(AF_INET, hostIP.c_str(), &ip_addr) != 1) {
		Utils::Log("The given IP address was invalid.\n");
		WSACleanup();
		throw std::runtime_error("The given IP address was invalid.\n");
	}

	uint32_t hostIP_int = ntohl(ip_addr.s_addr);
	uint32_t subnetMask;

	if ((hostIP_int & 0xF0000000) == 0x00000000) { // Class A
		subnetMask = 4278190080; // 255.0.0.0
	}
	else if ((hostIP_int & 0xE0000000) == 0x80000000) { // Class B
		subnetMask = 4294901760; // 255.255.0.0
	}
	else if ((hostIP_int & 0xC0000000) == 0xC0000000) { // Class C
		subnetMask = 4294967040; // 255.255.255.0
	}
	else {
		Utils::Log("Could not determine subnet mask from IP address.\n");
		WSACleanup();
		throw std::runtime_error("Could not determine subnet mask from IP address.");
	}
	
	// Get Broadcast Address
	auto broadcastInt = hostIP_int | ( ~subnetMask);
	
	// BroadcastAddress to IP string
	std::stringstream ss3;
	for (int i = 3; i >= 0; --i) {
		ss3 << ((broadcastInt >> (8 * i)) & 0xFF);
		if (i > 0) {
			ss3 << ".";
		}
	}
	auto broadcastIP = ss3.str();

	const char* addresses[3] = { 
		"255.255.255.255", 
		hostIP.c_str(),
		broadcastIP.c_str()
	};

	std::string macStr = Utils::PlatformStringToStdString(host->MacAddress);
	std::string macHex(6, 0x00);
	for (int i = 0; i < 6; i++) {
		char dummy[3] = "00";
		for (int j = 0; j < 2; j++) {
			dummy[j] = macStr[j + (i * 3)];
		}

		macHex[i] = (char)strtol(dummy, NULL, 16);
	}

	std::string data(6, (char)0xFF);
	for (int i = 0; i < 16; ++i) {
		data += macHex;
	}

	const int optval = 1;
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&optval, sizeof(optval));

	bool result = false;

	for (int i = 0; i < 3; i++) {
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(struct sockaddr_in));

		addr.sin_family = AF_INET;
		addr.sin_port = htons(9);
		addr.sin_addr.s_addr = inet_addr(addresses[i]);

		bind(s, (struct sockaddr*)&addr, sizeof(addr));

		int status = sendto(s, data.c_str(), (int)data.length(), 0, (struct sockaddr*)&addr, sizeof(addr));
		if (status == SOCKET_ERROR)	{
			Utils::Log("Error sending Wake-On-Lan packet.\n");
		}
		else {
			Utils::Log("Wake-On-Lan packet sent.\n");
			result = true;
		}
	}

	closesocket(s);
	WSACleanup();

	return result;
}

std::pair<std::string, int> moonlight_xbox_dx::ApplicationState::split_ip_port(const std::string& address)
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