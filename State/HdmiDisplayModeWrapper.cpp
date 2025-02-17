#include "pch.h"
#include "HdmiDisplayModeWrapper.h"
#include "MoonlightHost.h"
#include <ppltasks.h>

namespace moonlight_xbox_dx {

	void HdmiDisplayModeWrapper::OnPropertyChanged(Platform::String^ propertyName)
	{
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([this, propertyName]() {
			PropertyChanged(this, ref new  Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
			}));
	}

}