//
// DirectXPage.xaml.cpp
// Implementation of the DirectXPage class.
//

#include "pch.h"
#include "StreamPage.xaml.h"
#include <Utils.hpp>
#include <KeyboardControl.xaml.h>

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace concurrency;

StreamPage::StreamPage():
	m_windowVisible(true),
	m_coreInput(nullptr)
{
	InitializeComponent();

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();
	NavigationCacheMode = Windows::UI::Xaml::Navigation::NavigationCacheMode::Enabled;
	swapChainPanel->SizeChanged +=
		ref new SizeChangedEventHandler(this, &StreamPage::OnSwapChainPanelSizeChanged);
	m_deviceResources = std::make_shared<DX::DeviceResources>();
	//Resize to make fullscreen on Xbox

	this->Loaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &StreamPage::OnLoaded);
	this->Unloaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &StreamPage::OnUnloaded);

	m_logsTimer = ref new Windows::UI::Xaml::DispatcherTimer();
	m_logsTimer->Interval = Windows::Foundation::TimeSpan{ 10000000 }; // 1 second
	m_logsTimer->Tick += ref new Windows::Foundation::EventHandler<Object^>(this, &StreamPage::UpdateLogsText);
	
	m_statsTimer = ref new Windows::UI::Xaml::DispatcherTimer();
	m_statsTimer->Interval = Windows::Foundation::TimeSpan{ 10000000 }; // 1 second
	m_statsTimer->Tick += ref new Windows::Foundation::EventHandler<Object^>(this, &StreamPage::UpdateStatsText);
}


StreamPage::~StreamPage()
{
	// Stop rendering and processing events on destruction.
}

void StreamPage::OnBackRequested(Platform::Object^ e,Windows::UI::Core::BackRequestedEventArgs^ args)
{
	// UWP on Xbox One triggers a back request whenever the B
	// button is pressed which can result in the app being
	// suspended if unhandled
	args->Handled = true;
}

void StreamPage::Page_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	try {
		Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);
		keyDownHandler = (Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyDown += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::KeyEventArgs^>(this, &StreamPage::OnKeyDown));
		keyUpHandler = (Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyUp += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::KeyEventArgs^>(this, &StreamPage::OnKeyUp));
		// At this point we have access to the device. 
		// We can create the device-dependent resources.
		m_deviceResources->SetSwapChainPanel(swapChainPanel);
		m_main = std::unique_ptr<moonlight_xbox_dxMain>(new moonlight_xbox_dxMain(m_deviceResources,this,new MoonlightClient(),configuration));
		m_main->CreateDeviceDependentResources();
		m_main->CreateWindowSizeDependentResources();
		m_main->StartRenderLoop();
	}
	catch (const std::exception & ex) {
		Windows::UI::Xaml::Controls::ContentDialog^ dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
		dialog->Content = Utils::StringPrintf(ex.what());
		dialog->CloseButtonText = L"OK";
		dialog->ShowAsync();
	}
	catch (const std::string & string) {
		Windows::UI::Xaml::Controls::ContentDialog^ dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
		dialog->Content = Utils::StringPrintf(string.c_str());
		dialog->CloseButtonText = L"OK";
		dialog->ShowAsync();
	}
	catch (Platform::Exception ^ e) {
		Windows::UI::Xaml::Controls::ContentDialog^ dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
		Platform::String^ errorMsg = ref new Platform::String();
		errorMsg = errorMsg->Concat(L"Exception: ", e->Message);
		errorMsg = errorMsg->Concat(errorMsg,Utils::StringPrintf("%x",e->HResult));
		dialog->Content = errorMsg;
		dialog->CloseButtonText = L"OK";
		dialog->ShowAsync();
	}
	catch (...) {
		std::exception_ptr eptr;
		eptr = std::current_exception(); // capture
		Windows::UI::Xaml::Controls::ContentDialog^ dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
		dialog->Content = L"Generic Exception";
		dialog->CloseButtonText = L"OK";
		dialog->ShowAsync();
	}
}

void StreamPage::OnSwapChainPanelSizeChanged(Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e)
{
	if (m_main == nullptr || m_deviceResources == nullptr)return;
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetLogicalSize(e->NewSize);
	m_main->CreateDeviceDependentResources();
	m_main->CreateWindowSizeDependentResources();
}


void StreamPage::flyoutButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	Windows::UI::Xaml::Controls::Flyout::ShowAttachedFlyout((FrameworkElement^)sender);
	m_main->SetFlyoutOpened(true);
}


void StreamPage::ActionsFlyout_Closed(Platform::Object^ sender, Platform::Object^ e)
{
	if(m_main != nullptr) m_main->SetFlyoutOpened(false);
}


void StreamPage::toggleMouseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	m_main->mouseMode = !m_main->mouseMode;
	this->toggleMouseButton->Text = m_main->mouseMode ? "Exit Mouse Mode" : "Toggle Mouse Mode";
}


