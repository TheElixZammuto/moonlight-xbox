//
// KeyboardControl.xaml.h
// Declaration of the KeyboardControl class
//

#pragma once

#include "KeyboardControl.g.h"
#include "Keyboard/KeyboardCommon.h"
namespace moonlight_xbox_dx
{
	struct key_t {
		std::string sc;
		std::string vk;
		std::wstring caps[8];
	};

	public ref class KeyEvent sealed {
	public:
		property unsigned short VirtualKey {
			unsigned short get() { return this->virtualKey; }
			void set(unsigned short value) {
				this->virtualKey = value;
			}
		}
		property int Modifiers {
			int get() { return this->modifiers; }
			void set(int v) { this->modifiers = v; }
		}
	private:
		unsigned short virtualKey;
		int modifiers;
	};

	public delegate void SoftwareKeyboardDown(KeyboardControl^ sender, KeyEvent^ e);
	public delegate void SoftwareKeyboardUp(KeyboardControl^ sender, KeyEvent^ e);

	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class KeyboardControl sealed
	{
	public:
		KeyboardControl();
		event SoftwareKeyboardDown^ OnKeyDown;
		event SoftwareKeyboardUp^ OnKeyUp;
	private:
		//https://learn.microsoft.com/it-it/windows/win32/inputdev/virtual-key-codes
		//https://learn.microsoft.com/en-us/uwp/api/windows.system.virtualkey?view=winrt-22621
		void UpdateKeys();
		bool toggleShift = false;
		bool toggleCtrl = false;
		bool toggleAlt = false;
		void UpdateLabel(int virtualCode, wchar_t c);
		void Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void KeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		void KeyUp(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
		void PointerPressed(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		KBDTABLES layout;

	};
}
