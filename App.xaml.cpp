//
// App.xaml.cpp
// Implementation of the App class.
//

#include "pch.h"
#include <Utils.hpp>
#include "MoonlightWelcome.xaml.h"
#include "Pages\StreamPage.xaml.h"

#include <cerrno>
#include <climits>

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

void App::InitializeState()
{
	if (m_stateLoaded) {
		if (m_menuPage != nullptr) {
			m_menuPage->OnStateLoaded();
		}
		return;
	}

	if (m_initStarted) {
		return;
	}

	m_initStarted = true;
	auto that = this;
	try {
		m_initTask = GetApplicationState()->Init();
	}
	catch (const std::exception& ex) {
		m_initStarted = false;
		moonlight_xbox_dx::Utils::Logf("Application state initialization exception: %s\n", ex.what());
		return;
	}
	catch (...) {
		m_initStarted = false;
		moonlight_xbox_dx::Utils::Log("Application state initialization unknown exception\n");
		return;
	}

	m_initTask.then([that](Concurrency::task<void> initTask) {
		try {
			initTask.get();
			that->m_stateLoaded = true;
			if (that->m_menuPage != nullptr) {
				that->m_menuPage->OnStateLoaded();
			}
		}
		catch (const std::exception& ex) {
			that->m_initStarted = false;
			moonlight_xbox_dx::Utils::Logf("Application state initialization exception: %s\n", ex.what());
			return;
		}
		catch (...) {
			that->m_initStarted = false;
			moonlight_xbox_dx::Utils::Log("Application state initialization unknown exception\n");
			return;
		}
	}, Concurrency::task_continuation_context::get_current_winrt_context());
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
	InitializeState();
	displayRequest->RequestActive();
}

namespace {
	struct ProtocolLaunchRequest {
		bool hasTarget = false;
		std::wstring host;
		int appId = -1;
		std::wstring appName;
		bool resume = false;
		bool hasLaunchOnExit = false;
		Platform::String^ launchOnExit;
	};

	bool IsTruthyParam(Platform::String^ value) {
		if (value == nullptr || value->IsEmpty()) return true;
		return _wcsicmp(value->Data(), L"true") == 0 || _wcsicmp(value->Data(), L"1") == 0 || _wcsicmp(value->Data(), L"yes") == 0;
	}

	ProtocolLaunchRequest ParseProtocolUri(Windows::Foundation::Uri^ uri) {
		ProtocolLaunchRequest request;
		Windows::Foundation::WwwFormUrlDecoder^ query = nullptr;
		try {
			query = uri->QueryParsed;
		} catch (...) {
			moonlight_xbox_dx::Utils::Log("Protocol activation: failed to parse query string\n");
			return request;
		}
		if (query == nullptr) return request;
		for (unsigned int i = 0; i < query->Size; i++) {
			auto entry = query->GetAt(i);
			if (entry == nullptr || entry->Name == nullptr) continue;
			const wchar_t* name = entry->Name->Data();
			Platform::String^ value = entry->Value;
			bool hasValue = value != nullptr && !value->IsEmpty();
			if (_wcsicmp(name, L"host") == 0 && hasValue) {
				request.host = value->Data();
				request.hasTarget = true;
			} else if (_wcsicmp(name, L"appId") == 0 && hasValue) {
				wchar_t* end = nullptr;
				const wchar_t* begin = value->Data();
				errno = 0;
				long parsed = wcstol(begin, &end, 10);
				if (errno != ERANGE && end != begin && *end == L'\0' && parsed > 0 && parsed <= INT_MAX) {
					request.appId = static_cast<int>(parsed);
					request.hasTarget = true;
				} else {
					moonlight_xbox_dx::Utils::Log("Protocol activation: ignoring invalid appId\n");
				}
			} else if (_wcsicmp(name, L"appName") == 0 && hasValue) {
				request.appName = value->Data();
				request.hasTarget = true;
			} else if (_wcsicmp(name, L"desktop") == 0) {
				if (IsTruthyParam(value)) {
					request.appName = L"Desktop";
					request.hasTarget = true;
				}
			} else if (_wcsicmp(name, L"resume") == 0) {
				if (IsTruthyParam(value)) {
					request.resume = true;
					request.hasTarget = true;
				}
			} else if (_wcsicmp(name, L"launchOnExit") == 0 && hasValue) {
				request.launchOnExit = value;
				request.hasLaunchOnExit = true;
			}
		}
		return request;
	}
}

void App::OnActivated(Windows::ApplicationModel::Activation::IActivatedEventArgs^ e)
{
	if (e->Kind != Windows::ApplicationModel::Activation::ActivationKind::Protocol) {
		return;
	}
	auto protocolArgs = dynamic_cast<Windows::ApplicationModel::Activation::ProtocolActivatedEventArgs^>(e);
	if (protocolArgs == nullptr || protocolArgs->Uri == nullptr) {
		return;
	}
	moonlight_xbox_dx::Utils::Logf("Protocol activation: %S\n", protocolArgs->Uri->AbsoluteUri->Data());

	ProtocolLaunchRequest request = ParseProtocolUri(protocolArgs->Uri);

	auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);
	bool isColdStart = (rootFrame == nullptr);
	if (rootFrame == nullptr)
	{
		rootFrame = ref new Frame();
		rootFrame->NavigationFailed += ref new Windows::UI::Xaml::Navigation::NavigationFailedEventHandler(this, &App::OnNavigationFailed);
		Window::Current->Content = rootFrame;
	}

	if (dynamic_cast<StreamPage^>(rootFrame->Content) != nullptr) {
		moonlight_xbox_dx::Utils::Log("Protocol activation ignored: a stream is currently active\n");
		Window::Current->Activate();
		return;
	}

	auto state = GetApplicationState();
	state->pendingProtocolHostSelect = false;
	state->pendingProtocolHost.clear();
	state->pendingProtocolAppId = -1;
	state->pendingProtocolAppName.clear();
	state->pendingProtocolResume = false;
	state->launchOnExitUri = nullptr;

	if (!rootFrame->Navigate(TypeName(HostSelectorPage::typeid))) {
		moonlight_xbox_dx::Utils::Log("Protocol activation: navigation to HostSelectorPage failed\n");
		Window::Current->Activate();
		return;
	}
	rootFrame->BackStack->Clear();
	m_menuPage = dynamic_cast<HostSelectorPage^>(rootFrame->Content);
	if (m_menuPage == nullptr) {
		moonlight_xbox_dx::Utils::Log("Protocol activation: HostSelectorPage instance unavailable\n");
		Window::Current->Activate();
		return;
	}

	state->pendingProtocolHostSelect = request.hasTarget;
	state->pendingProtocolHost = request.host;
	state->pendingProtocolAppId = request.appId;
	state->pendingProtocolAppName = request.appName;
	state->pendingProtocolResume = request.resume;
	state->launchOnExitUri = request.hasTarget && request.hasLaunchOnExit ? request.launchOnExit : nullptr;

	Window::Current->Activate();
	if (isColdStart) {
		displayRequest->RequestActive();
	}

	InitializeState();
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
	//throw ref new FailureException("Failed to load Page " + e->SourcePageType.Name);
}

