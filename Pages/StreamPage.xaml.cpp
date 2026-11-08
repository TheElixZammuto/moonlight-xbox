//
// DirectXPage.xaml.cpp
// Implementation of the DirectXPage class.
//

#include "pch.h"
#include "StreamPage.xaml.h"
#include "../Plot/ImGuiPlots.h"
#include "../Streaming/AudioPlayer.h"
#include "../Streaming/FrameQueue.h"
#include "../Streaming/FFMpegDecoder.h"
#include <Utils.hpp>

extern "C" {
#include <Limelight.h>
}

#include <KeyboardControl.xaml.h>
#include "../Common/ModalDialog.xaml.h"
#include "../Utils/TracyBootstrap.h"

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Gaming::Input;
using namespace Windows::Graphics::Display;
using namespace Windows::Graphics::Display::Core;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::ViewManagement;
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

	// Init Tracy, all Tracy functions are noops if not enabled.
	TracyBootstrap::Initialize();

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();
	NavigationCacheMode = Windows::UI::Xaml::Navigation::NavigationCacheMode::Enabled;
	swapChainPanel->SizeChanged +=
		ref new SizeChangedEventHandler(this, &StreamPage::OnSwapChainPanelSizeChanged);
	m_deviceResources = std::make_shared<DX::DeviceResources>();
}

StreamPage::~StreamPage()
{
}

void StreamPage::OnBackRequested(Platform::Object^ e,Windows::UI::Core::BackRequestedEventArgs^ args)
{
	// UWP on Xbox One triggers a back request whenever the B
	// button is pressed which can result in the app being
	// suspended if unhandled. On the phone the hardware Back key
	// lands here too: while streaming, back means disconnect and
	// return to the host list.
	args->Handled = true;
	disconnectAndCloseButton_Click(nullptr, nullptr);
}

