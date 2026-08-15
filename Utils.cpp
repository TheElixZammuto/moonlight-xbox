#pragma once

// clang-format off
#include "pch.h"
// clang-format on
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

		void Log(const std::string_view& msg) {
			try {
				std::wstring string = GetCurrentTimestamp() + NarrowToWideString(msg);
				OutputDebugString(string.c_str());
				TracyMessage(msg.data(), msg.size());
				{
					std::unique_lock<std::mutex> lk(logMutex);
					if (logLines.size() == LOG_LINES) {
						logLines.erase(logLines.begin());
					}
					for (auto& ch : string) {
						// ModeSeven renders [ ] as left and right arrows, so we replace them
						// with { } which render as brackets
						if (ch == L'[') {
							ch = L'{';
						}
						else if (ch == L']') {
							ch = L'}';
						}
					}
					logLines.push_back(string);
				}
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

		namespace {
			std::string DebugString(Platform::String^ value) {
				if (value == nullptr) return "<null>";
				std::string result = "\"";
				for (char ch : PlatformStringToStdString(value)) {
					switch (ch) {
					case '\\': result += "\\\\"; break;
					case '"': result += "\\\""; break;
					case '\n': result += "\\n"; break;
					case '\r': result += "\\r"; break;
					case '\t': result += "\\t"; break;
					case '\0': result += "\\0"; break;
					default: result += ch; break;
					}
				}
				return result + "\"";
			}

			std::string DebugProperty(Platform::Object^ value) {
				using namespace Windows::Foundation;
				if (value == nullptr) return "<null>";
				auto property = dynamic_cast<IPropertyValue^>(value);
				if (property != nullptr) {
					switch (property->Type) {
					case PropertyType::Empty: return "<empty>";
					case PropertyType::String: return DebugString(property->GetString());
					case PropertyType::Boolean: return property->GetBoolean() ? "true" : "false";
					case PropertyType::StringArray: {
						// DNS-SD IP addresses and TXT attributes are string arrays.
						Platform::Array<Platform::String^>^ items;
						property->GetStringArray(&items);
						std::string result = "[";
						for (unsigned int i = 0; i < items->Length; ++i) {
							if (i != 0) result += ", ";
							result += DebugString(items[i]);
						}
						return result + "]";
					}
					case PropertyType::UInt8Array: {
						Platform::Array<unsigned char>^ items;
						property->GetUInt8Array(&items);
						std::string result = "[";
						for (unsigned int i = 0; i < items->Length; ++i) {
							if (i != 0) result += ", ";
							result += std::to_string(items[i]);
						}
						return result + "]";
					}
					default: break;
					}
				}
				// Boxed scalar values provide their textual representation here.
				return PlatformStringToStdString(value->ToString());
			}

			void DumpDeviceProperties(Windows::Foundation::Collections::IMapView<Platform::String^, Platform::Object^>^ properties) {
				if (properties == nullptr || properties->Size == 0) {
					Log("  Properties: <empty>\n");
					return;
				}
				for (auto entry : properties) {
					auto key = PlatformStringToStdString(entry->Key);
					try {
						// Use Log directly so large TXT arrays avoid Logf's fixed-size buffer.
						Log("  " + key + " = " + DebugProperty(entry->Value) + "\n");
					}
					catch (Platform::Exception^ ex) {
						Logf("  %s = <unreadable; HRESULT=0x%08X>\n", key.c_str(), static_cast<unsigned int>(ex->HResult));
					}
				}
			}
		}

		void DumpDeviceInformation(Windows::Devices::Enumeration::DeviceInformation^ info) {
			if (info == nullptr) {
				Log("DeviceInformation: <null>\n");
				return;
			}
			Log("DeviceInformation:\n");
			Log("  Id = " + DebugString(info->Id) + "\n");
			Log("  Name = " + DebugString(info->Name) + "\n");
			Logf("  Kind = %d, IsEnabled = %s, IsDefault = %s\n", static_cast<int>(info->Kind),
				info->IsEnabled ? "true" : "false", info->IsDefault ? "true" : "false");
			if (info->Pairing != nullptr) {
				Logf("  Pairing: IsPaired = %s, CanPair = %s\n",
					info->Pairing->IsPaired ? "true" : "false", info->Pairing->CanPair ? "true" : "false");
			}
			DumpDeviceProperties(info->Properties);
		}

		void DumpDeviceInformation(Windows::Devices::Enumeration::DeviceInformationUpdate^ update) {
			if (update == nullptr) {
				Log("DeviceInformationUpdate: <null>\n");
				return;
			}
			Log("DeviceInformationUpdate (changed properties only):\n");
			Log("  Id = " + DebugString(update->Id) + "\n");
			Logf("  Kind = %d\n", static_cast<int>(update->Kind));
			DumpDeviceProperties(update->Properties);
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

		std::string PlatformStringToStdString(Platform::String^ input) {
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

		bool ShowDevTools() {
			// Allow Frame Capture with Andy's build
			if (Windows::ApplicationModel::Package::Current->DisplayName == "Moonlight UWP (AndyG)") {
				return true;
			}

			// I think this check for Dev Mode only works when the app is deployed from Visual Studio
			if (Windows::ApplicationModel::Package::Current->IsDevelopmentMode) {
				return true;
			}

			return false;
		}
	}
}
