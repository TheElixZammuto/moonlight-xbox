
#include "pch.h"
#include "StreamPage.xaml.h"
#include "../Streaming/FFMpegDecoder.h"
#define MLOG_TAG_OVERRIDE "StreamPage"
#include <Utils.hpp>
#include <KeyboardControl.xaml.h>

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Gaming::Input;
using namespace Windows::Graphics::Display;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::ViewManagement::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml::Media::Imaging;
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

	m_subMenuCloseTimer = ref new Windows::UI::Xaml::DispatcherTimer();
	m_subMenuCloseTimer->Interval = Windows::Foundation::TimeSpan{ 1000000LL };
	m_subMenuCloseTimer->Tick += ref new Windows::Foundation::EventHandler<Platform::Object^>(this, &StreamPage::OnSubMenuCloseTimer_Tick);
}

void StreamPage::OnBackRequested(Platform::Object^ e,Windows::UI::Core::BackRequestedEventArgs^ args)
{

	args->Handled = true;
}

void StreamPage::Page_Loaded(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {

	this->m_progressView->Visibility = Windows::UI::Xaml::Visibility::Visible;

	m_streamMenuVisible = false;
	this->MenuShowStoryboard->Stop();
	this->MenuHideStoryboard->Stop();
	this->StreamMenuGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	InstantHideSubMenu();

	if (m_backgroundImage != nullptr) {
		StartBgPanAnimation();
		FadeInBackground();
	}

	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	m_back_cookie = navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs ^>(this, &StreamPage::OnBackRequested);

	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);

	keyDownHandler = (Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyDown += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::KeyEventArgs ^>(this, &StreamPage::OnKeyDown));
	keyUpHandler = (Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyUp += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::KeyEventArgs ^>(this, &StreamPage::OnKeyUp));

	gamepadAddedHandler = Gamepad::GamepadAdded += ref new EventHandler<Gamepad^>(this, &StreamPage::OnGamepadAdded);
	gamepadRemovedHandler = Gamepad::GamepadRemoved += ref new EventHandler<Gamepad ^>(this, &StreamPage::OnGamepadRemoved);

	try {
		m_deviceResources->SetSwapChainPanel(swapChainPanel);
	} catch (...) {
		MLOG(Utils::LogLevel::Error, "Page_Loaded: SetSwapChainPanel failed\n");
	}

	Platform::WeakReference weakThis(this);
	DISPATCH_UI([weakThis] {
		auto that = weakThis.Resolve<StreamPage>();
		if (that == nullptr) return;
		try {
			that->m_main = std::unique_ptr<moonlight_xbox_dxMain>(new moonlight_xbox_dxMain(that->m_deviceResources, that, new MoonlightClient(), that->configuration));
			that->m_main->CreateDeviceDependentResources();
			that->m_main->CreateWindowSizeDependentResources();
			that->m_main->StartRenderLoop();
        } catch (const std::exception &ex) {
			MLOGF(Utils::LogLevel::Error, "Page_Loaded: Exception when starting stream. Exception: %s", ex.what());
        } catch (const std::string &string) {
			MLOGF(Utils::LogLevel::Error, "Page_Loaded: Exception when starting stream. Exception: %s", string);
        } catch (Platform::Exception ^ e) {
            Platform::String ^ errorMsg = ref new Platform::String();
            errorMsg = errorMsg->Concat(L"Exception: ", e->Message);
            errorMsg = errorMsg->Concat(errorMsg, Utils::StringPrintf("%x", e->HResult));
			MLOGF(Utils::LogLevel::Error, "Page_Loaded: Exception when starting stream. Exception: %s", Utils::PlatformStringToStdString(errorMsg));
        } catch (...) {
            MLOG(Utils::LogLevel::Error, "Page_Loaded: Exception when starting stream. Exception: Generic Exception");
        }
	});
}

