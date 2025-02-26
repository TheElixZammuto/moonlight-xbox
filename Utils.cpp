#pragma once
#include "pch.h"
#include "Utils.hpp"

#include <locale>
#include <codecvt>
#include <string>
#include <string_view>
#include <array>

constexpr auto LOG_LINES = 64;

namespace moonlight_xbox_dx {
	namespace Utils {
		std::vector<std::wstring> logLines;
		bool showLogs = false;
		bool showStats = false;
		StreamingStats stats;
		std::mutex logMutex;

		Platform::String^ StringPrintf(const char* fmt, ...) {
			va_list args;
			va_start(args, fmt);

			va_list args_copy;
			va_copy(args_copy, args);
			auto size = vsnprintf_s(nullptr, 0, 0, fmt, args_copy);
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

		void Log(const std::string_view& msg) {
			try {
				std::wstring string = NarrowToWideString(msg);
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
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
			return converter.to_bytes(str.data(), str.data() + str.size());
		}

		std::wstring NarrowToWideString(const std::string_view& str) {
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
			return converter.from_bytes(str.data(), str.data() + str.size());
		}
	}
}
