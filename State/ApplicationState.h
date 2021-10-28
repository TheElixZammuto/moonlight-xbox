#pragma once
#include "State\MoonlightHost.h"
#include <ppltasks.h>
namespace moonlight_xbox_dx {
	public ref class ApplicationState sealed
	{
	private:
		Windows::Foundation::Collections::IVector<MoonlightHost^>^ hosts;
	internal:
		Concurrency::task<void> Init();
		Concurrency::task<void> UpdateFile();
		void RemoveHost(MoonlightHost^ host);
	public:
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
	};
}