void StreamPage::Page_Unloaded(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	try { if (m_bgPanStoryboard != nullptr) { m_bgPanStoryboard->Stop(); m_bgPanStoryboard = nullptr; } } catch(...) {}

	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_cookie;

	Gamepad::GamepadAdded -= gamepadAddedHandler;
	Gamepad::GamepadRemoved -= gamepadRemovedHandler;

	if (this->m_main) {

		MLOG(Utils::LogLevel::Info, "Page_Unloaded stopping m_main render loop\n");

		try {
			this->m_main->StopRenderLoop();
			this->m_main.reset();
		} catch (std::exception &ex) {
			MLOGF(Utils::LogLevel::Error, "Page_Unloaded m_main threw an exception: %s\n", ex.what());
		} catch (...) {
			MLOG(Utils::LogLevel::Error, "Page_Unloaded m_main threw an exception\n");
		}

		MLOG(Utils::LogLevel::Info, "Page_Unloaded m_main reset\n");
	}

	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyDown -= keyDownHandler;
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyUp -= keyUpHandler;
}

StreamPage::~StreamPage()
{
}

void StreamPage::OnSwapChainPanelSizeChanged(Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e)
{
	if (m_main == nullptr || m_deviceResources == nullptr)return;
	MLOGF(Utils::LogLevel::Debug, "OnSwapChainPanelSizeChanged( NewSize: %f x %f )\n", e->NewSize.Width, e->NewSize.Height);
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetLogicalSize(e->NewSize);
	m_main->CreateDeviceDependentResources();
	m_main->CreateWindowSizeDependentResources();
}

void StreamPage::SetStreamMenuVisible(bool visible) {
	m_streamMenuVisible = visible;
	if (visible) {
		this->MenuHideStoryboard->Stop();
		this->StreamMenuGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
		this->MenuShowStoryboard->Begin();
		this->FirstMenuButton->Focus(Windows::UI::Xaml::FocusState::Programmatic);
	} else {
		InstantHideSubMenu();
		this->MenuShowStoryboard->Stop();
		this->MenuHideStoryboard->Begin();

	}
	if (m_main) m_main->SetMenuVisible(visible);
}

void StreamPage::MenuHideStoryboard_Completed(Platform::Object^ sender, Platform::Object^ e) {
	this->StreamMenuGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
}

void StreamPage::ShowSubMenu() {
	if (m_subMenuVisible) return;
	m_subMenuVisible = true;
	this->SubMenuHideStoryboard->Stop();
	this->OtherSubMenuPanel->Visibility = Windows::UI::Xaml::Visibility::Visible;
	this->SubMenuShowStoryboard->Begin();
}

void StreamPage::HideSubMenu() {
	if (!m_subMenuVisible) return;
	m_subMenuVisible = false;
	this->SubMenuShowStoryboard->Stop();
	this->SubMenuHideStoryboard->Begin();
}

void StreamPage::InstantHideSubMenu() {
	if (m_subMenuCloseTimer) m_subMenuCloseTimer->Stop();
	m_subMenuVisible = false;
	this->SubMenuShowStoryboard->Stop();
	this->SubMenuHideStoryboard->Stop();
	this->OtherSubMenuPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
}

void StreamPage::SubMenuHideStoryboard_Completed(Platform::Object^ sender, Platform::Object^ e) {
	this->OtherSubMenuPanel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
}

void StreamPage::OtherButton_GotFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
	m_subMenuCloseTimer->Stop();
	ShowSubMenu();
}

void StreamPage::OtherButton_LostFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
	m_subMenuCloseTimer->Start();
}

void StreamPage::SubMenuButton_GotFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
	m_subMenuCloseTimer->Stop();
}

void StreamPage::SubMenuButton_LostFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
	m_subMenuCloseTimer->Start();
}

void StreamPage::OnSubMenuCloseTimer_Tick(Platform::Object^ sender, Platform::Object^ e) {
	m_subMenuCloseTimer->Stop();
	HideSubMenu();
}

void StreamPage::toggleMouseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	SetMouseMode(!this->m_mouseMode);
}