void StreamPage::Page_Loaded(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {

	this->m_progressView->Visibility = Windows::UI::Xaml::Visibility::Visible;
	this->m_progressRing->IsActive = true;

	if (Utils::ShowDevTools()) {
		toggleCapture->Visibility = Windows::UI::Xaml::Visibility::Visible;
	}

	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	m_back_cookie = navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs ^>(this, &StreamPage::OnBackRequested);

	if (Utils::IsWindowsPhone()) {
		// Fullscreen hides BOTH the status bar and the virtual navigation bar;
		// StatusBar::HideAsync alone leaves the nav bar eating 92px of the
		// panel width and the swap chain shows a black strip beside it.
		try {
			if (!Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->TryEnterFullScreenMode()) {
				Utils::Log("EnterFullScreenMode returned false\n");
			}
		} catch (...) {
			Utils::Log("EnterFullScreenMode failed\n");
		}
		// Minimal: a single edge swipe only shows a small edge affordance;
		// the nav bar no longer pops up on every touch (bounds were
		// flapping 592x360 <-> 640x360 while touching, letterboxing the
		// video into a bottom strip each time).
		try {
			Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->FullScreenSystemOverlayMode =
				Windows::UI::ViewManagement::FullScreenSystemOverlayMode::Minimal;
		} catch (...) {
			Utils::Log("FullScreenSystemOverlayMode failed\n");
		}
		// Draw UNDER the system bars: the panel spans the whole physical
		// screen (640x360) at all times. When the nav bar pops up it
		// becomes a translucent overlay on top of the video instead of
		// carving out a black strip + letterbox. Also removes the
		// swapchain resize stall that came with every bar pop.
		try {
			Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(
				Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);
		} catch (...) {
			Utils::Log("SetDesiredBoundsMode failed\n");
		}
		// Fullscreen can succeed while the XAML layout still reserves the
		// system bar area (panel stuck at 548x360 until a rotation cycle);
		// force a re-measure whenever the visible bounds change.
		m_visibleBoundsChangedCookie = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->VisibleBoundsChanged +=
			ref new Windows::Foundation::TypedEventHandler<Windows::UI::ViewManagement::ApplicationView^, Platform::Object^>(
				this, &StreamPage::OnVisibleBoundsChanged);
		// Physical mice pass through in relative mode: raw deltas go to the
		// host, which renders its own cursor; hide the local pointer arrow.
		try {
			m_mouseDevice = Windows::Devices::Input::MouseDevice::GetForCurrentView();
			m_mouseMovedCookie = m_mouseDevice->MouseMoved += ref new Windows::Foundation::TypedEventHandler<
				Windows::Devices::Input::MouseDevice^, Windows::Devices::Input::MouseEventArgs^>(
				this, &StreamPage::OnMouseMovedRelative);
		} catch (...) {
			Utils::Log("MouseDevice relative capture failed\n");
		}
		try {
			Windows::UI::Core::CoreWindow::GetForCurrentThread()->PointerCursor = nullptr;
		} catch (...) {
			Utils::Log("PointerCursor hide failed\n");
		}
		// Touch on the panel acts as a trackpad for the host cursor.
		m_pointerPressedCookie = swapChainPanel->PointerPressed += ref new Windows::UI::Xaml::Input::PointerEventHandler(this, &StreamPage::OnPointerPressed);
		m_pointerMovedCookie = swapChainPanel->PointerMoved += ref new Windows::UI::Xaml::Input::PointerEventHandler(this, &StreamPage::OnPointerMoved);
		m_pointerReleasedCookie = swapChainPanel->PointerReleased += ref new Windows::UI::Xaml::Input::PointerEventHandler(this, &StreamPage::OnPointerReleased);
	}

	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);

	keyDownHandler = (Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyDown += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::KeyEventArgs ^>(this, &StreamPage::OnKeyDown));
	keyUpHandler = (Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyUp += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow ^, Windows::UI::Core::KeyEventArgs ^>(this, &StreamPage::OnKeyUp));

	// Detect gamepad connection and disconnection events
	gamepadAddedHandler = Gamepad::GamepadAdded += ref new EventHandler<Gamepad^>(this, &StreamPage::OnGamepadAdded);
	gamepadRemovedHandler = Gamepad::GamepadRemoved += ref new EventHandler<Gamepad ^>(this, &StreamPage::OnGamepadRemoved);

	if (IsXbox() && m_hdmiInfo == nullptr) {
		auto hdmi = HdmiDisplayInformation::GetForCurrentView();
		if (hdmi != nullptr) {
			// Watch for HDMI display changes
			m_hdmiHandler = hdmi->DisplayModesChanged +=
				ref new TypedEventHandler<HdmiDisplayInformation^, Platform::Object^>(this, &StreamPage::OnHdmiDisplayModesChanged);

			m_hdmiInfo = hdmi;
			Utils::Log("HDMI display listener attached\n");
		}
	}

	try {
		m_deviceResources->SetSwapChainPanel(swapChainPanel);
	} catch (...) {
		Utils::Log("StreamPage::Page_Loaded: SetSwapChainPanel failed\n");
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
			Utils::Logf("StreamPage::Page_Loaded: Exception when starting stream. Exception: %s", ex.what());
        } catch (const std::string &string) {
			Utils::Logf("StreamPage::Page_Loaded: Exception when starting stream. Exception: %s", string);
        } catch (Platform::Exception ^ e) {
            Platform::String ^ errorMsg = ref new Platform::String();
            errorMsg = errorMsg->Concat(L"Exception: ", e->Message);
            errorMsg = errorMsg->Concat(errorMsg, Utils::StringPrintf("%x", e->HResult));
			Utils::Logf("StreamPage::Page_Loaded: Exception when starting stream. Exception: %s", Utils::PlatformStringToStdString(errorMsg));
        } catch (...) {
            Utils::Log("StreamPage::Page_Loaded: Exception when starting stream. Exception: Generic Exception");
        }
	});
}

