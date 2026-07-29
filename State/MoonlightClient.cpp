#include "MoonlightClient.h"
#include "pch.h"
#define MLOG_TAG_OVERRIDE "MoonlightClient"

extern "C" {
#include <Limelight.h>
#include <libgamestream/client.h>
#include <libgamestream/errors.h>
}
#include <State\StreamConfiguration.h>
#include <Streaming\AudioPlayer.h>
#include <Utils.hpp>
#include <atomic>
#include <mutex>
#include <cmath>
#include <string>
#include <unordered_set>
#include <gamingdeviceinformation.h>
#include "Streaming\FFMpegDecoder.h"

using namespace moonlight_xbox_dx;
using namespace Windows::Gaming::Input;
using namespace Windows::Graphics::Display;
using namespace Windows::Graphics::Display::Core;

void log_message(const char *fmt, ...);
void connection_started();
void connection_status_update(int status);
void connection_status_completed(int status);
void connection_terminated(int status);
void connection_set_hdr(bool value);
void stage_failed(int stage, int err);
void connection_rumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor);
void connection_trigger_rumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor);

void logDisplayMode(const char *str, HdmiDisplayMode ^ mode) {
	if (mode == nullptr) return;

	char modeStr[256];
	snprintf(modeStr, sizeof(modeStr), "%s: %s %ux%u @ %.2fhz, %u bpp, %s, %s\n",
	         str,
	         mode->IsSmpte2084Supported ? "HDR" : "   ",
	         mode->ResolutionWidthInRawPixels, mode->ResolutionHeightInRawPixels,
	         mode->RefreshRate,
	         mode->BitsPerPixel,
	         mode->ColorSpace == HdmiDisplayColorSpace::RgbLimited ? "RgbLimited"
	         : mode->ColorSpace == HdmiDisplayColorSpace::RgbFull  ? "RgbFull"
	         : mode->ColorSpace == HdmiDisplayColorSpace::BT709    ? "BT709"
	         : mode->ColorSpace == HdmiDisplayColorSpace::BT2020   ? "BT2020"
	                                                               : "Unknown",
	         mode->PixelEncoding == HdmiDisplayPixelEncoding::Rgb444   ? "Rgb444"
	         : mode->PixelEncoding == HdmiDisplayPixelEncoding::Ycc444 ? "Ycc444"
	         : mode->PixelEncoding == HdmiDisplayPixelEncoding::Ycc422 ? "Ycc422"
	         : mode->PixelEncoding == HdmiDisplayPixelEncoding::Ycc420 ? "Ycc420"
	                                                                   : "Unknown");
	MLOG(Utils::LogLevel::Debug, modeStr);
}

