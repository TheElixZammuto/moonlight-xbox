#pragma once
#include "pch.h"

namespace moonlight_xbox_dx {
	namespace Utils {
		struct StreamingStats {
			double averageRenderingTime = 0;
			int queueSize = 0;
			int fps = 0;
			double _accumulatedSeconds = 0;
			int _framesDecoded = 0;
		};
		
		extern std::vector<std::wstring> logLines;
		extern bool showLogs;
		extern bool showStats;
		extern float outputW;
		extern float outputH;
		extern StreamingStats stats;
		extern std::mutex logMutex;
		
		Platform::String^ StringPrintf(const char* fmt, ...);

		void Log(const char* fmt);

		std::vector<std::wstring> GetLogLines();
		Platform::String^ StringFromChars(char* chars);
		Platform::String^ StringFromStdString(std::string st);
		std::string PlatformStringToStdString(Platform::String^ input);

	}
}