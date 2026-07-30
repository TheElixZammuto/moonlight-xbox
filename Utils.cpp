#pragma once
#include "pch.h"
#include "Utils.hpp"

#include <chrono>
#include <cwchar>
#include <cmath>
#include <cstdlib>
#include <crtdbg.h>
#include <exception>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace std::chrono;
constexpr auto LOG_LINES = 70;
constexpr size_t MAX_LOG_READ_BYTES = 512 * 1024;

namespace moonlight_xbox_dx {
	namespace Utils {
		std::vector<std::wstring> logLines;
		bool showLogs = false;
		bool showStats = false;
		std::mutex logMutex;

		namespace {
			HANDLE g_currentLogHandle = INVALID_HANDLE_VALUE;
			std::wstring g_currentLogPath;
			std::wstring g_lastLogPath;

			Platform::String^ ReadLogFileText(const std::wstring& path) {
				if (path.empty()) {
					return ref new Platform::String(L"(log unavailable)");
				}
				HANDLE file = CreateFile2(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, nullptr);
				if (file == INVALID_HANDLE_VALUE) {
					return ref new Platform::String(L"(no log available)");
				}
				LARGE_INTEGER size{};
				if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0) {
					CloseHandle(file);
					return ref new Platform::String(L"(log is empty)");
				}

				bool truncated = false;
				LARGE_INTEGER readSize = size;
				if (static_cast<uint64_t>(size.QuadPart) > MAX_LOG_READ_BYTES) {
					truncated = true;
					readSize.QuadPart = static_cast<LONGLONG>(MAX_LOG_READ_BYTES);
					LARGE_INTEGER seekTo{};
					seekTo.QuadPart = size.QuadPart - readSize.QuadPart;
					SetFilePointerEx(file, seekTo, nullptr, FILE_BEGIN);
				}

				std::string buffer(static_cast<size_t>(readSize.QuadPart), '\0');
				DWORD bytesRead = 0;
				BOOL ok = ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr);
				CloseHandle(file);
				if (!ok) {
					return ref new Platform::String(L"(failed to read log)");
				}
				buffer.resize(bytesRead);

				std::string result = truncated ? "(...truncated, showing tail...)\n" + buffer : buffer;
				return StringFromStdString(result);
			}

