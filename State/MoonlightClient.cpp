#include "pch.h"
#include "MoonlightClient.h"

extern "C" {
#include<Limelight.h>
#include<libgamestream/client.h>
#include <libgamestream/errors.h>
}
#include "Streaming\FFMpegDecoder.h"
#include <Streaming\AudioPlayer.h>
#include <Utils.hpp>
#include <State\StreamConfiguration.h>
#include <gamingdeviceinformation.h>

using namespace moonlight_xbox_dx;
using namespace Windows::Gaming::Input;


void log_message(const char* fmt, ...);
void connection_started();
void connection_status_update(int status);
void connection_status_completed(int status);
void connection_terminated(int status);
void connection_set_hdr(bool value);
void stage_failed(int stage, int err);
void connection_rumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor);
void connection_trigger_rumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor);

MoonlightClient* connectedInstance;
Windows::Graphics::Display::Core::HdmiDisplayMode^ displayMode;

MoonlightClient::MoonlightClient() {

}

MoonlightClient::MoonlightClient(Windows::Graphics::Display::Core::HdmiDisplayMode^ hdmiDisplayMode) {
	displayMode = hdmiDisplayMode;
}

void MoonlightClient::StopApp() {
	gs_quit_app(&serverData);
}
int MoonlightClient::StartStreaming(std::shared_ptr<DX::DeviceResources> res, StreamConfiguration^ sConfig) {
	//Thanks to https://stackoverflow.com/questions/11746146/how-to-convert-platformstring-to-char
	std::wstring fooW(sConfig->hostname->Begin());
	std::string fooA(fooW.begin(), fooW.end());
	const char* charStr = fooA.c_str();
	this->Connect(charStr);
	STREAM_CONFIGURATION config;
	LiInitializeStreamConfiguration(&config);
	config.width = sConfig->width;
	config.height = sConfig->height;
	config.bitrate = sConfig->bitrate;
	config.clientRefreshRateX100 = sConfig->FPS * 100;
	config.colorRange = COLOR_RANGE_LIMITED;  //TODO Make me configurable
	config.encryptionFlags = 0;
	config.fps = sConfig->FPS;
	config.packetSize = 1024;
	config.supportedVideoFormats = VIDEO_FORMAT_H264;
	GAMING_DEVICE_MODEL_INFORMATION info = {};
	GetGamingDeviceModelInformation(&info);
	//Old Xbox One can only use H264
	if (!(info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT && info.deviceId == GAMING_DEVICE_DEVICE_ID_XBOX_ONE)) {
		if (config.height == 2160 || sConfig->videoCodec == "HEVC (H.265)" || sConfig->enableHDR) {
			config.supportedVideoFormats |= VIDEO_FORMAT_H265;
			if (sConfig->enableHDR) {
				config.supportedVideoFormats |= VIDEO_FORMAT_H265_MAIN10;
				config.colorSpace = COLORSPACE_REC_2020;
				config.colorRange = COLOR_RANGE_FULL;
			}
		}
	}
	/*if (config.height == 2160 || sConfig->videoCodec == "HEVC (H.265)") {
		config.supportedVideoFormats |= VIDEO_FORMAT_H265;
	}*/
	

	config.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
	if (sConfig->audioConfig == "Surround 5.1") {
		config.audioConfiguration = AUDIO_CONFIGURATION_51_SURROUND;
	}
	if (sConfig->audioConfig == "Surround 7.1") {
		config.audioConfiguration = AUDIO_CONFIGURATION_71_SURROUND;
	}

	config.streamingRemotely = STREAM_CFG_AUTO;
	char message[2048];
	sprintf(message, "Inserted App ID %d\n", sConfig->appID);
	Utils::Log(message);
	auto gamepads = Windows::Gaming::Input::Gamepad::Gamepads;
	//Since we are on Xbox, we can assume at least one gamepad is connected since they are required on this platform
	this->SetGamepadCount(max(1, gamepads->Size));
	int a = gs_start_app(&serverData, &config, sConfig->appID, false, sConfig->playAudioOnPC, activeGamepadMask);
	if (a != 0) {
		char message[2048];
		sprintf(message, "gs_startapp failed with status code %d\n", a);
		Utils::Log(message);
		Utils::Log(gs_error);
		return a;
	}
	//Sleep(10000);
	connectedInstance = this;
	CONNECTION_LISTENER_CALLBACKS callbacks;
	LiInitializeConnectionCallbacks(&callbacks);
	callbacks.logMessage = log_message;
	callbacks.connectionStarted = connection_started;
	callbacks.connectionStatusUpdate = connection_status_update;
	callbacks.connectionTerminated = connection_terminated;
	callbacks.stageStarting = connection_status_update;
	callbacks.stageFailed = stage_failed;
	callbacks.stageComplete = connection_status_completed;
	callbacks.setHdrMode = connection_set_hdr;
	callbacks.rumble = connection_rumble;
	//callbacks.rumbleTriggers = connection_trigger_rumble;
	FFMpegDecoder::createDecoderInstance(res);
	DECODER_RENDERER_CALLBACKS rCallbacks = FFMpegDecoder::getDecoder();
	AUDIO_RENDERER_CALLBACKS aCallbacks = AudioPlayer::getDecoder();
	int k = LiStartConnection(&serverData.serverInfo, &config, &callbacks, &rCallbacks, &aCallbacks, NULL, 0, NULL, 0);
	sprintf(message, "LiStartConnection %d\n", k);
	Utils::Log(message);
	return k;
}

