#pragma once
#include "pch.h"
#include "Common/DeviceResources.h"
#include <State/StreamConfiguration.h>
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
		MoonlightClient(Windows::Graphics::Display::Core::HdmiDisplayMode^ hdmiDisplayMode);
		int StartStreaming(std::shared_ptr<DX::DeviceResources> res, StreamConfiguration ^config);
		int Connect(const char* hostname);
		bool IsPaired();
		int Pair();
		char *GeneratePIN();
		std::vector<MoonlightApp^> MoonlightClient::GetApplications();
		void SendGamepadReading(short controllerNumber, Windows::Gaming::Input::GamepadReading reading);
		void SendMousePosition(float x, float y);
		void SendMousePressed(int button);
		void SendMouseReleased(int button);
		void SendScroll(float value);
		void SendScrollH(float value);
		void SetSoftwareEncoder(bool value);
		void SetGamepadCount(short count);
		int GetRunningAppID();
		void StopStreaming();
		void StopApp();
		void Unpair();
		void KeyDown(unsigned short v,char modifiers);
		void KeyUp(unsigned short v, char modifiers);
		void SendGuide(int controllerNumber, bool v);
		Platform::String^ GetInstanceID();
		Platform::String^ GetComputerName();
		Platform::String^ GetServerAddress();
		Platform::String^ GetServerMacAddress();
		Windows::Graphics::Display::Core::HdmiDisplayMode^ GetDisplayMode();
		std::function<void(int)> OnStatusUpdate;
		std::function<void()> OnCompleted;
		std::function<void(bool)> SetHDR;
		std::function<void(int,int, char*)> OnFailed;
	private:
		SERVER_DATA serverData;
		char* connectionPin = NULL;
		char* hostname = NULL;
		int port = 0;
		bool useSoftwareEncoder = false;
		int activeGamepadMask = 0;
	};
}