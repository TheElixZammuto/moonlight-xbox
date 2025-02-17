﻿//
// HostSettingsPage.xaml.h
// Declaration of the HostSettingsPage class
//

#pragma once

#include "Pages\HostSettingsPage.g.h"
#include "State\ScreenResolution.h"
#include "State\HdmiDisplayModeWrapper.h"

namespace moonlight_xbox_dx
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class HostSettingsPage sealed
	{
	private:
		MoonlightHost^ host;
		Windows::Foundation::Collections::IVector<HdmiDisplayModeWrapper^>^ availableResolutions;
		Windows::Foundation::Collections::IVector<Platform::String^>^ availableVideoCodecs;
		Windows::Foundation::Collections::IVector<Platform::String^>^ availableAudioConfigs;
		int currentResolutionIndex = 0;
		int currentAppIndex = 0;
	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
	public:
		HostSettingsPage();
		property MoonlightHost^ Host {
			MoonlightHost^ get() {
				return this->host;
			}
		}
		void OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args);
		
		property Windows::Foundation::Collections::IVector<HdmiDisplayModeWrapper ^>^ AvailableResolutions {
			Windows::Foundation::Collections::IVector<HdmiDisplayModeWrapper ^>^ get() {
				if (this->availableResolutions == nullptr)
				{
					this->availableResolutions = ref new Platform::Collections::Vector<HdmiDisplayModeWrapper ^>();
				}
				return this->availableResolutions;
			}
		}

		property Windows::Foundation::Collections::IVector<Platform::String^>^ AvailableVideoCodecs {
			Windows::Foundation::Collections::IVector<Platform::String^>^ get() {
				if (this->availableVideoCodecs == nullptr)
				{
					this->availableVideoCodecs = ref new Platform::Collections::Vector<Platform::String^>();
				}
				return this->availableVideoCodecs;
			}
		}

		property Windows::Foundation::Collections::IVector<Platform::String^>^ AvailableAudioConfigs {
			Windows::Foundation::Collections::IVector<Platform::String^>^ get() {
				if (this->availableAudioConfigs == nullptr)
				{
					this->availableAudioConfigs = ref new Platform::Collections::Vector<Platform::String^>();
				}
				return this->availableAudioConfigs;
			}
		}

		property int CurrentResolutionIndex
		{
			int get() { return this->currentResolutionIndex; }
			void set(int value) {
				this->currentResolutionIndex = value;
			}
		}

		property int CurrentAppIndex
		{
			int get() { return this->currentAppIndex; }
			void set(int value) {
				this->currentAppIndex = value;
			}
		}

	private:
		void backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ResolutionSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void AutoStartSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void GlobalSettingsOption_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void BitrateInput_KeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		std::string FormatMode(Windows::Graphics::Display::Core::HdmiDisplayMode mode);
	};
}