void MoonlightClient::StopStreaming() {
	LiStopConnection();
}

void log_message(const char* fmt, ...) {
	va_list argp;
	va_start(argp, fmt);
	char message[2048];
	vsprintf_s(message, fmt, argp);
	Utils::Log(message);
}

void connection_started() {
	char message[2048];
	sprintf(message, "Connection Started\n");
	Utils::Log(message);
	if (connectedInstance->OnCompleted != nullptr) {
		connectedInstance->OnCompleted();
	}
}

void connection_status_update(int status) {
	char message[4096];
	sprintf(message, "Stage %d started\n", status);
	Utils::Log(message);

}

void connection_status_completed(int status) {
	char message[4096];
	sprintf(message, "Stage %d completed\n", status);
	Utils::Log(message);
	if (connectedInstance->OnStatusUpdate != nullptr) {
		connectedInstance->OnStatusUpdate(status);
	}
}

void connection_set_hdr(bool enable) {
	if (connectedInstance->SetHDR != nullptr) {
		connectedInstance->SetHDR(enable);
	}

}

void connection_terminated(int status) {
	char message[4096];
	sprintf(message, "Connection terminated with status %d\n", status);
	Utils::Log(message);
}

void stage_failed(int stage, int err) {
	char message[4096];
	unsigned int portFlags = LiGetPortFlagsFromStage(stage);
	int portResult = LiTestClientConnectivity("qt.conntest.moonlight-stream.org", 443, portFlags);
	char failingPorts[128];
	LiStringifyPortFlags(portFlags, ", ", failingPorts, sizeof(failingPorts));
	sprintf(message, "%s failed with error %d.\n Check Firewall and Connections to port: %s\n", LiGetStageName(stage), err, failingPorts);
	Utils::Log(message);
	if (connectedInstance->OnFailed != nullptr) {
		connectedInstance->OnFailed(stage, err,message);
	}
}

void connection_rumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor) {
	if (Windows::Gaming::Input::Gamepad::Gamepads->Size <= controllerNumber)return;
	auto gp = Windows::Gaming::Input::Gamepad::Gamepads->GetAt(controllerNumber);
	float normalizedHigh = highFreqMotor / (float)(256 * 256);
	float normalizedLow = lowFreqMotor / (float)(256 * 256);
	Windows::Gaming::Input::GamepadVibration v = gp->Vibration;
	//v.LeftTrigger = normalizedHigh;
	//v.RightTrigger = normalizedHigh;
	//https://learn.microsoft.com/en-us/gaming/gdk/_content/gc/reference/input/xinputongameinput/structs/xinput_vibration#remarks
	v.LeftMotor = normalizedLow;
	v.RightMotor = normalizedHigh;
	gp->Vibration = v;
}

