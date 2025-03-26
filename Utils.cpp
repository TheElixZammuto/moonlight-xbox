#pragma once
#include "pch.h"
#include "Utils.hpp"

#include <chrono>
#include <cwchar>
#include <string>
#include <string_view>
#include <vector>

using namespace std::chrono;

constexpr auto LOG_LINES = 70;

namespace moonlight_xbox_dx {
	namespace Utils {
		std::vector<std::wstring> logLines;
		bool showLogs = false;
		bool showStats = false;
		std::mutex logMutex;

		Platform::String^ StringPrintf(const char* fmt, ...) {
			va_list args;
			va_start(args, fmt);

			va_list args_copy;
			va_copy(args_copy, args);
			auto size = vsnprintf(nullptr, 0, fmt, args_copy);
			va_end(args_copy);

			if (size < 0) {
				va_end(args);
				return nullptr;
			}

			// Needs space for NUL char
			std::vector<char> message(size + 1, 0);
			vsnprintf_s(message.data(), message.size(), message.size(), fmt, args);
			va_end(args);

			return ref new Platform::String(NarrowToWideString(std::string_view(message.data())).c_str());
		}

		std::wstring GetCurrentTimestamp() {
			auto now = system_clock::now().time_since_epoch();
			auto min = duration_cast<minutes>(now) % 60;
			auto sec = duration_cast<seconds>(now) % 60;
			auto ms  = duration_cast<milliseconds>(now) % 1000;

			wchar_t buffer[16];
			swprintf(buffer, 16, L"[%02d:%02d.%03d] ",
					 static_cast<int>(min.count()),
					 static_cast<int>(sec.count()),
					 static_cast<int>(ms.count()));
			return std::wstring(buffer);
		}

		void Log(const std::string_view& msg) {
			try {
				std::wstring string = GetCurrentTimestamp() + NarrowToWideString(msg);
				{
					std::unique_lock<std::mutex> lk(logMutex);
					if (logLines.size() == LOG_LINES) {
						logLines.erase(logLines.begin());
					}
					logLines.push_back(string);
				}
				OutputDebugString(string.c_str());
			}
			catch (...) {

			}
		}

		void Log(const char* msg) {
			if (msg) {
				Log(std::string_view(msg));
			}
		}

		void Logf(const char* format, ...) {
			va_list args;
			va_start(args, format);

			char buf[1024];
			std::vsnprintf(buf, sizeof(buf) - 1, format, args);
			va_end(args);

			Log(std::string_view(buf));
		}

		std::vector<std::wstring> GetLogLines() {
			return logLines;
		}

		Platform::String^ StringFromChars(const char* chars)
		{
			if (chars == nullptr) {
				return nullptr;
			}
			return ref new Platform::String(NarrowToWideString(std::string_view(chars)).c_str());
		}

		Platform::String^ StringFromStdString(std::string input) {
			return ref new Platform::String(NarrowToWideString(input).c_str());
		}

		std::string PlatformStringToStdString(Platform::String ^input) {
			return WideToNarrowString(std::wstring(input->Begin()));
		}

		std::string WideToNarrowString(const std::wstring_view& str) {
			auto bufferSize = WideCharToMultiByte(CP_UTF8,
				0,
				str.data(),
				str.length(),
				nullptr,
				0, nullptr, nullptr);

			std::string result;
			result.resize(bufferSize);
			WideCharToMultiByte(CP_UTF8,
				0,
				str.data(),
				str.length(),
				result.data(),
				result.size(), nullptr, nullptr);

			return result;
		}

		std::wstring NarrowToWideString(const std::string_view& str) {
			auto bufferSize = MultiByteToWideChar(CP_UTF8,
				0,
				str.data(),
				str.length(),
				nullptr,
				0);

			std::wstring result;
			result.resize(bufferSize);
			MultiByteToWideChar(CP_UTF8,
				0,
				str.data(),
				str.length(),
				result.data(),
				result.size());

			return result;
		}
	}
}