			void WriteCrashLine(const char* msg) {
				if (g_currentLogHandle == INVALID_HANDLE_VALUE || msg == nullptr) return;
				std::string line(msg);
				line += '\n';
				DWORD written = 0;
				WriteFile(g_currentLogHandle, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
				FlushFileBuffers(g_currentLogHandle);
			}

			const char* ExceptionCodeName(DWORD code) {
				switch (code) {
					case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
					case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
					case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
					case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
					case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
					case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
					case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
					case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
					case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
					case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
					case EXCEPTION_BREAKPOINT: return "BREAKPOINT";
					default: return "UNKNOWN";
				}
			}

			LONG WINAPI OnUnhandledSEHException(EXCEPTION_POINTERS* info) {
				DWORD code = (info && info->ExceptionRecord) ? info->ExceptionRecord->ExceptionCode : 0;
				void* addr = (info && info->ExceptionRecord) ? info->ExceptionRecord->ExceptionAddress : nullptr;
				char buf[256];
				snprintf(buf, sizeof(buf), "*** CRASH: unhandled exception 0x%08lX (%s) at address 0x%p ***",
					code, ExceptionCodeName(code), addr);
				WriteCrashLine(buf);
				return EXCEPTION_CONTINUE_SEARCH;
			}

			void OnTerminate() {
				try {
					if (auto ex = std::current_exception()) {
						std::rethrow_exception(ex);
					}
					WriteCrashLine("*** CRASH: std::terminate called (no active exception) ***");
				} catch (Platform::Exception^ ex) {
					char buf[512];
					std::wstring msg = ex->Message != nullptr ? std::wstring(ex->Message->Data()) : L"";
					snprintf(buf, sizeof(buf), "*** CRASH: unhandled Platform::Exception (HRESULT 0x%08X): %S ***",
						ex->HResult, msg.c_str());
					WriteCrashLine(buf);
				} catch (const std::exception& ex) {
					char buf[512];
					snprintf(buf, sizeof(buf), "*** CRASH: unhandled C++ exception: %s ***", ex.what());
					WriteCrashLine(buf);
				} catch (...) {
					WriteCrashLine("*** CRASH: unhandled exception of unknown type ***");
				}
				std::abort();
			}

			void OnPureCall() {
				WriteCrashLine("*** CRASH: pure virtual function call ***");
				std::abort();
			}

			void OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
				WriteCrashLine("*** CRASH: invalid parameter passed to a CRT function ***");
				std::abort();
			}
		}

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
			auto now = system_clock::now();
			auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
			std::time_t tt = system_clock::to_time_t(now);
			std::tm local_tm{};
			localtime_s(&local_tm, &tt);

			wchar_t buffer[32];
			swprintf(buffer, 32, L"[%02d:%02d:%02d.%03d] ",
			         local_tm.tm_hour,
			         local_tm.tm_min,
			         local_tm.tm_sec,
			         static_cast<int>(ms.count()));
			return std::wstring(buffer);
		}

		const wchar_t* LogLevelTag(LogLevel level) {
			switch (level) {
				case LogLevel::Verbose: return L"VRB";
				case LogLevel::Debug:   return L"DBG";
				case LogLevel::Info:    return L"INF";
				case LogLevel::Warning: return L"WRN";
				case LogLevel::Error:   return L"ERR";
			}
			return L"INF";
		}

		LogLevel ParseLogLevelFromLine(const std::wstring& line) {

			if (line.find(L"[ERR]") != std::wstring::npos || line.find(L"{ERR}") != std::wstring::npos) return LogLevel::Error;
			if (line.find(L"[WRN]") != std::wstring::npos || line.find(L"{WRN}") != std::wstring::npos) return LogLevel::Warning;
			if (line.find(L"[INF]") != std::wstring::npos || line.find(L"{INF}") != std::wstring::npos) return LogLevel::Info;
			if (line.find(L"[DBG]") != std::wstring::npos || line.find(L"{DBG}") != std::wstring::npos) return LogLevel::Debug;
			if (line.find(L"[VRB]") != std::wstring::npos || line.find(L"{VRB}") != std::wstring::npos) return LogLevel::Verbose;

			return LogLevel::Error;
		}

		void Log(LogLevel level, const std::string_view& msg) {
			try {
				std::wstring tag = std::wstring(L"[") + LogLevelTag(level) + L"] ";
				std::wstring string = GetCurrentTimestamp() + tag + NarrowToWideString(msg);
        OutputDebugString(string.c_str());
				{
					std::unique_lock<std::mutex> lk(logMutex);
					if (g_currentLogHandle != INVALID_HANDLE_VALUE) {
						std::string line = WideToNarrowString(string);
						if (line.empty() || line.back() != '\n') line += '\n';
						DWORD written = 0;
						WriteFile(g_currentLogHandle, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
					}
					if (logLines.size() == LOG_LINES) {
						logLines.erase(logLines.begin());
					}
					std::wstring displayString = string;
					for (auto& ch : displayString) {
						// ModeSeven renders [ ] as left and right arrows, so we replace them
						// with { } which render as brackets
						if (ch == L'[') {
							ch = L'{';
						}
						else if (ch == L']') {
							ch = L'}';
						}
					}
					logLines.push_back(displayString);
				}
			}
			catch (...) {

			}
		}

		void Log(LogLevel level, const char* msg) {
			if (msg) {
				Log(level, std::string_view(msg));
			}
		}

		void Logf(LogLevel level, const char* format, ...) {
			va_list args;
			va_start(args, format);

			char buf[1024];
			std::vsnprintf(buf, sizeof(buf) - 1, format, args);
			va_end(args);

			Log(level, std::string_view(buf));
		}

		std::string TagFromFunction(const char* function) {
			if (!function) return std::string();
			std::string_view sv(function);
			size_t lastSep = sv.rfind("::");
			if (lastSep == std::string_view::npos) {
				return std::string(sv);
			}
			std::string_view withoutMethod = sv.substr(0, lastSep);
			size_t prevSep = withoutMethod.rfind("::");
			std::string_view scope = (prevSep == std::string_view::npos)
				? withoutMethod
				: withoutMethod.substr(prevSep + 2);
			return std::string(scope);
		}

		void LogTagged(LogLevel level, const std::string& tag, const char* msg) {
			Log(level, "[" + tag + "] " + (msg ? msg : ""));
		}

		void LogfTagged(LogLevel level, const std::string& tag, const char* format, ...) {
			va_list args;
			va_start(args, format);

			char buf[1024];
			std::vsnprintf(buf, sizeof(buf) - 1, format, args);
			va_end(args);

			LogTagged(level, tag, buf);
		}

		std::vector<std::wstring> GetLogLines() {
			return logLines;
		}

		void InitFileLogging() {
			try {
				std::wstring folder(Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data());

				std::unique_lock<std::mutex> lk(logMutex);
				g_currentLogPath = folder + L"\\current_run.log";
				g_lastLogPath = folder + L"\\last_run.log";

				DeleteFileW(g_lastLogPath.c_str());
				MoveFileExW(g_currentLogPath.c_str(), g_lastLogPath.c_str(), MOVEFILE_REPLACE_EXISTING);

				if (g_currentLogHandle != INVALID_HANDLE_VALUE) {
					CloseHandle(g_currentLogHandle);
				}
				g_currentLogHandle = CreateFile2(g_currentLogPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS, nullptr);
			}
			catch (...) {

			}
		}

		void InstallCrashHandlers() {
			SetUnhandledExceptionFilter(&OnUnhandledSEHException);
			std::set_terminate(&OnTerminate);
			_set_purecall_handler(&OnPureCall);
			_set_invalid_parameter_handler(&OnInvalidParameter);
		}

		Platform::String^ ReadCurrentRunLogText() {
			std::wstring path;
			{
				std::unique_lock<std::mutex> lk(logMutex);
				path = g_currentLogPath;
			}
			return ReadLogFileText(path);
		}

		Platform::String^ ReadLastRunLogText() {
			std::wstring path;
			{
				std::unique_lock<std::mutex> lk(logMutex);
				path = g_lastLogPath;
			}
			return ReadLogFileText(path);
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

		std::string Join(const std::vector<std::string>& parts, const std::string& separator) {
			std::string result;
			for (size_t i = 0; i < parts.size(); i++) {
				if (i > 0) result += separator;
				result += parts[i];
			}
			return result;
		}

		double DurationStringToMs(Platform::String^ durationValue) {
			if (durationValue == nullptr || durationValue->IsEmpty()) return 250.0;

			std::wstring text(durationValue->Data());
			std::wstringstream ss(text);
			std::wstring segment;
			std::vector<double> parts;

			while (std::getline(ss, segment, L':')) {
				if (segment.empty()) return 250.0;
				try {
					size_t idx = 0;
					double value = std::stod(segment, &idx);
					if (idx != segment.size()) return 250.0;
					parts.push_back(value);
				} catch (...) {
					return 250.0;
				}
			}

			double totalSeconds = 0.0;
			if (parts.size() == 3) {
				totalSeconds = (parts[0] * 3600.0) + (parts[1] * 60.0) + parts[2];
			} else if (parts.size() == 2) {
				totalSeconds = (parts[0] * 60.0) + parts[1];
			} else if (parts.size() == 1) {
				totalSeconds = parts[0];
			} else {
				return 250.0;
			}

			if (!std::isfinite(totalSeconds) || totalSeconds <= 0.0) return 250.0;
			return totalSeconds * 1000.0;
		}

	}
}
