#pragma once

#include "pch.h"
#include "../Common/DeviceResources.h"
#include <State/StreamConfiguration.h>
#include "State\MoonlightApp.h"
#include <atomic>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <Limelight.h>
	#include <libgamestream/client.h>
}

typedef void(*MoonlightErrorCallback)(const char *msg);

namespace moonlight_xbox_dx {
    class MoonlightClient
    {
    public:
        MoonlightClient();
        ~MoonlightClient();
        bool SetDisplayHDR(bool enabled, const SS_HDR_METADATA& sunshineHdrMetadata);
        int StartStreaming(std::shared_ptr<DX::DeviceResources> res, StreamConfiguration ^config);
        int Connect(const char* hostname);
        bool IsHDR();
        bool IsPaired();
        bool IsRGBFull();
        bool IsConnectionTerminated();
        void SetConnectionTerminated();
        void SetStageFailureReported();
        int Pair();
        char *GeneratePIN();
        std::vector<MoonlightApp^> GetApplications(bool fetchAssets = true);
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
        std::function<void(int)> OnStatusUpdate;
        std::function<void()> OnCompleted;
        std::function<void(bool)> SetHDR;
        std::function<void(int,int, char*)> OnFailed;
        std::function<void(unsigned short, unsigned short, unsigned short)> OnRumble;
        std::function<void(unsigned short, unsigned short, unsigned short)> OnTriggerRumble;
    private:
        SERVER_DATA serverData;
        char* connectionPin = NULL;
        char* hostname = NULL;
        int port = 0;
        std::mutex m_connectMutex;
        bool useSoftwareEncoder = false;
        int activeGamepadMask = 0;
        bool m_isHDR;
        bool m_isRGBFull;
        std::atomic<bool> m_connectionTerminated{ false };
        std::atomic<bool> m_stageFailureReported{ false };
        Windows::Gaming::Input::GamepadReading m_lastGamepadReading[16];
    };
}