void StreamPage::SetMouseMode(bool enabled)
{
    this->MouseMode = enabled;
    if (m_main) m_main->mouseMode = this->MouseMode;

    if (this->MouseModeText != nullptr) {
        this->MouseModeText->Text = this->MouseMode ? ref new Platform::String(L"Mouse On") : ref new Platform::String(L"Mouse Off");
    }

    if (enabled) {
        this->StreamMenuGrid->Visibility = Windows::UI::Xaml::Visibility::Visible;
        this->FirstMenuButton->Focus(Windows::UI::Xaml::FocusState::Programmatic);
        this->StreamMenuGrid->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    }
}

void StreamPage::showKeyboardButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	if (!m_main) return;
	SetStreamMenuVisible(false);
	if (GetApplicationState()->EnableKeyboard) {
		m_main->keyboardMode = true;

		this->Dispatcher->RunAsync(
			Windows::UI::Core::CoreDispatcherPriority::Normal,
			ref new Windows::UI::Core::DispatchedHandler([this]() {
				this->m_keyboardView->Visibility = Windows::UI::Xaml::Visibility::Visible;
			}));
	} else {
		CoreInputView::GetForCurrentView()->TryShow(CoreInputViewKind::Keyboard);
	}
}

void StreamPage::toggleLogsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	bool isVisible = m_main->ToggleLogs();
	SetShowLogs(isVisible);
}

void StreamPage::SetShowLogs(bool enabled) {
    this->ShowLogs = enabled;
    if (this->ShowLogsText != nullptr) {
        this->ShowLogsText->Text = this->ShowLogs ? ref new Platform::String(L"Logs On") : ref new Platform::String(L"Logs Off");
    }
}

void StreamPage::toggleStatsButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	bool isVisible = m_main->ToggleStats();
	SetShowStats(isVisible);
}

void StreamPage::SetShowStats(bool enabled) {
    this->ShowStats = enabled;
    if (this->ShowStatsText != nullptr) {
        this->ShowStatsText->Text = this->ShowStats ? ref new Platform::String(L"Stats On") : ref new Platform::String(L"Stats Off");
    }
}

void StreamPage::StartBgPanAnimation() {
	try {
		if (PageBackgroundImage == nullptr) return;

		auto transform = ref new TranslateTransform();
		PageBackgroundImage->RenderTransform = transform;

		auto sb = ref new Storyboard();
		sb->RepeatBehavior = RepeatBehaviorHelper::Forever;
		sb->AutoReverse = true;

		Windows::UI::Xaml::Duration dur(TimeSpan{ 10LL * 10000000LL });

		auto ease = ref new SineEase();
		ease->EasingMode = EasingMode::EaseInOut;

		auto animY = ref new DoubleAnimation();
		animY->To = ref new Platform::Box<double>(120.0);
		animY->Duration = dur;
		animY->EasingFunction = ease;
		Storyboard::SetTarget(animY, transform);
		Storyboard::SetTargetProperty(animY, ref new Platform::String(L"Y"));
		sb->Children->Append(animY);

		sb->Begin();
		m_bgPanStoryboard = sb;
	} catch(...) {}
}

void StreamPage::FadeInBackground() {
	try {
		if (PageBackgroundImage == nullptr || m_backgroundImage == nullptr) return;

		auto brush = dynamic_cast<ImageBrush^>(PageBackgroundImage->Background);
		if (brush == nullptr) return;
		brush->ImageSource = m_backgroundImage;

		auto anim = ref new DoubleAnimation();
		anim->From = 0.0;
		anim->To = 0.2;
		anim->Duration = Windows::UI::Xaml::Duration(TimeSpan{ 2500000LL });

		auto sb = ref new Storyboard();
		sb->Children->Append(anim);
		Storyboard::SetTarget(anim, PageBackgroundImage);
		Storyboard::SetTargetProperty(anim, ref new Platform::String(L"Opacity"));
		sb->Begin();
	} catch(...) {}
}

void StreamPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {

	configuration = dynamic_cast<StreamConfiguration^>(e->Parameter);
	SetStreamConfig(configuration);

	if (configuration == nullptr)return;

	m_backgroundImage = configuration->backgroundImage;

	SetMouseMode(false);
	SetShowLogs(false);
	SetShowStats(configuration->enableStats);

}