// Based on CWIN32Util::ToggleWindowsHDR from xbmc
// switches to the HDR or SDR version of the current display mode, per the enabled argument
// Returns false if the requested state was not set for some reason.
bool MoonlightClient::SetDisplayHDR(bool enabled, const SS_HDR_METADATA &sunshineHdrMetadata) {
	HdmiDisplayInformation ^ hdmi = HdmiDisplayInformation::GetForCurrentView();
	if (!hdmi) {
		return false;
	}

	bool resendCurrentMode = false;
	HdmiDisplayMode ^ current = hdmi->GetCurrentDisplayMode();
	logDisplayMode("SetDisplayHDR(): current display mode", current);

	if (current->IsSmpte2084Supported) {
		// HDR is enabled
		m_isHDR = true;
		if (enabled) {
			MLOG(Utils::LogLevel::Info, "SetDisplayHDR(true): display is already in HDR mode\n");
			resendCurrentMode = true;
		}
	} else {
		// HDR is disabled
		m_isHDR = false;
		if (!enabled) {
			MLOG(Utils::LogLevel::Info, "SetDisplayHDR(false): display is already in SDR mode\n");
			return true;
		}
	}

	// this method is only run on Series S/X, so we can bail out if the system is not set to 4K
	if (current->ResolutionWidthInRawPixels < 3840) {
		MLOG(Utils::LogLevel::Warning, "Warning: HDR may be unavailable when Xbox is not set to 4K resolution\n");
		// return false;
	}

	// convert the HDR metadata format to HdmiDisplayHdr2086Metadata
	HdmiDisplayHdr2086Metadata hdrMetadata = {};
	hdrMetadata.RedPrimaryX = sunshineHdrMetadata.displayPrimaries[0].x;
	hdrMetadata.RedPrimaryY = sunshineHdrMetadata.displayPrimaries[0].y;
	hdrMetadata.GreenPrimaryX = sunshineHdrMetadata.displayPrimaries[1].x;
	hdrMetadata.GreenPrimaryY = sunshineHdrMetadata.displayPrimaries[1].y;
	hdrMetadata.BluePrimaryX = sunshineHdrMetadata.displayPrimaries[2].x;
	hdrMetadata.BluePrimaryY = sunshineHdrMetadata.displayPrimaries[2].y;
	hdrMetadata.WhitePointX = sunshineHdrMetadata.whitePoint.x;
	hdrMetadata.WhitePointY = sunshineHdrMetadata.whitePoint.y;
	hdrMetadata.MaxMasteringLuminance = sunshineHdrMetadata.maxDisplayLuminance;
	hdrMetadata.MinMasteringLuminance = sunshineHdrMetadata.minDisplayLuminance;
	hdrMetadata.MaxContentLightLevel = sunshineHdrMetadata.maxContentLightLevel;
	hdrMetadata.MaxFrameAverageLightLevel = sunshineHdrMetadata.maxFrameAverageLightLevel;

	// log all available modes
	MLOG(Utils::LogLevel::Debug, "Supported display modes:\n");
	for (auto mode : hdmi->GetSupportedDisplayModes()) {
		logDisplayMode(" ", mode);
	}

	// Choose new mode that is the same as current except for HDR status
	HdmiDisplayMode ^ newMode = {};
	for (auto mode : hdmi->GetSupportedDisplayModes()) {
		if (mode->IsSmpte2084Supported != current->IsSmpte2084Supported &&
		    mode->ResolutionHeightInRawPixels == current->ResolutionHeightInRawPixels &&
		    mode->ResolutionWidthInRawPixels == current->ResolutionWidthInRawPixels &&
		    mode->StereoEnabled == false &&
		    std::fabs(mode->RefreshRate - current->RefreshRate) <= 0.00001) {
			newMode = mode;
			break;
		}
	}

	// A non-HDR display viewing an HDR stream will error out here with no mode found
	if (newMode == nullptr) {
		MLOG(Utils::LogLevel::Warning, "SetDisplayHDR(): HDR is unavailable, no suitable display mode found\n");
		return false;
	}

	// we might resend the current mode with new HDR metadata
	if (resendCurrentMode) {
		newMode = current;
	}

	HdmiDisplayHdrOption hdrOption = HdmiDisplayHdrOption::None;
	if (enabled) {
		logDisplayMode("SetDisplayHDR(true): switching to HDR mode", newMode);
		MLOGF(Utils::LogLevel::Debug, "Sending HDR10 metadata: Min/MaxLuminance %.0f / %u\n",
		            hdrMetadata.MinMasteringLuminance / 10000.0, hdrMetadata.MaxMasteringLuminance);
		hdrOption = HdmiDisplayHdrOption::Eotf2084;
	} else {
		logDisplayMode("SetDisplayHDR(false): switching to SDR mode", newMode);
		hdrOption = HdmiDisplayHdrOption::None;
	}

	// Change the display mode
	auto modeChange = Concurrency::create_task(
	    hdmi->RequestSetCurrentDisplayModeAsync(newMode, hdrOption, hdrMetadata));

	// wait for async bool result of display mode change
	if (modeChange.get()) {
		// Check the current display mode to verify
		// XXX sometimes this lies and the TV is in another mode :(
		current = hdmi->GetCurrentDisplayMode();
		logDisplayMode("SetDisplayHDR(): successfully changed to", current);

		if (enabled && current->IsSmpte2084Supported) {
			m_isHDR = true;
			return true;
		} else if (!enabled && !current->IsSmpte2084Supported) {
			m_isHDR = false;
			return true;
		}
	} else {
		MLOG(Utils::LogLevel::Error, "SetDisplayHDR(): Error switching display mode.\n");
	}

	return false;
}

MoonlightClient *connectedInstance;

MoonlightClient::MoonlightClient()
    : m_isHDR(false),
      m_isRGBFull(false),
      m_connectionTerminated(false) {
	HdmiDisplayInformation ^ hdmi = HdmiDisplayInformation::GetForCurrentView();
	if (hdmi) {
		HdmiDisplayMode ^ current = hdmi->GetCurrentDisplayMode();
		if (current->IsSmpte2084Supported) {
			m_isHDR = true;
		}
		if (current->ColorSpace == HdmiDisplayColorSpace::RgbFull) {
			// RgbFull is the SDR mode used when Xbox is set to "PC RGB" in Video Fidelity -> Color space
			m_isRGBFull = true;
		}
	}
}

