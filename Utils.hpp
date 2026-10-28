#pragma once
#include "pch.h"

namespace moonlight_xbox_dx {
	namespace Utils {
		extern std::vector<std::wstring> logLines;
		extern bool showLogs;
		extern bool showStats;
		extern std::mutex logMutex;

		Platform::String^ StringPrintf(const char* fmt, ...);

		void Log(const char* msg);
		void Log(const std::string_view& msg);
		void Logf(const char* msg, ...);
		void DumpDeviceInformation(Windows::Devices::Enumeration::DeviceInformation^ info);
		void DumpDeviceInformation(Windows::Devices::Enumeration::DeviceInformationUpdate^ update);

		std::vector<std::wstring> GetLogLines();
		Platform::String^ StringFromChars(const char* chars);
		Platform::String^ StringFromStdString(std::string st);
		std::string PlatformStringToStdString(Platform::String^ input);
		std::string WideToNarrowString(const std::wstring_view& str);
		std::wstring NarrowToWideString(const std::string_view& str);

		// Splits "host", "host:port", "1.2.3.4:port", "[v6]", "[v6]:port" or a bare IPv6 literal
		// (which can never carry a port). The returned host has no brackets; port is 0 when absent.
		// Returns false on malformed input or a port outside 1-65535.
		bool ParseHostPort(const std::string& input, std::string& host, int& port);

		bool ShowDevTools();
	}
}
