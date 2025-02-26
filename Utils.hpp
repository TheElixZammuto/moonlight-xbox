#pragma once
#include "pch.h"

namespace moonlight_xbox_dx {
	namespace Utils {
		struct StreamingStats {
			double averageRenderingTime = 0;
			double averageNetworkTime = 0;
			double averageTotalTime = 0;
			int queueSize = 0;
			int fps = 0;
			double _accumulatedSeconds = 0;
			double _accumulatedSecondsEnd = 0;
			int _framesDecoded = 0;
			LARGE_INTEGER _timeAtEnd;
			float outputW = 0;
			float outputH = 0;
			float compositionScaleX = 0;
			float compositionScaleY = 0;
			float compositionScaleMultiplier = 0;
		};
		
		extern std::vector<std::wstring> logLines;
		extern bool showLogs;
		extern bool showStats;
		extern float outputW;
		extern float outputH;
		extern StreamingStats stats;
		extern std::mutex logMutex;
		
		Platform::String^ StringPrintf(const char* fmt, ...);

		void Log(const char* msg);
		void Log(const std::string_view& msg);

		std::vector<std::wstring> GetLogLines();
		Platform::String^ StringFromChars(const char* chars);
		Platform::String^ StringFromStdString(std::string st);
		std::string PlatformStringToStdString(Platform::String^ input);
		std::string WideToNarrowString(const std::wstring_view& str);
		std::wstring NarrowToWideString(const std::string_view& str);	}
}