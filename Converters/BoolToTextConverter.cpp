#include "pch.h"
#include "BoolToTextConverter.h"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::Foundation;

Object^ BoolToTextConverter::Convert(Object^ value, TypeName targetType, Object^ parameter, String^ language)
{
	bool v = (value != nullptr) && safe_cast<bool>(value);

	if (parameter != nullptr) {
		auto paramStr = safe_cast<String^>(parameter);
		const wchar_t* data = paramStr->Data();
		std::wstring ws(data ? data : L"");
		size_t pos = ws.find(L'|');
		if (pos != std::wstring::npos) {
			std::wstring wtrue = ws.substr(0, pos);
			std::wstring wfalse = ws.substr(pos + 1);
			auto truePart = ref new Platform::String(wtrue.c_str());
			auto falsePart = ref new Platform::String(wfalse.c_str());
			return v ? (Platform::Object^)truePart : (Platform::Object^)falsePart;
		}
		if (v) return (Platform::Object^)paramStr;
	}

	return v ? TrueText : FalseText;
}

Object^ BoolToTextConverter::ConvertBack(Object^ value, TypeName targetType, Object^ parameter, String^ language)
{
	return nullptr;
}
