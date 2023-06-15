//
// KeyboardControl.xaml.cpp
// Implementation of the KeyboardControl class
//

#include "pch.h"
#include "KeyboardControl.xaml.h"
#include <regex>
#include "Utils.hpp"
#include "Keyboard/KeyboardCommon.h"
#include <format>

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

KeyboardControl::KeyboardControl()
{
	InitializeComponent();
	std::string layoutName = "us";
	if (GetApplicationState()->EnableKeyboard && GetApplicationState()->KeyboardLayout != nullptr && GetApplicationState()->KeyboardLayout->Length() > 0) {
		layoutName = Utils::PlatformStringToStdString(GetApplicationState()->KeyboardLayout);
	}
	layout = keyboardLayouts[layoutName];
 	UpdateKeys();
}

void KeyboardControl::UpdateLabel(int virtualCode, wchar_t c) {
	if (c == '\n' || c == '\r' || c == '\t' || c == 0x1b || c == 0x8)return;
	int i = 0;
	for (i = 0; i < layout.bMaxVSCtoVK; i++) {
		if (layout.pusVSCtoVK[i] == virtualCode)break;
	}
	if (i == layout.bMaxVSCtoVK)return;
	char hex[5];
	sprintf(hex, "%02X", i);
	std::string s = std::string(hex);
	Platform::String^ name = ref new Platform::String(L"Key_");
	name = name->Concat(name, Utils::StringFromStdString(s));
	auto b = FindName(name);
	if (b == nullptr) return;
	Windows::UI::Xaml::Controls::Button^ button = (Windows::UI::Xaml::Controls::Button^)b;
	button->Content = c;
}

void KeyboardControl::UpdateKeys() {
	((Button^)FindName("Key_2A"))->Background = toggleShift ? (ref new SolidColorBrush((Windows::UI::Color)Resources->Lookup("SystemAccentColor"))) : (ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(16,255,255,255)));
	((Button^)FindName("Key_36"))->Background = toggleShift ? (ref new SolidColorBrush((Windows::UI::Color)Resources->Lookup("SystemAccentColor"))) : (ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(16, 255, 255, 255)));
	((Button^)FindName("Key_38"))->Background = toggleAlt ? (ref new SolidColorBrush((Windows::UI::Color)Resources->Lookup("SystemAccentColor"))) : (ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(16, 255, 255, 255)));
	((Button^)FindName("Key_E038"))->Background = toggleAlt ? (ref new SolidColorBrush((Windows::UI::Color)Resources->Lookup("SystemAccentColor"))) : (ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(16, 255, 255, 255)));
	((Button^)FindName("Key_E01D"))->Background = toggleCtrl ? (ref new SolidColorBrush((Windows::UI::Color)Resources->Lookup("SystemAccentColor"))) : (ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(16, 255, 255, 255)));
	((Button^)FindName("Key_1D"))->Background = toggleCtrl ? (ref new SolidColorBrush((Windows::UI::Color)Resources->Lookup("SystemAccentColor"))) : (ref new SolidColorBrush(Windows::UI::ColorHelper::FromArgb(16, 255, 255, 255)));
	
	auto currentModifier = layout.pVkToWcharTable;
	while (true) {
		if (currentModifier->nModifications == 0)break;
		int indexModifier = (toggleShift ? 1 : 0) + (toggleCtrl ? 2 : 0) + (toggleAlt ? 4 : 0);
		indexModifier = layout.pCharModifiers->ModNumber[indexModifier];
		if (currentModifier->nModifications == 1) {
			VK_TO_WCHARS1* a = (VK_TO_WCHARS1*)currentModifier->pVkToWchars;
			while (true) {
				if (a->VirtualKey == 0) break;
				UpdateLabel(a->VirtualKey, a->wch[0]);
				a++;
			}
		}
		else if (currentModifier->nModifications == 2) {
			VK_TO_WCHARS2* a = (VK_TO_WCHARS2*)currentModifier->pVkToWchars;
			while (true) {
				if (a->VirtualKey == 0) break;
				UpdateLabel(a->VirtualKey, a->wch[indexModifier >= 2 ? 0 : indexModifier]);
				a++;
			}
		}
		else if (currentModifier->nModifications == 3) {
			VK_TO_WCHARS3* a = (VK_TO_WCHARS3*)currentModifier->pVkToWchars;
			while (true) {
				if (a->VirtualKey == 0) break;
				UpdateLabel(a->VirtualKey, a->wch[indexModifier >= 3 ? 0 : indexModifier]);
				a++;
			}
		}
		else if (currentModifier->nModifications == 4) {
			VK_TO_WCHARS4* a = (VK_TO_WCHARS4*)currentModifier->pVkToWchars;
			while (true) {
				if (a->VirtualKey == 0) break;
				UpdateLabel(a->VirtualKey, a->wch[indexModifier >= 4 ? 0 : indexModifier]);
				a++;
			}
		} 
		else if (currentModifier->nModifications == 5) {
			VK_TO_WCHARS5* a = (VK_TO_WCHARS5*)currentModifier->pVkToWchars;
			while (true) {
				if (a->VirtualKey == 0) break;
				UpdateLabel(a->VirtualKey, a->wch[indexModifier >= 5 ? 0 : indexModifier]);
				a++;
			}
		}
		else if (currentModifier->nModifications == 6) {
			VK_TO_WCHARS6* a = (VK_TO_WCHARS6*)currentModifier->pVkToWchars;
			while (true) {
				if (a->VirtualKey == 0) break;
				UpdateLabel(a->VirtualKey, a->wch[indexModifier >= 6 ? 0 : indexModifier]);
				a++;
			}
		}
		currentModifier++;
	}
}

