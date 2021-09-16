#pragma once
#include "Common/DeviceResources.h"
#include <FramePacer.h>
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <Limelight.h>
	#include<libgamestream/client.h>
}

typedef struct MoonlightApplication {
	int id;
	char* name;
};
typedef void(*MoonlightErrorCallback)(const char *msg);
namespace moonlight_xbox_dx {
	class MoonlightClient
	{
	public:
		int Init(std::shared_ptr<DX::DeviceResources> res, int width, int height);
		int Connect(char* hostname);
		bool IsPaired();
		int Pair();
		char *GeneratePIN();
		std::vector<MoonlightApplication> GetApplications();
		void SetAppID(int appID);
		static MoonlightClient* GetInstance();
		void SendGamepadReading(Windows::Gaming::Input::GamepadReading reading);
		void SendMousePosition(float x, float y);
		void SendMousePressed(int button);
		void SendMouseReleased(int button);
		void SendScroll(float value);
		void SetSoftwareEncoder(bool value);
		FramePacer *pacer;
	private:
		SERVER_DATA serverData;
		char* connectionPin = NULL;
		char* hostname = NULL;
		int appID = 1;
		bool useSoftwareEncoder = false;
	};
}