void connection_trigger_rumble(unsigned short controllerNumber, unsigned short leftTriggerMotor, unsigned short rightTriggerMotor) {
	if (Windows::Gaming::Input::Gamepad::Gamepads->Size <= controllerNumber)return;
	auto gp = Windows::Gaming::Input::Gamepad::Gamepads->GetAt(controllerNumber);
	float normalizedLeft = leftTriggerMotor / (float)(256 * 256);
	float normalizedRight = rightTriggerMotor / (float)(256 * 256);
	Windows::Gaming::Input::GamepadVibration v = gp->Vibration;
	v.LeftTrigger = normalizedLeft;
	v.RightTrigger = normalizedRight;
	gp->Vibration = v;
}

int MoonlightClient::Connect(const char* hostname) {
	this->hostname = (char*)malloc(2048 * sizeof(char));
	strcpy_s(this->hostname, 2048, hostname);
	if (strchr(this->hostname, ':') != 0) {
		char portStr[2048];
		strcpy_s(portStr, 2048, strchr(this->hostname, ':') + 1);
		port = atoi(portStr);
		*strchr(this->hostname, ':') = '\0';
	}
	Platform::String^ folderString = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
	folderString = folderString->Concat(folderString, "\\");
	char folder[2048];
	wcstombs_s(NULL, folder, folderString->Data(), 2047);
	int status = 0;
	status = gs_init(&serverData, this->hostname, port, folder, 3, true);
	char msg[4096];
	sprintf(msg, "Got status %d from gs_init\n", status);
	Utils::Log(msg);
	return status;
}

bool MoonlightClient::IsPaired() {
	return serverData.paired;
}

char* MoonlightClient::GeneratePIN() {
	srand(time(NULL));
	if (connectionPin == NULL)connectionPin = (char*)malloc(5 * sizeof(char));
	sprintf(connectionPin, "%d%d%d%d", rand() % 10, rand() % 10, rand() % 10, rand() % 10);
	return connectionPin;
}

int MoonlightClient::Pair() {
	if (serverData.paired)return -7;
	int status;
	if ((status = gs_pair(&serverData, &connectionPin[0])) != 0) {
		//TODO: Handle gs WRONG STATE
		gs_unpair(&serverData);
		return status;
	}
	return 0;
}

std::vector<MoonlightApp^> MoonlightClient::GetApplications() {
	PAPP_LIST list;
	struct app {
		int Id;
		Platform::String^ Name;
	};
	std::vector<struct app> tempValues;
	std::vector<MoonlightApp^> values;
	int status = gs_applist(&serverData, &list);
	if (list == NULL)return values;
	if (status != 0)return values;
	while (list != NULL) {
		struct app a;
		a.Id = list->id;
		a.Name = Utils::StringFromChars(list->name);
		tempValues.push_back(a);
		list = list->next;
	}
	std::sort(begin(tempValues), end(tempValues), [](struct app& lhs, struct app& rhs)
		{
			if (lhs.Name->Data()[0] == '_' && rhs.Name->Data()[0] != '_')return true;
			if (rhs.Name->Data()[0] == '_' && lhs.Name->Data()[0] != '_')return false;
			return lhs.Name < rhs.Name;
		});
	for (auto s : tempValues) {
		MoonlightApp^ a = ref new MoonlightApp();
		a->Id = s.Id;
		a->Name = s.Name;
		values.push_back(a);
	}
	Platform::String^ folderString = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
	folderString = folderString->Concat(folderString, "\\images\\");
	char folder[2048];
	wcstombs_s(NULL, folder, folderString->Data(), 2047);
	CreateDirectory(folderString->Data(), NULL);
	Concurrency::create_task([folder, folderString, values, this]() {
		for (auto a : values) {
			auto imgPath = folderString->Concat(folderString, a->Id + ".png");
		https://stackoverflow.com/a/6218957
			DWORD dwAttrib = GetFileAttributes(imgPath->Data());
			if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
				gs_appasset(&serverData, folder, a->Id);
			}
			a->ImagePath = imgPath;
		}
		});

	return values;
}

