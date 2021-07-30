#pragma once
#include "Common/DeviceResources.h"
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
		AVFrame* GetLastFrame();
		static MoonlightClient* GetInstance();
		void SendGamepadReading(Windows::Gaming::Input::GamepadReading reading);
		std::vector<std::wstring> GetLogLines();
		void InsertLog(const char* msg);
		void SendMousePosition(float x, float y);
		void SendMousePressed();
		void SendMouseReleased();
		void SendKeyboardDown();
		void SendKeyboardUp();
	private:
		SERVER_DATA serverData;
		char* connectionPin = NULL;
		char* hostname = NULL;
		int appID = 1;
		std::vector<std::wstring> logLines;
	};
}