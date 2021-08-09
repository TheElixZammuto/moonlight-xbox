#pragma once
#include "pch.h"

namespace moonlight_xbox_dx {
	namespace Utils {
		extern std::vector<std::wstring> logLines;

		Platform::String^ StringPrintf(const char* fmt, ...);

		void Log(const char* fmt);

		std::vector<std::wstring> GetLogLines();
	}
}