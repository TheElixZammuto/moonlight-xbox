//
// HostSettingsPage.xaml.h
// Declaration of the HostSettingsPage class
//

#pragma once

#include "Pages\HostSettingsPage.g.h"
#include "State\ScreenResolution.h"

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
		Windows::Foundation::Collections::IVector<ScreenResolution^>^ availableResolutions;
		Windows::Foundation::Collections::IVector<int>^ availableFps;
		Windows::Foundation::Collections::IVector<Platform::String^>^ availableAudioConfigs;
		int currentResolutionIndex = 0;
		int currentFPSIndex = 0;
		int currentAudioConfigIndex = 0;
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
		
		property Windows::Foundation::Collections::IVector<ScreenResolution^>^ AvailableResolutions {
			Windows::Foundation::Collections::IVector<ScreenResolution^>^ get() {
				if (this->availableResolutions == nullptr)
				{
					this->availableResolutions = ref new Platform::Collections::Vector<ScreenResolution^>();
				}
				return this->availableResolutions;
			}
		}

		property Windows::Foundation::Collections::IVector<int>^ AvailableFPS {
			Windows::Foundation::Collections::IVector<int>^ get() {
				if (this->availableFps == nullptr)
				{
					this->availableFps = ref new Platform::Collections::Vector<int>();
				}
				return this->availableFps;
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

		property int CurrentFPSIndex
		{
			int get() { return this->currentFPSIndex; }
			void set(int value) {
				this->currentFPSIndex = value;
			}
		}

		property int CurrentAudioConfigIndex
		{
			int get() { return this->currentAudioConfigIndex; }
			void set(int value) {
				this->currentAudioConfigIndex = value;
			}
		}
	private:
		void backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ResolutionSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void BitrateInput_TextChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e);
	};
}
