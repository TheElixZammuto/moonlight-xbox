#pragma once

#include "pch.h"
#include <State/StreamConfiguration.h>
#include "../Common/DeviceResources.h"
#include "State\MoonlightApp.h"

extern "C" {
#include <Limelight.h>
#include <libavcodec/avcodec.h>
#include <libgamestream/client.h>
}

typedef void (*MoonlightErrorCallback)(const char *msg);

namespace moonlight_xbox_dx {
class MoonlightClient {
  public:
	MoonlightClient();
	~MoonlightClient();
	bool SetDisplayHDR(bool enabled, const SS_HDR_METADATA &sunshineHdrMetadata);
	int StartStreaming(std::shared_ptr<DX::DeviceResources> res, StreamConfiguration ^ config);
	int Connect(const char *hostname);
	bool IsConnectionTerminated();
	void SetConnectionTerminated();
	bool IsHDR();
	bool IsPaired();
	bool IsRGBFull();
	int Pair();
	char *GeneratePIN();
	std::vector<MoonlightApp ^> GetApplications(bool fetchAssets = true);
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
	void KeyDown(unsigned short v, char modifiers);
	void KeyUp(unsigned short v, char modifiers);
	void SendGuide(int controllerNumber, bool v);
	Platform::String ^ GetInstanceID();
	Platform::String ^ GetComputerName();
	Platform::String ^ GetServerAddress();
	Platform::String ^ GetServerMacAddress();
	std::function<void(int)> OnStatusUpdate;
	std::function<void()> OnCompleted;
	std::function<void(bool)> SetHDR;
	std::function<void(int, int, char *)> OnFailed;
	std::function<void(unsigned short, unsigned short, unsigned short)> OnRumble;
	std::function<void(unsigned short, unsigned short, unsigned short)> OnTriggerRumble;

	// RAII lock covering the full duration of a Connect()+status-read sequence (see
	// MoonlightHost::UpdateHostInfo/UpdateApps). Recursive so Connect() can also take it
	// internally without deadlocking when the caller already holds it.
	std::unique_lock<std::recursive_mutex> LockState() { return std::unique_lock<std::recursive_mutex>(m_stateMutex); }

  private:
	SERVER_DATA serverData;
	char *connectionPin = NULL;
	char *hostname = NULL;
	int port = 0;
	bool useSoftwareEncoder = false;
	bool m_isHDR;
	bool m_isRGBFull;
	uint16_t activeGamepadMask = 0;
	Windows::Gaming::Input::GamepadReading m_lastGamepadReading[16];
	// Guards serverData/hostname/port against concurrent access: Connect() locks this
	// internally, and callers reading status right after Connect() (see LockState()) should
	// hold it across their whole read sequence to avoid racing a concurrent Connect().
	std::recursive_mutex m_stateMutex;
};
} // namespace moonlight_xbox_dx
