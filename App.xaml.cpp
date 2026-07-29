//
// App.xaml.cpp
// Implementation of the App class.
//

#include "pch.h"
#define MLOG_TAG_OVERRIDE "App"
#include <Utils.hpp>
#include "UI\Pages\MoonlightWelcome.xaml.h"
#include "UI\Pages\MoonlightSettings.xaml.h"
#include "UI\Utilities\XamlHelpers.h"
#include "UI\Utilities\ToastService.h"

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
using namespace Windows::UI::Xaml::Media::Animation;

static Windows::UI::Xaml::Controls::Frame^ s_rootFrame = nullptr;

/// <summary>
/// Initializes the singleton application object.  This is the first line of authored code
/// executed, and as such is the logical equivalent of main() or WinMain().
/// </summary>
App::App()
{
	InitializeComponent();
	Utils::InitFileLogging();
	Utils::InstallCrashHandlers();
	RequiresPointerMode = Windows::UI::Xaml::ApplicationRequiresPointerMode::WhenRequested;
	Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
	Resuming += ref new EventHandler<Object^>(this, &App::OnResuming);
	displayRequest = ref new Windows::System::Display::DisplayRequest();

	this->UnhandledException += ref new Windows::UI::Xaml::UnhandledExceptionEventHandler(
		[](Platform::Object^, Windows::UI::Xaml::UnhandledExceptionEventArgs^ e) {
			try {
				if (e != nullptr && e->Message != nullptr) {
					std::wstring msg = e->Message->Data();
					MLOGF(Utils::LogLevel::Error, "Unhandled XAML exception: %S", msg.c_str());
				}
			} catch (...) {}
			try { if (e != nullptr) e->Handled = true; } catch (...) {}
		});

	GlobalMenuItems = ref new Platform::Collections::Vector<moonlight_xbox_dx::MenuItem^>();
	GlobalMenuItems->Append(ref new moonlight_xbox_dx::MenuItem(
		ref new Platform::String(L"App Settings"),
		ref new Platform::String(L""),
		ref new Windows::Foundation::EventHandler<Platform::Object^>([](Platform::Object^ s, Platform::Object^ e) {
			try {
				if (s_rootFrame != nullptr) {
					s_rootFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightSettings::typeid), nullptr, ref new Windows::UI::Xaml::Media::Animation::EntranceNavigationTransitionInfo());
				}
			} catch(...) {}
		})
	));
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
	MLOG(moonlight_xbox_dx::Utils::LogLevel::Info, "Hello from Moonlight!\n");

	if (s_rootFrame == nullptr)
	{
		s_rootFrame = ref new Frame();
		s_rootFrame->NavigationFailed += ref new NavigationFailedEventHandler(this, &App::OnNavigationFailed);

		auto rootGrid = ref new Grid();
		rootGrid->Children->Append(s_rootFrame);
		InitializeToastService(rootGrid);
		Window::Current->Content = rootGrid;
	}

	if (s_rootFrame->Content == nullptr)
	{
		s_rootFrame->Navigate(TypeName(HostSelectorPage::typeid), e->Arguments, ref new Windows::UI::Xaml::Media::Animation::SuppressNavigationTransitionInfo());
	}

	if (m_menuPage == nullptr)
	{
		m_menuPage = dynamic_cast<HostSelectorPage^>(s_rootFrame->Content);
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
}

Windows::UI::Xaml::Controls::Frame^ App::GetRootFrame()
{
	return s_rootFrame;
}
