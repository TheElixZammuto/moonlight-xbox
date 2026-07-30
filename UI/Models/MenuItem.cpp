#include "pch.h"
#include "MenuItem.h"

using namespace moonlight_xbox_dx;

MenuItem::MenuItem()
{
    Title = ref new Platform::String(L"");
    IconGlyph = ref new Platform::String(L"");
    Tag = nullptr;
}

MenuItem::MenuItem(Platform::String ^ title, Platform::String ^ iconGlyph) {
	Title = title;
	IconGlyph = iconGlyph;
}

MenuItem::MenuItem(Platform::String ^ title, Platform::String ^ iconGlyph, Platform::Object ^ tag) {
	Title = title;
	IconGlyph = iconGlyph;
	Tag = tag;
}

MenuItem::MenuItem(Platform::String ^ title, Platform::String ^ iconGlyph, Windows::Foundation::EventHandler<Platform::Object^>^ clickAction) {
	Title = title;
	IconGlyph = iconGlyph;
	Tag = nullptr;
	ClickAction = clickAction;
}

MenuItem::MenuItem(Platform::String ^ title, Platform::String ^ iconGlyph, Platform::Object ^ tag, Windows::Foundation::EventHandler<Platform::Object^>^ clickAction) {
	Title = title;
	IconGlyph = iconGlyph;
	Tag = tag;
	ClickAction = clickAction;
}