MoonlightClient::~MoonlightClient() {
	if (hostname != NULL) {
		free(hostname);
		hostname = NULL;
	}
}

void MoonlightClient::StopApp() {
	gs_quit_app(&serverData);
}
int MoonlightClient::StartStreaming(std::shared_ptr<DX::DeviceResources> res, StreamConfiguration ^ sConfig) {

	std::string fooA = Utils::PlatformStringToStdString(sConfig->hostname);
	this->Connect(fooA.c_str());
	STREAM_CONFIGURATION config;
	LiInitializeStreamConfiguration(&config);
	config.width = sConfig->width;
	config.height = sConfig->height;
	config.bitrate = sConfig->bitrate;
	config.fps = sConfig->FPS;
	if (res->GetRefreshRate() > 0.0 && IsXbox()) {
		// Pass fractional refresh rate to host in case it's supported
		double rr = res->GetRefreshRate();
		switch (config.fps) {
		case 120:
			if (rr >= 120.0) {
				config.clientRefreshRateX100 = 12000;
			} else if (rr >= 119.0) {
				config.clientRefreshRateX100 = 11988;
			} else {
				// Allow sending 120fps to a 60hz client. It's what they asked for
				// and how other clients behave.
				config.clientRefreshRateX100 = config.fps * 100;
			}
			break;
		case 60:
			if (rr >= 120.0) {
				config.clientRefreshRateX100 = 6000;
			} else if (rr >= 119.0) {
				config.clientRefreshRateX100 = 5994;
			} else {
				config.clientRefreshRateX100 = (int)std::lround(rr * 100.0);
			}
			break;
		case 30:
			if (rr >= 120.0) {
				config.clientRefreshRateX100 = 3000;
			} else if (rr >= 119.0) {
				config.clientRefreshRateX100 = 2997;
			} else if (rr >= 60.0) {
				config.clientRefreshRateX100 = 3000;
			} else if (rr >= 59.0) {
				config.clientRefreshRateX100 = 2997;
			} else {
				config.clientRefreshRateX100 = (int)std::lround(rr * 100.0);
			}
			break;
		default:
			config.clientRefreshRateX100 = config.fps * 100;
			break;
		}

		MLOGF(Utils::LogLevel::Debug, "Requesting stream with clientRefreshRateX100=%d for %d FPS on %.2f Hz display\n",
		            config.clientRefreshRateX100, config.fps, rr);
	}
	config.colorRange = this->IsRGBFull() ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
	config.colorSpace = COLORSPACE_REC_601;
	config.encryptionFlags = ENCFLG_AUDIO;
	config.packetSize = 1024;
	config.supportedVideoFormats = VIDEO_FORMAT_H264;
	if (!IsXboxOneVCR()) {
		config.supportedVideoFormats |= VIDEO_FORMAT_H265;
		if (sConfig->enableHDR) {
			config.supportedVideoFormats |= VIDEO_FORMAT_H265_MAIN10;
		}
	}

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
	MLOG(Utils::LogLevel::Info, message);
	auto gamepads = Windows::Gaming::Input::Gamepad::Gamepads;

	this->SetGamepadCount(std::max((UINT)1, gamepads->Size));
	int a = gs_start_app(&serverData, &config, sConfig->appID, sConfig->enableSOPS, sConfig->playAudioOnPC, activeGamepadMask);
	if (a != 0) {
		char message[2048];
		sprintf(message, "gs_startapp failed with status code %d\n", a);
		MLOG(Utils::LogLevel::Error, message);
		if (gs_error) {
			char errorMessage[2048];
			sprintf(errorMessage, "%s\n", gs_error);
			MLOG(Utils::LogLevel::Error, errorMessage);
			if (this->OnFailed != nullptr) {
				this->OnFailed(0, a, errorMessage);
			}
		}
		return a;
	}
	connectedInstance = this;
	m_connectionTerminated.store(false);
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
	callbacks.rumbleTriggers = connection_trigger_rumble;

	const bool framePacingImmediate = (sConfig->framePacing != nullptr && sConfig->framePacing == "Immediate");
	FFMpegDecoder::instance().CompleteInitialization(res, &config, framePacingImmediate);
	DECODER_RENDERER_CALLBACKS rCallbacks = FFMpegDecoder::getDecoder();

	AUDIO_RENDERER_CALLBACKS aCallbacks = AudioPlayer::getDecoder();
	m_stageFailureReported.store(false);
	int k = LiStartConnection(&serverData.serverInfo, &config, &callbacks, &rCallbacks, &aCallbacks, NULL, 0, NULL, 0);
	sprintf(message, "LiStartConnection %d\n", k);
	MLOG(Utils::LogLevel::Debug, message);

	if (k != 0 && !m_stageFailureReported.load() && this->OnFailed != nullptr) {
		this->OnFailed(0, k, "Connection failed");
	}
	return k;
}