void StreamPage::Page_Unloaded(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_cookie;

	if (Utils::IsWindowsPhone()) {
		try {
			Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->ExitFullScreenMode();
		} catch (...) {
			Utils::Log("ExitFullScreenMode failed\n");
		}
		swapChainPanel->PointerPressed -= m_pointerPressedCookie;
		swapChainPanel->PointerMoved -= m_pointerMovedCookie;
		swapChainPanel->PointerReleased -= m_pointerReleasedCookie;
		Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->VisibleBoundsChanged -= m_visibleBoundsChangedCookie;
		if (m_mouseDevice != nullptr) {
			m_mouseDevice->MouseMoved -= m_mouseMovedCookie;
			m_mouseDevice = nullptr;
		}
		try {
			Windows::UI::Core::CoreWindow::GetForCurrentThread()->PointerCursor =
				ref new Windows::UI::Core::CoreCursor(Windows::UI::Core::CoreCursorType::Arrow, 0);
		} catch (...) {}
	}

	Gamepad::GamepadAdded -= gamepadAddedHandler;
	Gamepad::GamepadRemoved -= gamepadRemovedHandler;

	if (m_hdmiInfo != nullptr) {
		m_hdmiInfo->DisplayModesChanged -= m_hdmiHandler;
		m_hdmiInfo = nullptr;
	}

	if (this->m_main) {

		Utils::Log("StreamPage::Page_Unloaded stopping m_main render loop\n");

		try {
			this->m_main->StopRenderLoop();
			this->m_main.reset();
		} catch (std::exception &ex) {
			Utils::Logf("StreamPage::Page_Unloaded m_main threw an exception: %s\n", ex.what());
		} catch (...) {
			Utils::Log("StreamPage::Page_Unloaded m_main threw an exception\n");
		}

		Utils::Log("StreamPage::Page_Unloaded m_main reset\n");
	}

	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyDown -= keyDownHandler;
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyUp -= keyUpHandler;
}

void StreamPage::OnSwapChainPanelSizeChanged(Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e)
{
	if (m_main == nullptr || m_deviceResources == nullptr)return;
	Utils::Logf("StreamPage::OnSwapChainPanelSizeChanged( NewSize: %f x %f )\n", e->NewSize.Width, e->NewSize.Height);
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetLogicalSize(e->NewSize);
	m_main->CreateDeviceDependentResources();
	m_main->CreateWindowSizeDependentResources();
}


void StreamPage::flyoutButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	// Capture stops itself when it hits the size limit, so resync the menu text.
	this->CaptureMode = FFMpegDecoder::instance().IsCaptureActive();

	Windows::UI::Xaml::Controls::Flyout::ShowAttachedFlyout((FrameworkElement^)sender);
	m_main->SetFlyoutOpened(true);
}


void StreamPage::ActionsFlyout_Closed(Platform::Object^ sender, Platform::Object^ e)
{
	if (m_main != nullptr) m_main->SetFlyoutOpened(false);
}

void StreamPage::toggleMouseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	SetMouseMode(!this->m_mouseMode);
}

void StreamPage::SetMouseMode(bool enabled)
{
	this->MouseMode = enabled;
	if (m_main) m_main->mouseMode = this->MouseMode;
}

void StreamPage::showKeyboardButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	if (!m_main) return;
	if (GetApplicationState()->EnableKeyboard) {
		m_main->keyboardMode = true;

		this->Dispatcher->RunAsync(
			Windows::UI::Core::CoreDispatcherPriority::Normal,
			ref new Windows::UI::Core::DispatchedHandler([this]() {
				this->m_keyboardView->Visibility = Windows::UI::Xaml::Visibility::Visible;
			}));
	} else {
		Windows::UI::ViewManagement::InputPane::GetForCurrentView()->TryShow();
	}
}

void StreamPage::toggleLogsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	bool isVisible = m_main->ToggleLogs();
	SetShowLogs(isVisible);
}

void StreamPage::SetShowLogs(bool enabled) {
	this->ShowLogs = enabled;
}

void StreamPage::toggleStatsButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	bool isVisible = m_main->ToggleStats();
	SetShowStats(isVisible);
}

void StreamPage::SetShowStats(bool enabled) {
	this->ShowStats = enabled;
}

void StreamPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	// MUST run before the page's first layout pass. Page_Loaded is too
	// late: the swapchain is created with the stale expanded-nav-bar
	// size (548x360) and the post-load relayout never fires, leaving a
	// 92px black strip on the right. With UseCoreWindow set here the
	// very first layout already uses the full 640x360, so the
	// swapchain is born full-bleed and no transition can get stuck.
	try {
		Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(
			Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);
	} catch (...) {
		Utils::Log("OnNavigatedTo SetDesiredBoundsMode failed\n");
	}

	configuration = dynamic_cast<StreamConfiguration^>(e->Parameter);
	SetStreamConfig(configuration);

	if (configuration == nullptr)return;

	SetCaptureMode(false);
	SetMouseMode(false);
	SetShowLogs(false);
	SetShowStats(configuration->enableStats);

}

