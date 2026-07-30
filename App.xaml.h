//
// App.xaml.h
// Declaration of the App class.
//

#pragma once

#include "App.g.h"
#include "UI\Models\MenuItem.h"
#include "UI\Pages\HostSelectorPage.xaml.h"

namespace moonlight_xbox_dx
{
		/// <summary>
	/// Provides application-specific behavior to supplement the default Application class.
	/// </summary>
	ref class App sealed
	{
	public:
		App();

		static Windows::UI::Xaml::Controls::Frame^ GetRootFrame();

		property Windows::Foundation::Collections::IObservableVector<moonlight_xbox_dx::MenuItem^>^ GlobalMenuItems;
		virtual void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e) override;
		void OnStateLoaded();
	private:
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
		void OnResuming(Platform::Object ^sender, Platform::Object ^args);
		void OnNavigationFailed(Platform::Object ^sender, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs ^e);
		HostSelectorPage^ m_menuPage;
		Windows::System::Display::DisplayRequest^ displayRequest;
	};
}