void moonlight_xbox_dx::KeyboardControl::Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
	Windows::UI::Xaml::Controls::Button^ button = (Windows::UI::Xaml::Controls::Button^)sender;
	if (button->Name == "Key_2A" || button->Name == "Key_36") {
		toggleShift = !toggleShift;
		KeyEvent^ ev = ref new KeyEvent();
		ev->VirtualKey = (unsigned short)Windows::System::VirtualKey::Shift;
		toggleShift ? OnKeyDown(this, ev) : OnKeyUp(this, ev);
	}
	else if (button->Name == "Key_38" || button->Name == "Key_E038") {
		toggleAlt = !toggleAlt;
		KeyEvent^ ev = ref new KeyEvent();
		ev->VirtualKey = (unsigned short)Windows::System::VirtualKey::Menu;
		toggleAlt ? OnKeyDown(this, ev) : OnKeyUp(this, ev);
	}
	else if (button->Name == "Key_1D" || button->Name == "Key_E01D") {
		toggleCtrl = !toggleCtrl;
		KeyEvent^ ev = ref new KeyEvent();
		ev->VirtualKey = (unsigned short)Windows::System::VirtualKey::Control;
		toggleCtrl ? OnKeyDown(this, ev) : OnKeyUp(this, ev);
	}
	else {
		std::string name = Utils::PlatformStringToStdString(button->Name);
		name = name.substr(name.find("_") + 1, 8);
		int number = (int)strtol(name.data(), NULL, 16);
		KeyEvent^ ev = ref new KeyEvent();
		if (number > 0xE000) {
			number -= 0xE000;
			auto a = layout.pVSCtoVK_E0;
			while (true) {
				if (a->Vk == 0)break;
				if (a->Vsc == number) {
					ev->VirtualKey = a->Vk;
					break;
				}
				a++;
			}
		}
		else {
			ev->VirtualKey = layout.pusVSCtoVK[number];
		}
		int currentState = toggleShift + (toggleCtrl ? 2 : 0) + (toggleAlt ? 4 : 0);
		ev->Modifiers = currentState;
		OnKeyDown(this, ev);
		OnKeyUp(this, ev);
		if (toggleShift) {
			toggleShift = false;
			KeyEvent^ ev = ref new KeyEvent();
			ev->VirtualKey = (unsigned short)Windows::System::VirtualKey::Shift;
			OnKeyUp(this, ev);
		}
		if (toggleCtrl) {
			toggleCtrl = false;
			KeyEvent^ ev = ref new KeyEvent();
			ev->VirtualKey = (unsigned short)Windows::System::VirtualKey::Control;
			OnKeyUp(this, ev);
		}
		if (toggleAlt) {
			toggleAlt = false;
			KeyEvent^ ev = ref new KeyEvent();
			ev->VirtualKey = (unsigned short)Windows::System::VirtualKey::Menu;
			OnKeyUp(this, ev);
		}
	}
	UpdateKeys();
}