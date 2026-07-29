#pragma once
#include "UI\Controls\SlidingMenu.g.h"
#include "UI\Models\MenuItem.h"

namespace moonlight_xbox_dx::Controls
{
    public ref class SlidingMenu sealed
    {
    public:
        SlidingMenu();

        property Windows::Foundation::Collections::IObservableVector<moonlight_xbox_dx::MenuItem^>^ GlobalItems;

        property Windows::Foundation::Collections::IObservableVector<moonlight_xbox_dx::MenuItem^>^ PageItems;

        void AddPageItem(moonlight_xbox_dx::MenuItem^ item);
        void ClearPageItems();

        void Open();
        void Close();
        property bool IsOpen { bool get(); }
        event Windows::Foundation::TypedEventHandler<Platform::Object^, moonlight_xbox_dx::MenuItem^>^ MenuItemInvoked;
    protected:
        virtual void OnApplyTemplate() override;
    private:
        Platform::Object^ m_prevFocusedElement = nullptr;
        bool m_isOpen = false;
        void OnMenuItemClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void Overlay_Tapped(Platform::Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e);
    };
}