void StreamPage::disonnectButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyDown -= keyDownHandler;
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyUp -= keyUpHandler;

	this->m_main->moonlightClient->SetConnectionTerminated();
}

void StreamPage::OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ e)
{

	if (e->VirtualKey >= Windows::System::VirtualKey::GamepadA && e->VirtualKey <= Windows::System::VirtualKey::GamepadRightThumbstickLeft) {
		return;
	}
	if (!m_main) return;
	char modifiers = 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Control) == (CoreVirtualKeyStates::Down) ? MODIFIER_CTRL : 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Menu) == (CoreVirtualKeyStates::Down) ? MODIFIER_ALT : 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Shift) == (CoreVirtualKeyStates::Down) ? MODIFIER_SHIFT : 0;
	this->m_main->OnKeyDown((unsigned short)e->VirtualKey,modifiers);
}

void StreamPage::OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ e)
{

	if (e->VirtualKey >= Windows::System::VirtualKey::GamepadA && e->VirtualKey <= Windows::System::VirtualKey::GamepadRightThumbstickLeft) {
		return;
	}
	char modifiers = 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Control) == (CoreVirtualKeyStates::Down) ? MODIFIER_CTRL : 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Menu) == (CoreVirtualKeyStates::Down) ? MODIFIER_ALT : 0;
	modifiers |= CoreWindow::GetForCurrentThread()->GetKeyState(Windows::System::VirtualKey::Shift) == (CoreVirtualKeyStates::Down) ? MODIFIER_SHIFT : 0;
	this->m_main->OnKeyUp((unsigned short) e->VirtualKey, modifiers);
}

void StreamPage::disconnectAndCloseButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyDown -= keyDownHandler;
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyUp -= keyUpHandler;
	if (this->m_main) {

		this->m_main->moonlightClient->SetConnectionTerminated();
	}

	auto that = this;

	auto name = (configuration != nullptr && configuration->appName != nullptr) ? configuration->appName : nullptr;
	this->ClosingOverlayText->Text = (name != nullptr && name->Length() > 0)
		? ref new Platform::String((std::wstring(L"Closing ") + name->Data() + L"...").c_str())
		: ref new Platform::String(L"Closing...");
	this->ClosingOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;

	concurrency::create_task(concurrency::create_async([that]() {
		try {
			if (that->m_main) {
				that->m_main->CloseApp();
			}
		} catch (...) {
		}
	})).then([that](concurrency::task<void> t) {
		try {
			t.get();

			DISPATCH_UI([that] {
				that->ClosingOverlay->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
			});
		} catch (...) {
		}
	});
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
	SetStreamMenuVisible(false);
	this->m_main->SendGuideButton(500);
}

void StreamPage::guideButtonLong_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	SetStreamMenuVisible(false);
	this->m_main->SendGuideButton(3000);
}

void StreamPage::toggleHDR_WinAltB_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->m_main->SendWinAltB();
}

void StreamPage::resetDecoder_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	LiRequestIdrFrame();
}

void StreamPage::toggleFramePacing_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

	bool isImmediate = Pacer::instance().getPacingImmediate();
	Pacer::instance().setPacingImmediate(isImmediate ? false : true);
}

void StreamPage::OnPropertyChanged(Platform::String^ propertyName)
{

}

void StreamPage::OnGamepadAdded(Platform::Object^ sender, Gamepad^ gamepad)
{
	m_refreshGamepads.store(true, std::memory_order_release);
}

void StreamPage::OnGamepadRemoved(Platform::Object^ sender, Gamepad^ gamepad)
{
	m_refreshGamepads.store(true, std::memory_order_release);
}

bool StreamPage::ShouldRefreshGamepads() {
	return m_refreshGamepads.exchange(false);
}

void StreamPage::RequestRefreshGamepads() {
	m_refreshGamepads.store(true, std::memory_order_release);
}
