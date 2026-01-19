#pragma once
#include "Pages\PreReleaseSignupPage.g.h"

namespace moonlight_xbox_dx
{
    public ref class PreReleaseSignupPage sealed
    {
    public:
        PreReleaseSignupPage();
    private:
        void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void backButton_Click(Platform::Object^ sender, Windows::UI::Core::BackRequestedEventArgs^ e);
        void OptInButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        Windows::Foundation::EventRegistrationToken m_back_token;
        bool m_isMember;
    };
}