void StreamPage::disonnectButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyDown -= keyDownHandler;
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyUp -= keyUpHandler;

	// trigger the server disconnected flow
	this->m_main->moonlightClient->SetConnectionTerminated();
}

void StreamPage::OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ e)
{
	//Ignore Gamepad input
	if (e->VirtualKey >= Windows::System::VirtualKey::GamepadA && e->VirtualKey <= Windows::System::VirtualKey::GamepadRightThumbstickLeft) {
		return;
	}
	auto window = CoreWindow::GetForCurrentThread();
	char modifiers = 0;
	modifiers |= window->GetKeyState(Windows::System::VirtualKey::Control) == (CoreVirtualKeyStates::Down) ? MODIFIER_CTRL : 0;
	modifiers |= window->GetKeyState(Windows::System::VirtualKey::Menu) == (CoreVirtualKeyStates::Down) ? MODIFIER_ALT : 0;
	modifiers |= window->GetKeyState(Windows::System::VirtualKey::Shift) == (CoreVirtualKeyStates::Down) ? MODIFIER_SHIFT : 0;
	if (window->GetKeyState(Windows::System::VirtualKey::LeftWindows) == CoreVirtualKeyStates::Down ||
		window->GetKeyState(Windows::System::VirtualKey::RightWindows) == CoreVirtualKeyStates::Down) {
		Utils::Log("Windows key down\n");
		modifiers |= MODIFIER_META;
	}
	this->m_main->OnKeyDown((unsigned short)e->VirtualKey, modifiers);
}


