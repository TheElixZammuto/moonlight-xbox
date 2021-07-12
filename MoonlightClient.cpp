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

void MoonlightClient::Init(std::shared_ptr<DX::DeviceResources> res,int width,int height) {
	SERVER_DATA server;
	STREAM_CONFIGURATION config;
	Platform::String^ folderString = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
	folderString = folderString->Concat(folderString,"\\");
	char folder[2048];
	wcstombs_s(NULL, folder, folderString->Data(), 2047);
	char ipAddressFile[2048];
	char mappingsFile[2048];
	sprintf(ipAddressFile, "%s%s", folder, "ip_address");
	char ipAddress[256];
	FILE* fp = fopen(ipAddressFile, "r");
	if (fp == NULL) {
		char errorMsg[2048];
		//set_text("Error in opening ip_address in LocalState");
		return;
	}
	fgets(ipAddress, 256, fp);
	fclose(fp);
	int status = 0;
	char connectionMsg[2048];
	sprintf(connectionMsg, "Connecting to %s", ipAddress);
	//set_text(connectionMsg);
	status = gs_init(&server, ipAddress, folder, 0, 0);
	if (status != 0) {
		return;
	}
	//set_text("Init complete");
	if (!server.paired) {
		char pin[5];
		sprintf(pin, "%d%d%d%d", rand() % 10, rand() % 10, rand() % 10, rand() % 10);
		char printText[1024];
		sprintf(printText, "PIN to Pair: %s", pin);
		//set_text(printText);
		if ((status = gs_pair(&server, &pin[0])) != 0) {
			gs_unpair(&server);
			//set_text("Failed to pair to server");
			return;
		}
		else {
			//set_text("Succesfully paired\n");
		}
	}
	else {
		//set_text("Succesfully paired\n");
	}
	PAPP_LIST list;
	gs_applist(&server, &list);
	if (list == NULL)return;
	while (list != NULL && strcmp(list->name, "Desktop") != 0) {
		list = list->next;
	}
	if (list == NULL) {
		//set_text("Missing 'Desktop' app in Host");
		return;
	}
	config.width = 1280;
	config.height = 720;
	config.bitrate = 2000;
	config.clientRefreshRateX100 = 60 * 100;
	config.colorRange = COLOR_RANGE_LIMITED;
	config.encryptionFlags = 0;
	config.fps = 60;
	config.packetSize = 1024;
	config.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
	int a = gs_start_app(&server, &config, list->id, false, true, 0);
	CONNECTION_LISTENER_CALLBACKS callbacks;
	FFMpegDecoder::createDecoderInstance(res);
	DECODER_RENDERER_CALLBACKS rCallbacks = FFMpegDecoder::getDecoder();
	int e = LiStartConnection(&server.serverInfo, &config, NULL, &rCallbacks, NULL, NULL, 0, NULL, 0);
	if (e != 0) {
		return;
	}
	else {
		//set_text("OK");
	}
}