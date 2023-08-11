#pragma once
#include "pch.h"
#include "Utils.hpp"
#define LOG_LINES 64

namespace moonlight_xbox_dx {
	namespace Utils {
		std::vector<std::wstring> logLines;
		bool showLogs = true;
		bool showStats = true;
		float outputW = 0;
		float outputH = 0;
		StreamingStats stats;
		std::mutex logMutex;
		FILE* logHandle;
		time_t start;

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
			try {
				if (fmt == nullptr || fmt == NULL)return;
				int len = strlen(fmt) + 1;
				wchar_t* stringBuf = (wchar_t*)malloc(sizeof(wchar_t) * len);
				if (stringBuf == NULL)return;
				mbstowcs(stringBuf, fmt, len);
				std::wstring string(stringBuf);
				logMutex.lock();
				if (logLines.size() == LOG_LINES)logLines.erase(logLines.begin());
				logLines.push_back(string);
				clock_t seconds_since_start = clock();
				char msg[4096];
				sprintf_s(msg, "[%f] %ws", seconds_since_start / 1000.0f, string.data());
				fwrite(msg, strlen(msg), 1, logHandle);
				fflush(logHandle);
				logMutex.unlock();
			}
			catch (...){

			}
		}

		std::vector<std::wstring> GetLogLines() {
			return logLines;
		}

		//https://stackoverflow.com/a/20707518
		Platform::String^ StringFromChars(char* chars)
		{
			int wchars_num = MultiByteToWideChar(CP_UTF8, 0, chars, -1, NULL, 0);
			wchar_t* wstr = new wchar_t[wchars_num];
			MultiByteToWideChar(CP_UTF8, 0, chars, -1, wstr, wchars_num);
			Platform::String^ str = ref new Platform::String(wstr);
			delete[] wstr;
			return str;
		}

		//https://stackoverflow.com/a/43628199
		Platform::String^ StringFromStdString(std::string input) {
			std::wstring w_str = std::wstring(input.begin(), input.end());
			const wchar_t* w_chars = w_str.c_str();
			return (ref new Platform::String(w_chars));
		}

		//https://stackoverflow.com/a/35905753
		std::string PlatformStringToStdString(Platform::String ^input) {
			std::wstring fooW(input->Begin());
			std::string fooA(fooW.begin(), fooW.end());
			return fooA;
		}
	}

}