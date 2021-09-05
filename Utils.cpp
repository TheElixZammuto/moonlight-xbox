#pragma once
#include "pch.h"
#include "Utils.hpp"
#define LOG_LINES 64

namespace moonlight_xbox_dx {
	namespace Utils {
		std::vector<std::wstring> logLines;

		Platform::String^ StringPrintf(const char* fmt, ...) {
			va_list list;
			va_start(list, fmt);
			char message[2048];
			vsprintf_s(message, 2047, fmt, list);
			std::string s_str = std::string(message);
			std::wstring wid_str = std::wstring(s_str.begin(), s_str.end());
			const wchar_t* w_char = wid_str.c_str();
			Platform::String^ p_string = ref new Platform::String(w_char);
			return p_string;
		}

		void Log(const char* fmt) {
			int len = strlen(fmt) + 1;
			if (len > 2048 || len < 1)return;
			wchar_t* stringBuf = (wchar_t*)malloc(sizeof(wchar_t) * len);
			mbstowcs(stringBuf, fmt, len);
			std::wstring string(stringBuf);
			OutputDebugStringW(stringBuf);
			if (logLines.size() == LOG_LINES)logLines.erase(logLines.begin());
			logLines.push_back(string);
		}

		std::vector<std::wstring> GetLogLines() {
			return logLines;
		}
	}

}