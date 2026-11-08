//
// DirectXPage.xaml.h
// Declaration of the DirectXPage class.
//

#pragma once

#include "Pages\StreamPage.g.h"

#include "Common\DeviceResources.h"
#include "Streaming\moonlight_xbox_dxMain.h"
#include "KeyboardControl.xaml.h"
#include "Converters\BoolToTextConverter.h"

// muxc removed for W10M (system XAML only)
using namespace Windows::UI::Xaml::Controls;
namespace moonlight_xbox_dx
{
	/// <summary>
	/// A page that hosts a DirectX SwapChainPanel.
	/// </summary>

	class moonlight_xbox_dxMain;
	public ref class StreamPage sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
	{
	public:
		StreamPage();
		virtual ~StreamPage();
		virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
		void OnPropertyChanged(Platform::String^ propertyName);
		bool ShouldRefreshGamepads();
		void RequestRefreshGamepads();
		bool ShouldRefreshDisplay();

		property bool CaptureMode {
			bool get() { return m_captureMode; }
			void set(bool value) {
				if (m_captureMode != value) {
					m_captureMode = value;
					OnPropertyChanged("CaptureMode");
				}
			}
	    }

		property bool MouseMode {
			bool get() { return m_mouseMode; }
			void set(bool value) {
				if (m_mouseMode != value) {
					m_mouseMode = value;
					OnPropertyChanged("MouseMode");
				}
			}
	    }

	    property bool ShowLogs {
		    bool get() {
			    return m_showLogs;
		    }
		    void set(bool value) {
			    if (m_showLogs != value) {
				    m_showLogs = value;
				    OnPropertyChanged("ShowLogs");
			    }
		    }
	    }

	    property bool ShowStats {
		    bool get() {
			    return m_showStats;
		    }
		    void set(bool value) {
			    if (m_showStats != value) {
				    m_showStats = value;
				    OnPropertyChanged("ShowStats");
			    }
		    }
	    }

		void OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args);
		property ApplicationState^ State {
			ApplicationState^ get() {
				return GetApplicationState();
			}
		}
		property MenuFlyout^ m_flyout {
			MenuFlyout^ get() {
				return this->ActionsFlyout;
			}
		}

		property Button^ m_flyoutButton {
			Button^ get() {
				return this->flyoutButton;
			}
		}

		property Grid^ m_progressView {
			Grid^ get() {
				return this->ProgressView;
			}
		}

		property Windows::UI::Xaml::Controls::ProgressRing^ m_progressRing {
		    Windows::UI::Xaml::Controls::ProgressRing ^ get() {
				return this->MainProgressRing;
			}
		}

		property TextBlock^ m_statusText {
			TextBlock^ get() {
				return this->StatusText;
			}
		}

		property TextBlock^ m_stepText {
			TextBlock^ get() {
				return this->StepText;
			}
		}

		property StackPanel^ m_keyboardView {
			StackPanel^ get() {
				return this->KeyboardView;
			}
		}

		property KeyboardControl^ m_keyboard {
			KeyboardControl^ get() {
				return this->Keyboard;
			}
		}

	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
	private:
		// Track our independent input on a background worker thread.
		Windows::Foundation::IAsyncAction^ m_inputLoopWorker;
		Windows::UI::Core::CoreIndependentInputSource^ m_coreInput;
	    Windows::Foundation::EventRegistrationToken m_back_cookie;
	    // W10M: touch pointer events on the swap chain panel act as a
	    // trackpad for the host cursor; physical mice pass through in
	    // relative mode via MouseDevice.
	    Windows::Foundation::EventRegistrationToken m_pointerPressedCookie;
	    Windows::Foundation::EventRegistrationToken m_pointerMovedCookie;
	    Windows::Foundation::EventRegistrationToken m_pointerReleasedCookie;
	    Windows::Devices::Input::MouseDevice^ m_mouseDevice;
	    Windows::Foundation::EventRegistrationToken m_mouseMovedCookie;
	    Windows::Foundation::EventRegistrationToken m_visibleBoundsChangedCookie;
	    int m_activePointerButton = 0;
	    // Touch cursor mode state (trackpad style: slide = relative cursor move,
	    // tap = click at the cursor). Physical mice are absolute and share the
	    // same cursor coordinates.
	    bool m_touchIsDown = false;
	    Windows::Foundation::Point m_touchDownPos = { 0, 0 };
	    Windows::Foundation::Point m_touchLastPos = { 0, 0 };
	    uint64_t m_touchDownMs = 0;
	    int m_touchPointerId = -1;
	    int m_cursorX = 640;
	    int m_cursorY = 360;

		// Resources used to render the DirectX content in the XAML page background.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::unique_ptr<moonlight_xbox_dxMain> m_main;
		bool m_windowVisible;
	    void Page_Loaded(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e);
	    void Page_Unloaded(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e);

		void OnSwapChainPanelSizeChanged(Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
		void OnPointerPressed(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void OnPointerMoved(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void OnPointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void OnMouseMovedRelative(Windows::Devices::Input::MouseDevice^ sender, Windows::Devices::Input::MouseEventArgs^ args);
		void OnVisibleBoundsChanged(Windows::UI::ViewManagement::ApplicationView^ sender, Platform::Object^ args);
		void flyoutButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ActionsFlyout_Closed(Platform::Object^ sender, Platform::Object^ e);
		void toggleMouseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	    void SetMouseMode(bool enabled);
		void showKeyboardButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void toggleLogsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	    void SetShowLogs(bool show);
		StreamConfiguration^ configuration;
		void toggleStatsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	    void SetShowStats(bool show);
		void disonnectButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);
		void OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);
		void disconnectAndCloseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		Windows::Foundation::EventRegistrationToken keyDownHandler, keyUpHandler;
		void Keyboard_OnKeyDown(moonlight_xbox_dx::KeyboardControl^ sender, moonlight_xbox_dx::KeyEvent^ e);
		void Keyboard_OnKeyUp(moonlight_xbox_dx::KeyboardControl^ sender, moonlight_xbox_dx::KeyEvent^ e);
		void guideButtonShort_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void guideButtonLong_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void toggleHDR_WinAltB_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void resetDecoder_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void toggleFramePacing_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void toggleCapture_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void SetCaptureMode(bool wanted);

		Windows::UI::Xaml::Controls::Slider^ m_audioBufferSlider;
		bool m_audioBufferSliderReady = false;
		void audioBufferSlider_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void audioBufferSliderItem_PreviewKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ args);
		void audioBufferSlider_ValueChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ args);

		Windows::UI::Xaml::Controls::TextBlock^ m_audioGlitchText;
		void audioGlitchText_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
		void UpdateAudioGlitchText();

		Windows::Foundation::EventRegistrationToken gamepadAddedHandler, gamepadRemovedHandler;
		std::atomic<bool> m_refreshGamepads{false};
		void OnGamepadAdded(Platform::Object^, Windows::Gaming::Input::Gamepad^ args);
		void OnGamepadRemoved(Platform::Object^, Windows::Gaming::Input::Gamepad^ args);

		Windows::Graphics::Display::Core::HdmiDisplayInformation^ m_hdmiInfo;
		Windows::Foundation::EventRegistrationToken m_hdmiHandler{};
		std::atomic<bool> m_refreshDisplay{false};
		void OnHdmiDisplayModesChanged(Windows::Graphics::Display::Core::HdmiDisplayInformation^, Platform::Object^);



		bool m_captureMode = false;
        bool m_mouseMode = false;
	    bool m_showLogs = false;
	    bool m_showStats = false;
	};
}

