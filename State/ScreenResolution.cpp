#include "pch.h"
#include "MoonlightHost.h"
#include "ScreenResolution.h"
#include <ppltasks.h>

namespace moonlight_xbox_dx {
	
	void ScreenResolution::OnPropertyChanged(Platform::String^ propertyName)
	{
		PropertyChanged(this, ref new  Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
	}

}