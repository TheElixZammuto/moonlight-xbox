#pragma once
#include "State\MoonlightHost.h"
#include <ppltasks.h>

namespace moonlight_xbox_dx {

	[Windows::UI::Xaml::Data::Bindable]
	public ref class ApplicationState sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
	{
	private:
		Windows::Foundation::Collections::IVector<MoonlightHost^>^ hosts;
		int screenMarginW;
		int screenMarginH;
		int mouseSensitivity = 5;
		Platform::String^ compositionScale;
	internal:
		Concurrency::task<void> Init();
		void AddHost(Platform::String^ hostname);
		Concurrency::task<void> UpdateFile();
		void RemoveHost(MoonlightHost^ host);
		std::string autostartInstance = "";
		bool shouldAutoConnect = false;
		 
	public:
		//Thanks to https://phsucharee.wordpress.com/2013/06/19/data-binding-and-ccx-inotifypropertychanged/
		virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
		void OnPropertyChanged(Platform::String^ propertyName);
		property Windows::Foundation::Collections::IVector<MoonlightHost^>^ SavedHosts
		{
			Windows::Foundation::Collections::IVector<MoonlightHost^>^ get()
			{
				if (this->hosts == nullptr)
				{
					this->hosts = ref new Platform::Collections::Vector<MoonlightHost^>();
				}
				return this->hosts;
			};
		}
		property int ScreenMarginWidth
		{
			int get()
			{
				return this->screenMarginW;
			};
			void set(int value) {
				this->screenMarginW = value;
				OnPropertyChanged("ScreenMarginWidth");
				OnPropertyChanged("ScreenMargin");
			}
		}
		property int ScreenMarginHeight
		{
			int get()
			{
				return this->screenMarginH;
			};
			void set(int value) {
				this->screenMarginH = value;
				OnPropertyChanged("ScreenMarginHeight");
				OnPropertyChanged("ScreenMargin");
			}
		}

		property Platform::String^ CompositionScale
		{
			Platform::String^ get()
			{
				return this->compositionScale;
			};
			void set(Platform::String^ value) {
				this->compositionScale = value;
				OnPropertyChanged("CompositionScale");
			}
		}

		property Windows::UI::Xaml::Thickness ScreenMargin
		{
			Windows::UI::Xaml::Thickness get()
			{
				return { (double)this->screenMarginW,(double)this->screenMarginH,(double)this->screenMarginW,(double)this->screenMarginH };
			};
		}

		property int MouseSensitivity
		{
			int get()
			{
				return this->mouseSensitivity;
			};
			void set(int value) {
				this->mouseSensitivity = value;
				OnPropertyChanged("MouseSensitivity");
			}
		}
	};

	moonlight_xbox_dx::ApplicationState^ GetApplicationState();

}