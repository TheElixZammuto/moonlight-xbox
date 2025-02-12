//
// HostSettingsPage.xaml.h
// Declaration of the HostSettingsPage class
//

#pragma once

#include "Pages\HostSettingsPage.g.h"
#include "State\ScreenResolution.h"
#include "State\HdmiDisplayModeWrapper.h"
#include <algorithm>

namespace moonlight_xbox_dx
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	[Windows::UI::Xaml::Data::Bindable]
	public ref class HostSettingsPage sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
	{
	private:
		MoonlightHost^ host;
		Windows::Foundation::Collections::IVector<HdmiDisplayModeWrapper^>^ availableModes;
		Windows::Foundation::Collections::IVector<ScreenResolution^>^ availableStreamResolutions;
		Windows::Foundation::Collections::IVector<double>^ availableFPS;
		Windows::Foundation::Collections::IVector<Platform::String^>^ availableVideoCodecs;
		Windows::Foundation::Collections::IVector<Platform::String^>^ availableAudioConfigs;
		bool hdrAvailable;
		int currentStreamResolutionIndex = 0;
		int currentDisplayResolutionIndex = 0;
		int currentFPSIndex = 0;
		int currentAppIndex = 0;
		Windows::Foundation::EventRegistrationToken m_back_cookie;
	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
	public:
		virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
		void OnPropertyChanged(Platform::String^ propertyName);

		HostSettingsPage();
		property MoonlightHost^ Host {
			MoonlightHost^ get() {
				return this->host;
			}
		}
		void OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args);
		
		property Windows::Foundation::Collections::IVector<HdmiDisplayModeWrapper ^>^ AvailableModes {
			Windows::Foundation::Collections::IVector<HdmiDisplayModeWrapper ^>^ get() {
				if (this->availableModes == nullptr)
				{
					this->availableModes = ref new Platform::Collections::Vector<HdmiDisplayModeWrapper ^>();
				}
				return this->availableModes;
			}
		}

		property Windows::Foundation::Collections::IVector<ScreenResolution^>^ AvailableStreamResolutions {
			Windows::Foundation::Collections::IVector<ScreenResolution^>^ get() {
				if (this->availableStreamResolutions == nullptr)
				{
					this->availableStreamResolutions = ref new Platform::Collections::Vector<ScreenResolution^>();
				}
				return this->availableStreamResolutions;
			}
		}

		property Windows::Foundation::Collections::IVector<ScreenResolution^>^ AvailableDisplayResolutions {
			Windows::Foundation::Collections::IVector<ScreenResolution^>^ get() {
				auto availableTVResolutions = ref new Platform::Collections::Vector<ScreenResolution^>();
				for (int i = 0; i < availableModes->Size; i++)
				{
					bool contains = false;
					for (int j = 0; j < availableTVResolutions->Size; j++)
					{
						if (availableTVResolutions->GetAt(j)->Width == availableModes->GetAt(i)->HdmiDisplayMode->ResolutionWidthInRawPixels
							&& availableTVResolutions->GetAt(j)->Height == availableModes->GetAt(i)->HdmiDisplayMode->ResolutionHeightInRawPixels)
						{
							contains = true;
						}
					}

					if (!contains)
					{
						availableTVResolutions->Append(ref new ScreenResolution(availableModes->GetAt(i)->HdmiDisplayMode->ResolutionWidthInRawPixels,
																			  availableModes->GetAt(i)->HdmiDisplayMode->ResolutionHeightInRawPixels));
					}
				}

				return availableTVResolutions;
			}
		}

		property Windows::Foundation::Collections::IVector<double>^ AvailableFPS {
			Windows::Foundation::Collections::IVector<double>^ get() { return this->availableFPS; }
			void set(Windows::Foundation::Collections::IVector<double>^ value) {
				this->availableFPS = value;
				OnPropertyChanged("AvailableFPS");
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

		property int CurrentDisplayResolutionIndex
		{
			int get() { return this->currentDisplayResolutionIndex; }
			void set(int value) {
				this->currentDisplayResolutionIndex = value;
			}
		}

		property int CurrentStreamResolutionIndex
		{
			int get() { return this->currentStreamResolutionIndex; }
			void set(int value) {
				this->currentStreamResolutionIndex = value;
			}
		}

		property int CurrentFPSIndex
		{
			int get() { return this->currentFPSIndex; }
			void set(int value) {
				this->currentFPSIndex = value;
				OnPropertyChanged("CurrentFPSIndex");
			}
		}

		property int CurrentAppIndex
		{
			int get() { return this->currentAppIndex; }
			void set(int value) {
				this->currentAppIndex = value;
			}
		}

		property bool HDRAvailable
		{
			bool get() { return this->hdrAvailable; }
			void set(bool value) {
				this->hdrAvailable = value;
				OnPropertyChanged("HDRAvailable");
			}
		}

	private:
		void backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void StreamResolutionSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void DisplayResolutionSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void FPSSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void EnableHDR_Checked(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void AutoStartSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void GlobalSettingsOption_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void BitrateInput_KeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		HdmiDisplayModeWrapper^ FilterMode();
		Platform::Collections::Vector<double>^ UpdateFPS();
		bool IsHDRAvailable();
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}