void StreamPage::toggleLogsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	Utils::showLogs = !Utils::showLogs;
	this->toggleLogsButton->Text = Utils::showLogs ? "Hide Logs" : "Show Logs";
	m_logsPanel->Visibility = Utils::showLogs ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
	
	if (Utils::showLogs) {
		m_logsTimer->Start();
		UpdateLogsText(sender, e);
	} else {
		m_logsTimer->Stop();
	}
}

void StreamPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	configuration = dynamic_cast<StreamConfiguration^>(e->Parameter);
	SetStreamConfig(configuration);
	if (configuration == nullptr)return;
}

void StreamPage::toggleStatsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	Utils::showStats = !Utils::showStats;
	this->toggleStatsButton->Text = Utils::showStats ? "Hide Stats" : "Show Stats";

	m_statsPanel->Visibility = Utils::showStats ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;

	if (Utils::showStats) {
		m_statsTimer->Start();
		UpdateStatsText(sender, e);
	}
	else {
		m_statsTimer->Stop();
	}
}


void StreamPage::disonnectButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyDown -= keyDownHandler;
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyUp -= keyUpHandler;
	this->m_main->StopRenderLoop();
	this->m_main->Disconnect();
	this->Frame->GoBack();
}

void StreamPage::OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ e)
{
	//Ignore Gamepad input
	if (e->VirtualKey >= Windows::System::VirtualKey::GamepadA && e->VirtualKey <= Windows::System::VirtualKey::GamepadRightThumbstickLeft) {
		return;
	}
	char modifiers = 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Control) == (CoreVirtualKeyStates::Down) ? MODIFIER_CTRL : 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Menu) == (CoreVirtualKeyStates::Down) ? MODIFIER_ALT : 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Shift) == (CoreVirtualKeyStates::Down) ? MODIFIER_SHIFT : 0;
	this->m_main->OnKeyDown((unsigned short)e->VirtualKey,modifiers);
}


void StreamPage::OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ e)
{
	//Ignore Gamepad input
	if (e->VirtualKey >= Windows::System::VirtualKey::GamepadA && e->VirtualKey <= Windows::System::VirtualKey::GamepadRightThumbstickLeft) {
		return;
	}
	char modifiers = 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Control) == (CoreVirtualKeyStates::Down) ? MODIFIER_CTRL : 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Menu) == (CoreVirtualKeyStates::Down) ? MODIFIER_ALT : 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Shift) == (CoreVirtualKeyStates::Down) ? MODIFIER_SHIFT : 0;
	this->m_main->OnKeyUp((unsigned short) e->VirtualKey, modifiers);

}


void StreamPage::disconnectAndCloseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyDown -= keyDownHandler;
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyUp -= keyUpHandler;
	this->m_main->StopRenderLoop();
	this->m_main->Disconnect();
	this->m_main->CloseApp();
	this->Frame->GoBack();
}

void StreamPage::Keyboard_OnKeyDown(KeyboardControl^ sender, KeyEvent^ e)
{
	this->m_main->OnKeyDown(e->VirtualKey, e->Modifiers);
}


void StreamPage::Keyboard_OnKeyUp(KeyboardControl^ sender, KeyEvent^ e)
{
	this->m_main->OnKeyUp(e->VirtualKey, e->Modifiers);
}


void StreamPage::guideButtonShort_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->m_main->SendGuideButton(500);
}


void StreamPage::guideButtonLong_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->m_main->SendGuideButton(3000);
}


void moonlight_xbox_dx::StreamPage::toggleHDR_WinAltB_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->m_main->SendWinAltB();
}

void StreamPage::OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	m_back_cookie = navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &StreamPage::OnBackRequested);
}


void StreamPage::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_cookie;
}

void StreamPage::UpdateLogsText(Platform::Object^ sender, Platform::Object^ args)
{
    Utils::logMutex.lock();
    std::vector<std::wstring> lines = Utils::GetLogLines();
    std::wstring combinedText;
    for (const auto& line : lines) {
        combinedText += line;
    }
    Utils::logMutex.unlock();

    m_logsTextBlock->Text = ref new Platform::String(combinedText.c_str());
}

void StreamPage::UpdateStatsText(Platform::Object^ sender, Platform::Object^ args)
{
	if (m_main != nullptr && m_main->GetStats() != nullptr) {
		char statsOutput[1024];
		DX::StepTimer timer = m_main->GetTimer();

		if (m_main->GetStats()->ShouldUpdateDisplay(timer, Utils::showStats, statsOutput, sizeof(statsOutput))) {
			// Remove trailing newline if it exists
			size_t len = strlen(statsOutput);
			if (len > 0 && statsOutput[len - 1] == '\n') {
				statsOutput[len - 1] = '\0';
			}

			// Convert from char* to Platform::String^
			size_t size = strlen(statsOutput) + 1;
			wchar_t* wideText = new wchar_t[size];
			size_t convertedChars = 0;
			mbstowcs_s(&convertedChars, wideText, size, statsOutput, _TRUNCATE);
			Platform::String^ statsText = ref new Platform::String(wideText);
			delete[] wideText;

			m_statsTextBlock->Text = statsText;
		}
	}

}

