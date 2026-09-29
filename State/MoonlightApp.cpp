#include "pch.h"
#include "MoonlightApp.h"
#include "State\MoonlightClient.h"
#include "State\ApplicationState.h"
#include <ppltasks.h>

namespace moonlight_xbox_dx {
	
	double MoonlightApp::TileWidth::get()
	{
		return GetApplicationState()->TileWidth;
	}

	double MoonlightApp::TileHeight::get()
	{
		return GetApplicationState()->TileHeight;
	}

	Windows::UI::Xaml::Thickness MoonlightApp::TilePadding::get()
	{
		return GetApplicationState()->TilePadding;
	}

	void MoonlightApp::OnPropertyChanged(Platform::String^ propertyName)
	{
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
			Windows::UI::Core::CoreDispatcherPriority::High,
			ref new Windows::UI::Core::DispatchedHandler([this, propertyName]()
				{
					PropertyChanged(this, ref new  Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
				}));
	}
}