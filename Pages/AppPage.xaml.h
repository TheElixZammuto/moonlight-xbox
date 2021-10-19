//
// AppPage.xaml.h
// Declaration of the AppPage class
//

#pragma once

#include "Pages\AppPage.g.h"
#include "State\MoonlightApp.h"

namespace moonlight_xbox_dx
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class AppPage sealed
	{
	private:
		MoonlightHost^ host;
		Windows::Foundation::Collections::IVector<MoonlightApp^>^ apps;
	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
	public:
		AppPage();
		property MoonlightHost^ Host {
			MoonlightHost^ get() {
				return this->host;
			}
		}

		property Windows::Foundation::Collections::IVector<MoonlightApp^>^ Apps {
			Windows::Foundation::Collections::IVector<MoonlightApp^>^ get() {
				if (this->apps == nullptr)
				{
					this->apps = ref new Platform::Collections::Vector<MoonlightApp^>();
				}
				return this->apps;
			}
		}
	private:
		void AppsGrid_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
	};
}
