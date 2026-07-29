
#pragma once

#include "UI\Pages\StreamPage.g.h"

#include "Common\DeviceResources.h"
#include "Streaming\moonlight_xbox_dxMain.h"
#include "KeyboardControl.xaml.h"
#include "UI\Converters\BoolToTextConverter.h"

using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls;
namespace moonlight_xbox_dx
{

    class moonlight_xbox_dxMain;
    public ref class StreamPage sealed
	{
	public:
		StreamPage();
		virtual ~StreamPage();
        void OnPropertyChanged(Platform::String^ propertyName);
		bool ShouldRefreshGamepads();
		void RequestRefreshGamepads();
		void SetMouseMode(bool enabled);
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
		property Grid^ m_streamMenuGrid {
			Grid^ get() {
				return this->StreamMenuGrid;
			}
		}

		void SetStreamMenuVisible(bool visible);

		property Grid^ m_progressView {
			Grid^ get() {
				return this->ProgressView;
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

		Windows::Foundation::IAsyncAction^ m_inputLoopWorker;
		Windows::UI::Core::CoreIndependentInputSource^ m_coreInput;
	    Windows::Foundation::EventRegistrationToken m_back_cookie;

		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::unique_ptr<moonlight_xbox_dxMain> m_main;
		bool m_windowVisible;
	    void Page_Loaded(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e);
	    void Page_Unloaded(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e);

		void OnSwapChainPanelSizeChanged(Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
		void toggleMouseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OtherButton_GotFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OtherButton_LostFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void SubMenuButton_GotFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void SubMenuButton_LostFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void SubMenuHideStoryboard_Completed(Platform::Object^ sender, Platform::Object^ e);
		void OnSubMenuCloseTimer_Tick(Platform::Object^ sender, Platform::Object^ e);
		void ShowSubMenu();
		void HideSubMenu();
		void InstantHideSubMenu();
		void showKeyboardButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void toggleLogsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	    void SetShowLogs(bool show);
		StreamConfiguration^ configuration;
		Windows::UI::Xaml::Media::Animation::Storyboard^ m_bgPanStoryboard = nullptr;
		Windows::UI::Xaml::Media::Imaging::BitmapImage^ m_backgroundImage = nullptr;
		void StartBgPanAnimation();
		void FadeInBackground();
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

		Windows::Foundation::EventRegistrationToken gamepadAddedHandler, gamepadRemovedHandler;
		std::atomic<bool> m_refreshGamepads{false};
		void OnGamepadAdded(Platform::Object^, Windows::Gaming::Input::Gamepad^ args);
		void OnGamepadRemoved(Platform::Object^, Windows::Gaming::Input::Gamepad^ args);

        bool m_mouseMode = false;
	    bool m_showLogs = false;
	    bool m_showStats = false;
	    bool m_streamMenuVisible = false;
	    bool m_subMenuVisible = false;
	    Windows::UI::Xaml::DispatcherTimer^ m_subMenuCloseTimer = nullptr;
	    void MenuHideStoryboard_Completed(Platform::Object^ sender, Platform::Object^ e);
	};
}
