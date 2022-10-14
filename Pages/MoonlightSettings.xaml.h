//
// MoonlightSettings.xaml.h
// Dichiarazione della classe MoonlightSettings
//

#pragma once

#include "Pages\MoonlightSettings.g.h"

namespace moonlight_xbox_dx
{
	/// <summary>
	/// Pagina vuota che può essere usata autonomamente oppure per l'esplorazione all'interno di un frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class MoonlightSettings sealed
	{
	private: 
		ApplicationState^ state;
		Windows::Foundation::Collections::IVector<Platform::String^>^ availableCompositionScale;
		int compositionScaleIndex = 0;
	public:
		MoonlightSettings();
		property ApplicationState^ State {
			ApplicationState^ get() {
				return this->state;
			}
		}

		property Windows::Foundation::Collections::IVector<Platform::String^>^ AvailableCompositionScale {
			Windows::Foundation::Collections::IVector<Platform::String^>^ get() {
				if (this->availableCompositionScale == nullptr)
				{
					this->availableCompositionScale = ref new Platform::Collections::Vector<Platform::String^>();
				}
				return this->availableCompositionScale;
			}
		}

		property int CompositionScaleIndex
		{
			int get() { return this->compositionScaleIndex; }
			void set(int value) {
				this->compositionScaleIndex = value;
			}
		}

		void OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args);
	private:
		void backButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void HostSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
		void CompositionScaleSelector_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
	};
}
