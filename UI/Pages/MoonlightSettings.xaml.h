
#pragma once

#include "UI\Pages\MoonlightSettings.g.h"

namespace moonlight_xbox_dx
{
	public ref class LogLineItem sealed
	{
	public:
		property Platform::String^ Text;
		property int Level;
		property Windows::UI::Xaml::Media::Brush^ Foreground;
	};

	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class MoonlightSettings sealed
	{
	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
		virtual void OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
	private:
		ApplicationState^ state;
		Windows::Foundation::EventRegistrationToken m_back_cookie;
		bool m_marginSliderFocused = false;
		bool m_showingLastRunLog = false;
		int m_minLogLevel = -1;
	public:
		MoonlightSettings();
		property ApplicationState^ State {
			ApplicationState^ get() {
				return this->state;
			}
		}

		void OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args);
	private:
		void backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void HostSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void WelcomeButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void LayoutSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnMarginSliderChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
		void OnMarginSliderGotFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnMarginSliderLostFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void LogViewTab_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void RefreshLogButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void LogLevelFilterSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void LogListView_LostFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void LogListView_KeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		void RefreshLogView();
		void ScrollLogViewToBottom();
	};
}
