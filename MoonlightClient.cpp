#include "pch.h"
#include "MoonlightClient.h"

extern "C" {
#include<Limelight.h>
#include<libgamestream/client.h>
}
#include "FFMpegDecoder.h"

using namespace moonlight_xbox_dx;

AVFrame* MoonlightClient::GetLastFrame() {
	if (FFMpegDecoder::getInstance() == NULL || !FFMpegDecoder::getInstance()->setup)return NULL;
	return FFMpegDecoder::getInstance()->GetLastFrame();
}

void log_message(const char* fmt, ...);
void connection_started();
void connection_status_update(int status);
void stage_failed(int stage, int err);

void MoonlightClient::Init(std::shared_ptr<DX::DeviceResources> res,int width,int height) {
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
	int a = gs_start_app(&serverData, &config, appID, false, true, 0);
	CONNECTION_LISTENER_CALLBACKS callbacks;
	callbacks.logMessage = log_message;
	callbacks.connectionStarted = connection_started;
	callbacks.connectionStatusUpdate = connection_status_update;
	callbacks.connectionTerminated = connection_status_update;
	callbacks.stageStarting = connection_status_update;
	callbacks.stageFailed = stage_failed;
	callbacks.stageComplete = connection_status_update;
	FFMpegDecoder::createDecoderInstance(res);
	DECODER_RENDERER_CALLBACKS rCallbacks = FFMpegDecoder::getDecoder();
	int e = LiStartConnection(&serverData.serverInfo, &config, &callbacks, &rCallbacks, NULL, NULL, 0, NULL, 0);
	if (e != 0) {
		return;
	}
	else {
		//set_text("OK");
	}
}

void log_message(const char* fmt, ...) {
	va_list argp;
	va_start(argp, fmt);
	char message[2048];
	sprintf_s(message, fmt, 2048, argp);
	OutputDebugStringA(message);
}

void connection_started() {

}

void connection_status_update(int status) {

}

void stage_failed(int stage, int err) {

}

//Singleton Helpers
MoonlightClient* client;

MoonlightClient* MoonlightClient::GetInstance() {
	if (client != NULL)return client;
	client = new MoonlightClient();
	return client;
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
	if (status != 0) {
		return status;
	}
	return 0;
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