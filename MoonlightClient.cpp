#include "pch.h"
#include "MoonlightClient.h"

extern "C" {
#include<Limelight.h>
#include<libgamestream/client.h>
}
#include "FFMpegDecoder.h"
#include <AudioPlayer.h>
#include <Utils.hpp>

using namespace moonlight_xbox_dx;
using namespace Windows::Gaming::Input;


void log_message(const char* fmt, ...);
void connection_started();
void connection_status_update(int status);
void connection_terminated(int status);
void stage_failed(int stage, int err);
void connection_rumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor);

//Singleton Helpers
MoonlightClient* client;

MoonlightClient* MoonlightClient::GetInstance() {
	if (client != NULL)return client;
	client = new MoonlightClient();
	return client;
}

int MoonlightClient::Init(std::shared_ptr<DX::DeviceResources> res,int width,int height) {
	STREAM_CONFIGURATION config;
	config.width = 1280;
	config.height = 720;
	config.bitrate = 2000;
	config.clientRefreshRateX100 = 60 * 100;
	config.colorRange = COLOR_RANGE_LIMITED;
	config.encryptionFlags = 0;
	config.fps = 60;
	config.packetSize = 1024;
	config.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
	config.supportsHevc = false;
	config.streamingRemotely = STREAM_CFG_AUTO;
	char message[2048];
	sprintf(message, "Inserted App ID %d\n", appID);
	Utils::Log(message);
	int a = gs_start_app(&serverData, &config, appID, false, true, 0);
	CONNECTION_LISTENER_CALLBACKS callbacks;
	callbacks.logMessage = log_message;
	callbacks.connectionStarted = connection_started;
	callbacks.connectionStatusUpdate = connection_status_update;
	callbacks.connectionTerminated = connection_terminated;
	callbacks.stageStarting = connection_status_update;
	callbacks.stageFailed = stage_failed;
	callbacks.stageComplete = connection_status_update;
	callbacks.rumble = connection_rumble;
	FFMpegDecoder::createDecoderInstance(res, useSoftwareEncoder);
	DECODER_RENDERER_CALLBACKS rCallbacks = FFMpegDecoder::getDecoder();
	AUDIO_RENDERER_CALLBACKS aCallbacks = AudioPlayer::getDecoder();
	return LiStartConnection(&serverData.serverInfo, &config, &callbacks, &rCallbacks, &aCallbacks, NULL, 0, NULL, 0);
}

void log_message(const char* fmt, ...) {
	va_list argp;
	va_start(argp, fmt);
	char message[2048];
	vsprintf_s(message, fmt,argp);
	Utils::Log(message);
}

void connection_started() {
	char message[2048];
	sprintf(message, "Connection Started\n");
	Utils::Log(message);
}

void connection_status_update(int status) {
	char message[4096];
	sprintf(message, "Connection updated with status %d\n", status);
	Utils::Log(message);
}

void connection_terminated(int status) {
	char message[4096];
	sprintf(message, "Connection terminated with status %d\n", status);
	Utils::Log(message);
}

void stage_failed(int stage, int err) {
	char message[4096];
	sprintf(message, "Stage %d failed with error %d\n", stage, err);
	Utils::Log(message);
}

void connection_rumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor) {
	Utils::Log("Rumble\n");
}

int MoonlightClient::Connect(char* hostname) {
	this->hostname = (char*)malloc(2048 * sizeof(char));
	strcpy_s(this->hostname, 2048,hostname);
	Platform::String^ folderString = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
	folderString = folderString->Concat(folderString, "\\");
	char folder[2048];
	wcstombs_s(NULL, folder, folderString->Data(), 2047);
	int status = 0;
	status = gs_init(&serverData, this->hostname, folder, 0, 0);
	char msg[4096];
	sprintf(msg,"Got status %d from Moonlight\n", status);
	Utils::Log(msg);
	return status;
}

bool MoonlightClient::IsPaired() {
	return serverData.paired;
}

char *MoonlightClient::GeneratePIN() {
	srand(time(NULL));
	if (connectionPin == NULL)connectionPin = (char*)malloc(5 * sizeof(char));
	sprintf(connectionPin, "%d%d%d%d", rand() % 10, rand() % 10, rand() % 10, rand() % 10);
	return connectionPin;
}

int MoonlightClient::Pair() {
	if (serverData.paired)return -7;
	int status;
	if ((status = gs_pair(&serverData, &connectionPin[0])) != 0) {
		gs_unpair(&serverData);
		return status;
	}
	return 0;
}

std::vector<MoonlightApplication> MoonlightClient::GetApplications() {
	PAPP_LIST list;
	gs_applist(&serverData, &list);
	std::vector<MoonlightApplication> values;
	if (list == NULL)return values;
	while (list != NULL) {
		MoonlightApplication a;
		a.id = list->id;
		a.name = list->name;
		values.push_back(a);
		list = list->next;
	}
	return values;
}

void MoonlightClient::SetAppID(int id) {
	appID = id;
}

void MoonlightClient::SendGamepadReading(GamepadReading reading) {
	int buttonFlags = 0;
	GamepadButtons buttons[] = { GamepadButtons::A,GamepadButtons::B,GamepadButtons::X,GamepadButtons::Y,GamepadButtons::DPadLeft,GamepadButtons::DPadRight,GamepadButtons::DPadUp,GamepadButtons::DPadDown,GamepadButtons::LeftShoulder,GamepadButtons::RightShoulder,GamepadButtons::Menu,GamepadButtons::View,GamepadButtons::LeftThumbstick,GamepadButtons::RightThumbstick };
	int LiButtonFlags[] = { A_FLAG,B_FLAG,X_FLAG,Y_FLAG,LEFT_FLAG,RIGHT_FLAG,UP_FLAG,DOWN_FLAG,LB_FLAG,RB_FLAG,PLAY_FLAG,BACK_FLAG,LS_CLK_FLAG,RS_CLK_FLAG };
	for (int i = 0; i < 14; i++) {
		if ((reading.Buttons & buttons[i]) == buttons[i]) {
			buttonFlags |= LiButtonFlags[i];
		}
	}
	char output[1024];
	//sprintf(output,"Got from gamepad: %d\n", buttonFlags);
	OutputDebugStringA(output);
	LiSendControllerEvent(buttonFlags, (short)(reading.LeftTrigger * 32767), (short)(reading.RightTrigger * 32767), (short)(reading.LeftThumbstickX * 32767), (short)(reading.LeftThumbstickY * 32767), (short)(reading.RightThumbstickX * 32767), (short)(reading.RightThumbstickY * 32767));
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
	LiSendScrollEvent((signed char)(value * 2.0f));
}

void MoonlightClient::SetSoftwareEncoder(bool value) {
	useSoftwareEncoder = value;
}