void StreamPage::OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ e)
{
	//Ignore Gamepad input
	if (e->VirtualKey >= Windows::System::VirtualKey::GamepadA && e->VirtualKey <= Windows::System::VirtualKey::GamepadRightThumbstickLeft) {
		return;
	}
	auto window = CoreWindow::GetForCurrentThread();
	char modifiers = 0;
	modifiers |= window->GetKeyState(Windows::System::VirtualKey::Control) == (CoreVirtualKeyStates::Down) ? MODIFIER_CTRL : 0;
	modifiers |= window->GetKeyState(Windows::System::VirtualKey::Menu) == (CoreVirtualKeyStates::Down) ? MODIFIER_ALT : 0;
	modifiers |= window->GetKeyState(Windows::System::VirtualKey::Shift) == (CoreVirtualKeyStates::Down) ? MODIFIER_SHIFT : 0;
	if (window->GetKeyState(Windows::System::VirtualKey::LeftWindows) == CoreVirtualKeyStates::Down ||
		window->GetKeyState(Windows::System::VirtualKey::RightWindows) == CoreVirtualKeyStates::Down) {
		Utils::Log("Windows key up\n");
		modifiers |= MODIFIER_META;
	}
	this->m_main->OnKeyUp((unsigned short)e->VirtualKey, modifiers);
}

	void StreamPage::OnPointerPressed(Platform::Object ^, Windows::UI::Xaml::Input::PointerRoutedEventArgs ^ e) {
		auto pt = e->GetCurrentPoint(swapChainPanel);
		auto pos = pt->Position;
		if (e->Pointer->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Mouse) {
			// Physical mouse: passthrough mode, movement comes from MouseDevice
			// deltas; only the buttons are handled here.
			int button = pt->Properties->IsRightButtonPressed ? BUTTON_RIGHT : BUTTON_LEFT;
			LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, button);
			m_activePointerButton = button;
		}
		else {
			// Touch: trackpad mode. Pressing a finger does NOT move the cursor;
			// only the slide delta does. A short tap clicks at the cursor.
			m_touchPointerId = (int)e->Pointer->PointerId;
			m_touchLastPos = pos;
			m_touchDownPos = pos;
			m_touchDownMs = GetTickCount64();
			m_touchIsDown = true;
		}
		e->Handled = true;
	}

	void StreamPage::OnPointerMoved(Platform::Object ^, Windows::UI::Xaml::Input::PointerRoutedEventArgs ^ e) {
		auto pt = e->GetCurrentPoint(swapChainPanel);
		auto pos = pt->Position;
		// Physical mice are handled by the relative MouseDevice handler; only
		// touch moves the host cursor here.
		if (m_touchIsDown && (int)e->Pointer->PointerId == m_touchPointerId) {
			// Trackpad mode: accumulate the finger delta onto the cursor.
			double dx = pos.X - m_touchLastPos.X;
			double dy = pos.Y - m_touchLastPos.Y;
			m_touchLastPos = pos;
			m_cursorX = std::max(0, std::min(1279, m_cursorX + (int)dx));
			m_cursorY = std::max(0, std::min(719, m_cursorY + (int)dy));
			LiSendMousePositionEvent((short)m_cursorX, (short)m_cursorY, 1280, 720);
		}
		e->Handled = true;
	}

	void StreamPage::OnPointerReleased(Platform::Object ^, Windows::UI::Xaml::Input::PointerRoutedEventArgs ^ e) {
		auto pt = e->GetCurrentPoint(swapChainPanel);
		if (e->Pointer->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Mouse) {
			LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE,
				m_activePointerButton ? m_activePointerButton : BUTTON_LEFT);
			m_activePointerButton = 0;
		}
		else if (m_touchIsDown && (int)e->Pointer->PointerId == m_touchPointerId) {
			auto pos = pt->Position;
			double dx = pos.X - m_touchDownPos.X;
			double dy = pos.Y - m_touchDownPos.Y;
			uint64_t heldMs = GetTickCount64() - m_touchDownMs;
			// A short tap with <10px of travel sends a click at the cursor's
			// current position; anything else was pure cursor movement.
			if (dx * dx + dy * dy < 100.0 && heldMs < 500) {
				LiSendMousePositionEvent((short)m_cursorX, (short)m_cursorY, 1280, 720);
				LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
				LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
			}
			m_touchIsDown = false;
			m_touchPointerId = -1;
		}
		e->Handled = true;
	}

	void StreamPage::OnVisibleBoundsChanged(Windows::UI::ViewManagement::ApplicationView^, Platform::Object^) {
		auto b = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->VisibleBounds;
		// X/Y offset tells WHICH system bar is covering the screen
		// (Y>0 = status bar at top, X>0/W deficit = nav bar at a side).
		Utils::Logf("VisibleBoundsChanged: x=%.0f y=%.0f %.0fx%.0f\n", b.X, b.Y, b.Width, b.Height);
		// No forced relayout anymore. Rotation and bar transitions re-layout
		// through the normal XAML pipeline (panel SizeChanged -> swapchain
		// recreate). Worse, a forced UpdateLayout fired mid-bar-animation
		// could snap the panel to a transient geometry and freeze it there
		// (suspected source of the bottom-only black strip). Log only.
	}

	void StreamPage::OnMouseMovedRelative(Windows::Devices::Input::MouseDevice^, Windows::Devices::Input::MouseEventArgs^ args) {
		// Raw relative deltas straight to the host; the host renders its own
		// cursor (passthrough, like moonlight-android capture mode).
		LiSendMouseMoveEvent((short)args->MouseDelta.X, (short)args->MouseDelta.Y);
	}

void StreamPage::disconnectAndCloseButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyDown -= keyDownHandler;
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->KeyUp -= keyUpHandler;
	if (this->m_main) {
		// trigger the server disconnected flow which will cleanly exit the loop and call StopRenderLoop()
		this->m_main->moonlightClient->SetConnectionTerminated();
	}

	auto that = this;

	auto progressToken = ::moonlight_xbox_dx::ModalDialog::ShowProgressDialogToken(nullptr, Utils::StringFromStdString("Closing..."));

	concurrency::create_task(concurrency::create_async([that, progressToken]() {
		try {
			if (that->m_main) {
				that->m_main->CloseApp();
			}
		} catch (...) {
		}
	})).then([that, progressToken](concurrency::task<void> t) {
		try {
			t.get();

			// UI is sent back to HostSelectorPage in StartRenderLoop(), after the loop exits
			// All we need to do is close the progress dialog

			DISPATCH_UI([progressToken] {
				::moonlight_xbox_dx::ModalDialog::HideDialogByToken(progressToken);
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
	this->m_main->SendGuideButton(500);
}


void StreamPage::guideButtonLong_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->m_main->SendGuideButton(3000);
}


void StreamPage::toggleHDR_WinAltB_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	this->m_main->SendWinAltB();
}

void StreamPage::resetDecoder_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	// start with a fresh IDR
	LiRequestIdrFrame();

	// This also is a handy place to flush the queue
	FrameQueue::instance().clear();
	ImGuiPlots::instance().clearData(PLOT_QUEUED_FRAMES);
}

void StreamPage::toggleFramePacing_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	// thread safe atomic bool
	bool isImmediate = Pacer::instance().getPacingImmediate();
	Pacer::instance().setPacingImmediate(isImmediate ? false : true);
}

