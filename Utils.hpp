#pragma once
#include "pch.h"

namespace moonlight_xbox_dx {
	namespace Utils {
		enum class LogLevel {
			Verbose = 0,
			Debug = 1,
			Info = 2,
			Warning = 3,
			Error = 4
		};

		extern std::vector<std::wstring> logLines;
		extern bool showLogs;
		extern bool showStats;
		extern std::mutex logMutex;

		Platform::String^ StringPrintf(const char* fmt, ...);

		void Log(LogLevel level, const char* msg);
		void Log(LogLevel level, const std::string_view& msg);
		void Logf(LogLevel level, const char* msg, ...);

		std::string TagFromFunction(const char* function);
			void LogTagged(LogLevel level, const std::string& tag, const char* msg);
			void LogfTagged(LogLevel level, const std::string& tag, const char* format, ...);

			const wchar_t* LogLevelTag(LogLevel level);
		LogLevel ParseLogLevelFromLine(const std::wstring& line);

		void InitFileLogging();
		void InstallCrashHandlers();
		Platform::String^ ReadCurrentRunLogText();
		Platform::String^ ReadLastRunLogText();

		std::vector<std::wstring> GetLogLines();
		Platform::String^ StringFromChars(const char* chars);
		Platform::String^ StringFromStdString(std::string st);
		std::string PlatformStringToStdString(Platform::String^ input);
		std::string WideToNarrowString(const std::wstring_view& str);
		std::wstring NarrowToWideString(const std::string_view& str);
			std::string Join(const std::vector<std::string>& parts, const std::string& separator);

		double DurationStringToMs(Platform::String^ durationValue);
	}
}

#ifdef MLOG_TAG_OVERRIDE
#define MLOG_TAG (MLOG_TAG_OVERRIDE)
#else
#define MLOG_TAG (moonlight_xbox_dx::Utils::TagFromFunction(__FUNCTION__))
#endif

#define MLOG(level, msg) moonlight_xbox_dx::Utils::LogTagged((level), MLOG_TAG, (msg))
#define MLOGF(level, fmt, ...) moonlight_xbox_dx::Utils::LogfTagged((level), MLOG_TAG, (fmt), ##__VA_ARGS__)
