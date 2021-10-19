#pragma once
#include "pch.h"
#include "Common/DeviceResources.h"
#include <Streaming\FramePacer.h>
#include <Client\StreamConfiguration.h>
#include "State\MoonlightApp.h"
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <Limelight.h>
	#include<libgamestream/client.h>
}

typedef void(*MoonlightErrorCallback)(const char *msg);
namespace moonlight_xbox_dx {
	class MoonlightClient
	{
	public:
		MoonlightClient();
		int StartStreaming(std::shared_ptr<DX::DeviceResources> res, StreamConfiguration ^config);
		int Connect(const char* hostname);
		bool IsPaired();
		int Pair();
		char *GeneratePIN();
		std::vector<MoonlightApp^> MoonlightClient::GetApplications();
		void SendGamepadReading(Windows::Gaming::Input::GamepadReading reading);
		void SendMousePosition(float x, float y);
		void SendMousePressed(int button);
		void SendMouseReleased(int button);
		void SendScroll(float value);
		void SetSoftwareEncoder(bool value);
		int GetRunningAppID();
		FramePacer *pacer;
	private:
		SERVER_DATA serverData;
		char* connectionPin = NULL;
		char* hostname = NULL;
		bool useSoftwareEncoder = false;
	};
}