void StreamPage::toggleCapture_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	SetCaptureMode(!this->CaptureMode);
}

void StreamPage::SetCaptureMode(bool wanted)
{
	this->CaptureMode = wanted;
	FFMpegDecoder::instance().SetCapture(wanted);
}

// Audio buffer slider

void StreamPage::audioBufferSlider_Loaded(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^) {
	m_audioBufferSlider = safe_cast<Windows::UI::Xaml::Controls::Slider ^>(sender);

	// Seed from the live value; ValueChanged fires during template load with
	// the coerced default, which must not be pushed back into AudioPlayer.
	m_audioBufferSliderReady = false;
	m_audioBufferSlider->Value = AudioPlayer::instance().GetAudioBufferMs();
	m_audioBufferSliderReady = true;
}

void StreamPage::audioBufferSlider_ValueChanged(Platform::Object ^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs ^ args) {
	if (!m_audioBufferSliderReady) {
		return;
	}

	int audioBufferMs = static_cast<int>(args->NewValue);
	AudioPlayer::instance().SetAudioBufferMs(audioBufferMs);
}

void StreamPage::audioBufferSliderItem_PreviewKeyDown(Platform::Object ^, Windows::UI::Xaml::Input::KeyRoutedEventArgs ^ args) {
	if (m_audioBufferSlider == nullptr) {
		return;
	}

	double change = 0;

	switch (args->Key) {
	case Windows::System::VirtualKey::Left:
	case Windows::System::VirtualKey::GamepadDPadLeft:
	case Windows::System::VirtualKey::GamepadLeftThumbstickLeft:
		change = -10;
		break;

	case Windows::System::VirtualKey::Right:
	case Windows::System::VirtualKey::GamepadDPadRight:
	case Windows::System::VirtualKey::GamepadLeftThumbstickRight:
		change = 10;
		break;

	default:
		return;
	}

	double value = m_audioBufferSlider->Value + change;

	if (value < m_audioBufferSlider->Minimum) {
		value = m_audioBufferSlider->Minimum;
	} else if (value > m_audioBufferSlider->Maximum) {
		value = m_audioBufferSlider->Maximum;
	}

	m_audioBufferSlider->Value = value;
	args->Handled = true;
}

// Glitch counter

void StreamPage::audioGlitchText_Loaded(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^) {
	m_audioGlitchText = safe_cast<Windows::UI::Xaml::Controls::TextBlock ^>(sender);
	UpdateAudioGlitchText();
}

void StreamPage::UpdateAudioGlitchText() {
	if (m_audioGlitchText == nullptr) {
		return;
	}


	m_audioGlitchText->Text = Utils::StringFromStdString(
		"Glitches: " + std::to_string(AudioPlayer::instance().GetGlitchCount()) +
		" / Drops to reduce latency: " + std::to_string(AudioPlayer::instance().GetLatencyDropCount()));
}

// Event handlers

void StreamPage::OnPropertyChanged(Platform::String^ propertyName)
{
	PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
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

void StreamPage::OnHdmiDisplayModesChanged(HdmiDisplayInformation^, Platform::Object^) {
	Utils::Log("HDMI display mode changed, requesting refresh\n");
	m_refreshDisplay.store(true, std::memory_order_release);
}

bool StreamPage::ShouldRefreshDisplay() {
	return m_refreshDisplay.exchange(false);
}
