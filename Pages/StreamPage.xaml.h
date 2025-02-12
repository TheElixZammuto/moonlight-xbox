//
// DirectXPage.xaml.h
// Declaration of the DirectXPage class.
//

#pragma once

#include "Pages\StreamPage.g.h"

#include "Common\DeviceResources.h"
#include "Streaming\moonlight_xbox_dxMain.h"
#include "KeyboardControl.xaml.h"

using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls;
namespace moonlight_xbox_dx
{
	/// <summary>
	/// A page that hosts a DirectX SwapChainPanel.
	/// </summary>
	
	class moonlight_xbox_dxMain;
	public ref class StreamPage sealed
	{
	public:
		StreamPage();
		virtual ~StreamPage();
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

		property StackPanel^ m_progressView {
			StackPanel^ get() {
				return this->ProgressView;
			}
		}

		property TextBlock^ m_statusText {
			TextBlock^ get() {
				return this->StatusText;
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

		// Resources used to render the DirectX content in the XAML page background.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::unique_ptr<moonlight_xbox_dxMain> m_main; 
		bool m_windowVisible;
		void Page_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnSwapChainPanelSizeChanged(Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
		void flyoutButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ActionsFlyout_Closed(Platform::Object^ sender, Platform::Object^ e);
		void toggleMouseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void toggleLogsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		StreamConfiguration^ configuration;
		void toggleStatsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void disonnectButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);
		void OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);
		void disconnectAndCloseButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		Windows::Foundation::EventRegistrationToken keyDownHandler, keyUpHandler;
		void Keyboard_OnKeyDown(moonlight_xbox_dx::KeyboardControl^ sender, moonlight_xbox_dx::KeyEvent^ e);
		void Keyboard_OnKeyUp(moonlight_xbox_dx::KeyboardControl^ sender, moonlight_xbox_dx::KeyEvent^ e);
		void guideButtonShort_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void guideButtonLong_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}