void MoonlightClient::SendGamepadReading(short controllerNumber, GamepadReading reading) {
	int buttonFlags = 0;
	GamepadButtons buttons[] = { GamepadButtons::A,GamepadButtons::B,GamepadButtons::X,GamepadButtons::Y,GamepadButtons::DPadLeft,GamepadButtons::DPadRight,GamepadButtons::DPadUp,GamepadButtons::DPadDown,GamepadButtons::LeftShoulder,GamepadButtons::RightShoulder,GamepadButtons::Menu,GamepadButtons::View,GamepadButtons::LeftThumbstick,GamepadButtons::RightThumbstick };
	int LiButtonFlags[] = { A_FLAG,B_FLAG,X_FLAG,Y_FLAG,LEFT_FLAG,RIGHT_FLAG,UP_FLAG,DOWN_FLAG,LB_FLAG,RB_FLAG,PLAY_FLAG,BACK_FLAG,LS_CLK_FLAG,RS_CLK_FLAG };
	for (int i = 0; i < 14; i++) {
		if ((reading.Buttons & buttons[i]) == buttons[i]) {
			buttonFlags |= LiButtonFlags[i];
		}
	}
	unsigned char leftTrigger = (unsigned char)(round(reading.LeftTrigger * 255.0f));
	unsigned char rightTrigger = (unsigned char)(round(reading.RightTrigger * 255.0f));
	LiSendMultiControllerEvent(controllerNumber, activeGamepadMask, buttonFlags, leftTrigger, rightTrigger, (short)(reading.LeftThumbstickX * 32767), (short)(reading.LeftThumbstickY * 32767), (short)(reading.RightThumbstickX * 32767), (short)(reading.RightThumbstickY * 32767));
}

void MoonlightClient::SendGuide(int controllerNumber, bool s) {
	LiSendMultiControllerEvent(controllerNumber, activeGamepadMask, s ? SPECIAL_FLAG : 0, 0, 0, 0, 0, 0, 0);
}


void MoonlightClient::SendMousePosition(float deltaX, float deltaY) {
	LiSendMouseMoveEvent(deltaX, deltaY);
}

void MoonlightClient::SendMousePressed(int button) {
	LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, button);
}

void MoonlightClient::SendMouseReleased(int button) {
	LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, button);
}

void MoonlightClient::SendScroll(float value) {
	LiSendHighResScrollEvent((short)value);
}

void MoonlightClient::SendScrollH(float value) {
	LiSendHighResHScrollEvent((short)value);
}

void MoonlightClient::SetSoftwareEncoder(bool value) {
	useSoftwareEncoder = value;
}

void MoonlightClient::SetGamepadCount(short count) {
	activeGamepadMask = (1 << count) - 1;
}

int MoonlightClient::GetRunningAppID() {
	return serverData.currentGame;
}

void MoonlightClient::Unpair() {
	int status = gs_unpair(&serverData);
}

Platform::String^ MoonlightClient::GetInstanceID() {
	return Utils::StringFromChars(serverData.uniqueId);
}

Platform::String^ MoonlightClient::GetComputerName() {
	return Utils::StringFromChars(serverData.serverName);
}

Platform::String^ MoonlightClient::GetServerAddress() {
	if (!serverData.serverInfo.address) {
		return nullptr;
	}
	return Utils::StringFromChars((char*)serverData.serverInfo.address);
}

Platform::String^ MoonlightClient::GetServerMacAddress() {
	if (!serverData.macAddress) {
		return nullptr;
	}
	return Utils::StringFromChars(serverData.macAddress);
}

Windows::Graphics::Display::Core::HdmiDisplayMode^ MoonlightClient::GetDisplayMode() {
	if (!displayMode) {
		return nullptr;
	}
	return displayMode;
}

void MoonlightClient::KeyDown(unsigned short v, char modifiers) {
	LiSendKeyboardEvent2(v, KEY_ACTION_DOWN, modifiers, 0);
}

void MoonlightClient::KeyUp(unsigned short v, char modifiers) {
	LiSendKeyboardEvent2(v, KEY_ACTION_UP, modifiers, 0);
}