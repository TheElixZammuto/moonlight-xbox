#pragma once

#include "UI\Controls\TabsLayout.g.h"

namespace moonlight_xbox_dx
{
    public ref class TabsLayout sealed
    {
    public:
        TabsLayout();

        property Windows::UI::Xaml::UIElement^ LeftContent {
            Windows::UI::Xaml::UIElement^ get();
            void set(Windows::UI::Xaml::UIElement^ value);
        }

        property Windows::UI::Xaml::UIElement^ TabsContent {
            Windows::UI::Xaml::UIElement^ get();
            void set(Windows::UI::Xaml::UIElement^ value);
        }

        property Windows::UI::Color AccentColor {
            Windows::UI::Color get() { return m_accentColor; }
            void set(Windows::UI::Color value);
        }

        property Windows::UI::Xaml::Controls::ScrollViewer^ ContentScrollViewer {
            Windows::UI::Xaml::Controls::ScrollViewer^ get() { return ContentSV; }
        }

        void FocusFirstTab();

        static void SetTargetPanelName(Windows::UI::Xaml::UIElement^ element, Platform::String^ value);
        static Platform::String^ GetTargetPanelName(Windows::UI::Xaml::UIElement^ element);

        static void SetTargetPanel(Windows::UI::Xaml::UIElement^ element, Windows::UI::Xaml::UIElement^ value);
        static Windows::UI::Xaml::UIElement^ GetTargetPanel(Windows::UI::Xaml::UIElement^ element);

    private:
        void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void HookUpTabs();
        void UnhookTabs();
        void SelectTabByName(Platform::String^ panelName);
        void OnTabButtonClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnTabButtonGotFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnContentScrollViewerKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);

        void AnimateSelectionToButton(Windows::UI::Xaml::Controls::Button^ btn, bool animate);
        void AnimateForeground(Windows::UI::Xaml::Controls::Button^ btn, bool selected);

        Windows::Foundation::Collections::IVector<Windows::UI::Xaml::Controls::Button^>^ m_tabButtons;
        Windows::Foundation::Collections::IVector<long long>^ m_clickTokens;
        Windows::Foundation::Collections::IVector<long long>^ m_focusTokens;
        Windows::UI::Xaml::Controls::Button^ m_selectedButton;
        Windows::UI::Xaml::Media::Animation::Storyboard^ m_slideStoryboard;
        Windows::UI::Xaml::Media::Animation::Storyboard^ m_fadeStoryboard;
        bool m_selectionIndicatorReady;
        double m_indicatorY;

        static Windows::UI::Xaml::DependencyProperty^ TargetPanelNameProperty;
        static Windows::UI::Xaml::DependencyProperty^ TargetPanelProperty;

        void SetIsSelected(Windows::UI::Xaml::UIElement^ element, bool value);
        static bool GetIsSelected(Windows::UI::Xaml::UIElement^ element);
        static Windows::UI::Xaml::DependencyProperty^ IsSelectedProperty;
        Windows::UI::Color m_accentColor;
    };
}