void MoonlightClient::StopStreaming() {
	LiStopConnection();
}

void log_message(const char *fmt, ...) {
	va_list argp;
	va_start(argp, fmt);
	char message[2048];
	vsprintf_s(message, fmt, argp);
	MLOG(Utils::LogLevel::Debug, message);
}

void connection_started() {
	char message[2048];
	sprintf(message, "Connection Started\n");
	MLOG(Utils::LogLevel::Info, message);
	if (connectedInstance->OnCompleted != nullptr) {
		connectedInstance->OnCompleted();
	}
}

void connection_status_update(int status) {
	char message[4096];
	sprintf(message, "Stage %d started\n", status);
	MLOG(Utils::LogLevel::Debug, message);
}

void connection_status_completed(int status) {
	char message[4096];
	sprintf(message, "Stage %d completed\n", status);
	MLOG(Utils::LogLevel::Debug, message);
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
	MLOG(Utils::LogLevel::Info, message);
	if (connectedInstance != nullptr) {
		connectedInstance->SetConnectionTerminated();
	}
}

void stage_failed(int stage, int err) {
	char message[4096];
	unsigned int portFlags = LiGetPortFlagsFromStage(stage);
	int portResult = LiTestClientConnectivity("qt.conntest.moonlight-stream.org", 443, portFlags);
	char failingPorts[128];
	LiStringifyPortFlags(portFlags, ", ", failingPorts, sizeof(failingPorts));
	sprintf(message, "%s failed with error %d.\n Check Firewall and Connections to port: %s\n", LiGetStageName(stage), err, failingPorts);
	MLOG(Utils::LogLevel::Error, message);
	connectedInstance->SetStageFailureReported();
	if (connectedInstance->OnFailed != nullptr) {
		connectedInstance->OnFailed(stage, err, message);
	}
}

void connection_rumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor) {
	if (connectedInstance != nullptr && connectedInstance->OnRumble != nullptr) {
		connectedInstance->OnRumble(controllerNumber, lowFreqMotor, highFreqMotor);
		return;
	}
	if (Windows::Gaming::Input::Gamepad::Gamepads->Size <= controllerNumber) return;
	auto gp = Windows::Gaming::Input::Gamepad::Gamepads->GetAt(controllerNumber);
	float normalizedHigh = highFreqMotor / (float)(256 * 256);
	float normalizedLow = lowFreqMotor / (float)(256 * 256);
	Windows::Gaming::Input::GamepadVibration v = gp->Vibration;

	v.LeftMotor = normalizedLow;
	v.RightMotor = normalizedHigh;
	gp->Vibration = v;
}

void connection_trigger_rumble(unsigned short controllerNumber, unsigned short leftTriggerMotor, unsigned short rightTriggerMotor) {
	if (connectedInstance != nullptr && connectedInstance->OnTriggerRumble != nullptr) {
		connectedInstance->OnTriggerRumble(controllerNumber, leftTriggerMotor, rightTriggerMotor);
		return;
	}
	if (Windows::Gaming::Input::Gamepad::Gamepads->Size <= controllerNumber) return;
	auto gp = Windows::Gaming::Input::Gamepad::Gamepads->GetAt(controllerNumber);
	float normalizedLeft = leftTriggerMotor / (float)(256 * 256);
	float normalizedRight = rightTriggerMotor / (float)(256 * 256);
	Windows::Gaming::Input::GamepadVibration v = gp->Vibration;
	v.LeftTrigger = normalizedLeft;
	v.RightTrigger = normalizedRight;
	gp->Vibration = v;
}

int MoonlightClient::Connect(const char *hostname) {
	std::lock_guard<std::mutex> lock(m_connectMutex);
	if (this->hostname != NULL) {
		free(this->hostname);
		this->hostname = NULL;
	}
	this->hostname = (char *)malloc(2048 * sizeof(char));
	strcpy_s(this->hostname, 2048, hostname);
	if (strchr(this->hostname, ':') != 0) {
		char portStr[2048];
		strcpy_s(portStr, 2048, strchr(this->hostname, ':') + 1);
		port = atoi(portStr);
		*strchr(this->hostname, ':') = '\0';
	}

	Platform::String ^ folderString = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
	folderString = folderString->Concat(folderString, "\\");
	char folder[2048];
	wcstombs_s(NULL, folder, folderString->Data(), 2047);

	int status = gs_init(&serverData, this->hostname, port, folder, 3, true);
	return status;
}

