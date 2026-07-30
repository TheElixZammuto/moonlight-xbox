#pragma once

namespace moonlight_xbox_dx
{
    public ref class MenuItem sealed
    {
    public:
    MenuItem();
    MenuItem(Platform::String ^ title, Platform::String ^ iconGlyph);
    MenuItem(Platform::String ^ title, Platform::String ^ iconGlyph, Platform::Object ^ tag);

    MenuItem(Platform::String ^ title, Platform::String ^ iconGlyph, Windows::Foundation::EventHandler<Platform::Object^>^ clickAction);

    MenuItem(Platform::String ^ title, Platform::String ^ iconGlyph, Platform::Object ^ tag, Windows::Foundation::EventHandler<Platform::Object^>^ clickAction);

        property Platform::String^ Title;
        property Platform::String^ IconGlyph;
        property Platform::Object^ Tag;
        property Windows::Foundation::EventHandler<Platform::Object^>^ ClickAction;
    };
}
