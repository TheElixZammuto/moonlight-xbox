#pragma once
#include "pch.h"

Platform::String^ StringPrintf(const char* fmt, ...) {
	va_list list;
	va_start(list,fmt);
	char message[2048];
	vsprintf_s(message, 2047, fmt, list);
	std::string s_str = std::string(message);
	std::wstring wid_str = std::wstring(s_str.begin(), s_str.end());
	const wchar_t* w_char = wid_str.c_str();
	Platform::String^ p_string = ref new Platform::String(w_char);
	return p_string;
}