bool MoonlightClient::IsHDR() {
	return m_isHDR;
}

bool MoonlightClient::IsRGBFull() {
	return m_isRGBFull;
}

bool MoonlightClient::IsConnectionTerminated() {
	return m_connectionTerminated.load();
}

void MoonlightClient::SetConnectionTerminated() {
	m_connectionTerminated.store(true);
}

void MoonlightClient::SetStageFailureReported() {
	m_stageFailureReported.store(true);
}

bool MoonlightClient::IsPaired() {
	return serverData.paired;
}

char *MoonlightClient::GeneratePIN() {
	srand(time(NULL));
	if (connectionPin == NULL) connectionPin = (char *)malloc(5 * sizeof(char));
	sprintf(connectionPin, "%d%d%d%d", rand() % 10, rand() % 10, rand() % 10, rand() % 10);
	return connectionPin;
}

int MoonlightClient::Pair() {
	if (serverData.paired) return -7;
	int status;
	if ((status = gs_pair(&serverData, &connectionPin[0])) != 0) {
		gs_unpair(&serverData);
		return status;
	}
	return 0;
}

std::vector<MoonlightApp ^> MoonlightClient::GetApplications(bool fetchAssets) {
	PAPP_LIST list;
	struct app {
		int Id;
		Platform::String ^ Name;
	};
	std::vector<struct app> tempValues;
	std::vector<MoonlightApp ^> values;
	int status = gs_applist(&serverData, &list);
	if (list == NULL) return values;
	if (status != 0) return values;
	while (list != NULL) {
		struct app a;
		a.Id = list->id;
		a.Name = Utils::StringFromChars(list->name);
		tempValues.push_back(a);
		list = list->next;
	}
	std::sort(begin(tempValues), end(tempValues), [](struct app &lhs, struct app &rhs) {
		if (lhs.Name->Data()[0] == '_' && rhs.Name->Data()[0] != '_') return true;
		if (rhs.Name->Data()[0] == '_' && lhs.Name->Data()[0] != '_') return false;
		return lhs.Name < rhs.Name;
	});
	for (auto s : tempValues) {
		MoonlightApp ^ a = ref new MoonlightApp();
		a->Id = s.Id;
		a->Name = s.Name;
		values.push_back(a);
	}
	Platform::String ^ baseImages = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
	baseImages = Platform::String::Concat(baseImages, L"\\images\\");
	Platform::String ^ hostId = (serverData.uniqueId != nullptr)
	    ? Utils::StringFromChars(serverData.uniqueId)
	    : ref new Platform::String(L"unknown");
	Platform::String ^ folderString = Platform::String::Concat(baseImages, Platform::String::Concat(hostId, L"\\"));
	char folder[2048];
	wcstombs_s(NULL, folder, folderString->Data(), 2047);
	CreateDirectory(baseImages->Data(), NULL);
	CreateDirectory(folderString->Data(), NULL);
	if (fetchAssets) {
		Concurrency::create_task([folder, folderString, values, this]() {
			std::unordered_set<int> currentIds;
			for (auto a : values) currentIds.insert(a->Id);

			for (auto a : values) {
				auto imgPath = Platform::String::Concat(folderString, a->Id + ".png");
				// https://stackoverflow.com/a/6218957
				DWORD dwAttrib = GetFileAttributes(imgPath->Data());
				if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
					gs_appasset(&serverData, folder, a->Id);
				}
				a->ImagePath = imgPath;
			}

			std::wstring searchPath(folderString->Data());
			searchPath += L"*.png";
			WIN32_FIND_DATAW findData;
			HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					std::wstring fname(findData.cFileName);
					if (fname.size() > 4 && fname.substr(fname.size() - 4) == L".png") {
						std::wstring numStr = fname.substr(0, fname.size() - 4);
						wchar_t* end = nullptr;
						long id = wcstol(numStr.c_str(), &end, 10);
						if (end != numStr.c_str() && *end == L'\0' && currentIds.find((int)id) == currentIds.end()) {
							std::wstring base(folderString->Data());
							DeleteFileW((base + fname).c_str());
							DeleteFileW((base + L"blur\\" + numStr + L"_bg.png").c_str());
							DeleteFileW((base + L"blur\\" + numStr + L"_glow.png").c_str());
						}
					}
				} while (FindNextFileW(hFind, &findData));
				FindClose(hFind);
			}
		});
	}

	return values;
}

