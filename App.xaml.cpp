//
// App.xaml.cpp
// Implementation of the App class.
//

#include "pch.h"
#include <Utils.hpp>
#include "MoonlightWelcome.xaml.h"
#include "State/MDNSHandler.h"

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
/// <summary>
/// Initializes the singleton application object.  This is the first line of authored code
/// executed, and as such is the logical equivalent of main() or WinMain().
/// </summary>
App::App()
{
	InitializeComponent();
	RequiresPointerMode = Windows::UI::Xaml::ApplicationRequiresPointerMode::WhenRequested;
	Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
	Resuming += ref new EventHandler<Object^>(this, &App::OnResuming);
	displayRequest = ref new Windows::System::Display::DisplayRequest();
}

/// <summary>
/// Invoked when the application is launched normally by the end user.  Other entry points
/// will be used when the application is launched to open a specific file, to display
/// search results, and so forth.
/// </summary>
/// <param name="e">Details about the launch request and process.</param>
void App::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
{
// #if _DEBUG
// 	if (IsDebuggerPresent())
// 	{
// 		DebugSettings->EnableFrameRateCounter = true;
// 	}
// #endif
	moonlight_xbox_dx::Utils::Log("Hello from Moonlight!\n");
	auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);

	// Do not repeat app initialization when the Window already has content,
	// just ensure that the window is active
	if (rootFrame == nullptr)
	{
		// Create a Frame to act as the navigation context and associate it with
		// a SuspensionManager key
		rootFrame = ref new Frame();

		rootFrame->NavigationFailed += ref new Windows::UI::Xaml::Navigation::NavigationFailedEventHandler(this, &App::OnNavigationFailed);

		// Place the frame in the current Window
		Window::Current->Content = rootFrame;
	}

	if (rootFrame->Content == nullptr)
	{
		// When the navigation stack isn't restored navigate to the first page,
		// configuring the new page by passing required information as a navigation
		// parameter
		rootFrame->Navigate(TypeName(HostSelectorPage::typeid), e->Arguments);
	}

	if (m_menuPage == nullptr)
	{
		m_menuPage = dynamic_cast<HostSelectorPage^>(rootFrame->Content);
	}
	// Ensure the current window is active
	Window::Current->Activate();
	//Start the state
	auto state = GetApplicationState();
	auto that = this;
	state->Init().then([that](){
		that->m_menuPage->OnStateLoaded();
	});
	displayRequest->RequestActive();
}
/// <summary>
/// Invoked when application execution is being suspended.  Application state is saved
/// without knowing whether the application will be terminated or resumed with the contents
/// of memory still intact.
/// </summary>
/// <param name="sender">The source of the suspend request.</param>
/// <param name="e">Details about the suspend request.</param>
void App::OnSuspending(Object^ sender, SuspendingEventArgs^ e)
{
	(void) sender;	// Unused parameter
	(void) e;	// Unused parameter
	displayRequest->RequestRelease();

	try {
		close_mdns();
	} catch(...) {}
}

/// <summary>
/// Invoked when application execution is being resumed.
/// </summary>
/// <param name="sender">The source of the resume request.</param>
/// <param name="args">Details about the resume request.</param>
void App::OnResuming(Object ^sender, Object ^args)
{
	(void) sender; // Unused parameter
	(void) args; // Unused parameter
	displayRequest->RequestActive();

	try {
		reinit_mdns();
	} catch(...) {}
}

/// <summary>
/// Invoked when Navigation to a certain page fails
/// </summary>
/// <param name="sender">The Frame which failed navigation</param>
/// <param name="e">Details about the navigation failure</param>
void App::OnNavigationFailed(Platform::Object ^sender, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs ^e)
{
	e->Handled = true;
	Windows::UI::Xaml::Controls::ContentDialog^ dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
	dialog->Content = e->Exception.ToString();
	dialog->CloseButtonText = L"OK";
	dialog->ShowAsync();
	//throw ref new FailureException("Failed to load Page " + e->SourcePageType.Name);
}