static bool hasGamepadReadingChanged(GamepadReading a, GamepadReading b) {
	if (a.Buttons != b.Buttons) {
		return true;
	}

	short altX = (short)(a.LeftThumbstickX * 32767);
	short altY = (short)(a.LeftThumbstickY * 32767);
	short artX = (short)(a.RightThumbstickX * 32767);
	short artY = (short)(a.RightThumbstickY * 32767);
	short bltX = (short)(b.LeftThumbstickX * 32767);
	short bltY = (short)(b.LeftThumbstickY * 32767);
	short brtX = (short)(b.RightThumbstickX * 32767);
	short brtY = (short)(b.RightThumbstickY * 32767);
	if (altX != bltX || altY != bltY || artX != brtX || artY != brtY) {
		return true;
	}

	unsigned char alTrig = (unsigned char)(round(a.LeftTrigger * 255.0f));
	unsigned char arTrig = (unsigned char)(round(a.RightTrigger * 255.0f));
	unsigned char blTrig = (unsigned char)(round(b.LeftTrigger * 255.0f));
	unsigned char brTrig = (unsigned char)(round(b.RightTrigger * 255.0f));
	if (alTrig != blTrig || arTrig != brTrig) {
		return true;
	}

	return false;
}

void MoonlightClient::SendGamepadReading(short controllerNumber, GamepadReading reading) {
	int buttonFlags = 0;
	GamepadButtons buttons[] = {GamepadButtons::A, GamepadButtons::B, GamepadButtons::X, GamepadButtons::Y, GamepadButtons::DPadLeft, GamepadButtons::DPadRight, GamepadButtons::DPadUp, GamepadButtons::DPadDown, GamepadButtons::LeftShoulder, GamepadButtons::RightShoulder, GamepadButtons::Menu, GamepadButtons::View, GamepadButtons::LeftThumbstick, GamepadButtons::RightThumbstick};
	int LiButtonFlags[] = {A_FLAG, B_FLAG, X_FLAG, Y_FLAG, LEFT_FLAG, RIGHT_FLAG, UP_FLAG, DOWN_FLAG, LB_FLAG, RB_FLAG, PLAY_FLAG, BACK_FLAG, LS_CLK_FLAG, RS_CLK_FLAG};
	for (int i = 0; i < 14; i++) {
		if ((reading.Buttons & buttons[i]) == buttons[i]) {
			buttonFlags |= LiButtonFlags[i];
		}
	}
	unsigned char leftTrigger = (unsigned char)(round(reading.LeftTrigger * 255.0f));
	unsigned char rightTrigger = (unsigned char)(round(reading.RightTrigger * 255.0f));

	if (hasGamepadReadingChanged(reading, m_lastGamepadReading[controllerNumber])) {
		LiSendMultiControllerEvent(controllerNumber, activeGamepadMask, buttonFlags, leftTrigger, rightTrigger, (short)(reading.LeftThumbstickX * 32767), (short)(reading.LeftThumbstickY * 32767), (short)(reading.RightThumbstickX * 32767), (short)(reading.RightThumbstickY * 32767));
		m_lastGamepadReading[controllerNumber] = reading;
	}
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

Platform::String ^ MoonlightClient::GetInstanceID() {
	return Utils::StringFromChars(serverData.uniqueId);
}

Platform::String ^ MoonlightClient::GetComputerName() {
	return Utils::StringFromChars(serverData.serverName);
}

Platform::String ^ MoonlightClient::GetServerAddress() {
	if (!serverData.serverInfo.address) {
		return nullptr;
	}
	return Utils::StringFromChars((char *)serverData.serverInfo.address);
}

Platform::String ^ MoonlightClient::GetServerMacAddress() {
	if (!serverData.macAddress) {
		return nullptr;
	}
	return Utils::StringFromChars(serverData.macAddress);
}

void MoonlightClient::KeyDown(unsigned short v, char modifiers) {
	LiSendKeyboardEvent2(v, KEY_ACTION_DOWN, modifiers, 0);
}

void MoonlightClient::KeyUp(unsigned short v, char modifiers) {
	LiSendKeyboardEvent2(v, KEY_ACTION_UP, modifiers, 0);
}
