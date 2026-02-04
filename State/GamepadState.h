#pragma once

#include <algorithm>

enum class ComboState {
	None,
	ViewWaiting,
	MenuWaiting,
	ComboActive
};

struct GamepadComboState {
	bool viewPressed = false;
	bool menuPressed = false;
	int64_t startTime = 0;
	ComboState comboState = ComboState::None;
};

struct ComboResult {
	Windows::Gaming::Input::GamepadReading currentReading; // untouched reading
	Windows::Gaming::Input::GamepadReading maskedReading;  // reading with pending combo buttons masked out
	bool comboTriggered;                                   // True when combo completes
};

struct GamepadState {
	Windows::Gaming::Input::Gamepad ^ controller;
	uint32_t localId;         // index into Gamepad::Gamepads vector, used during add/remove
	uint8_t hostId;           // index this host is mapped to on the host, used for all commands
	int64_t lastRefreshedQpc; // timestamp of last refresh
	bool didSendArrival;      // do we need to send LiSendControllerArrivalEvent?

	Windows::Gaming::Input::GamepadReading reading;
	Windows::Gaming::Input::GamepadReading previousReading;
	GamepadComboState combo;

	short ltX, ltY, rtX, rtY;   // after a call to normalizeAxes() these are
	unsigned char lTrig, rTrig; // populated with values expected by the protocol

	static inline Windows::Gaming::Input::GamepadReading EmptyReading() {
		return Windows::Gaming::Input::GamepadReading{};
	}

	static inline bool isPressed(Windows::Gaming::Input::GamepadButtons buttons, Windows::Gaming::Input::GamepadButtons b) {
		return (buttons & b) == b;
	}

	static inline Windows::Gaming::Input::GamepadButtons clearButtons(Windows::Gaming::Input::GamepadButtons buttons, Windows::Gaming::Input::GamepadButtons mask) {
		return static_cast<Windows::Gaming::Input::GamepadButtons>(
		    static_cast<uint32_t>(buttons) & ~static_cast<uint32_t>(mask));
	}

	static inline Windows::Gaming::Input::GamepadButtons setButtons(Windows::Gaming::Input::GamepadButtons buttons, Windows::Gaming::Input::GamepadButtons mask) {
		return static_cast<Windows::Gaming::Input::GamepadButtons>(
		    static_cast<uint32_t>(buttons) | static_cast<uint32_t>(mask));
	}

	void Reset() {
		controller = nullptr;
		localId = hostId = 0;
		lastRefreshedQpc = 0;
		didSendArrival = false;
		reading = EmptyReading();
		previousReading = EmptyReading();
		ltX = ltY = rtX = rtY = 0;
		lTrig = rTrig = 0;
		combo.comboState = ComboState::None;
		combo.viewPressed = false;
		combo.menuPressed = false;
		combo.startTime = 0;
	}

	ComboResult GetComboResult(int comboTimeoutMs) {
		using namespace Windows::Gaming::Input;

		ComboResult result = ComboResult{EmptyReading(), EmptyReading(), false};
		if (controller == nullptr) {
			return result;
		}

		auto gamepads = Gamepad::Gamepads;
		if (localId >= gamepads->Size) {
			return result;
		}

		Gamepad^ gamepad = gamepads->GetAt(localId);
		if (gamepad == nullptr) {
			return result;
		}

		result.currentReading = gamepad->GetCurrentReading();
		auto buttons = result.currentReading.Buttons;

		const bool viewCurrentlyPressed = isPressed(buttons, GamepadButtons::View);
		const bool menuCurrentlyPressed = isPressed(buttons, GamepadButtons::Menu);

		// Start with unmasked buttons
		GamepadButtons maskedButtons = buttons;

		switch (combo.comboState) {
		case ComboState::None:
			// Handle simultaneous press in the same poll
			if (viewCurrentlyPressed && menuCurrentlyPressed && !combo.viewPressed && !combo.menuPressed) {
				combo.comboState = ComboState::ComboActive;
				result.comboTriggered = true;
				maskedButtons = clearButtons(buttons, GamepadButtons::View | GamepadButtons::Menu);
				// Check if either button is newly pressed
			} else if (viewCurrentlyPressed && !combo.viewPressed) {
				combo.comboState = ComboState::ViewWaiting;
				combo.startTime = QpcNow();
				maskedButtons = clearButtons(buttons, GamepadButtons::View);
			} else if (menuCurrentlyPressed && !combo.menuPressed) {
				combo.comboState = ComboState::MenuWaiting;
				combo.startTime = QpcNow();
				maskedButtons = clearButtons(buttons, GamepadButtons::Menu);
			}
			break;

		case ComboState::ViewWaiting:
			// Check timeout
			if (QpcToMs(QpcNow() - combo.startTime) > comboTimeoutMs) {
				combo.comboState = ComboState::None;
				maskedButtons = setButtons(buttons, GamepadButtons::View);
				break;
			}

			// Check if View was released before combo completed
			if (!viewCurrentlyPressed) {
				combo.comboState = ComboState::None;
				maskedButtons = setButtons(buttons, GamepadButtons::View);
				break;
			}

			// Mask View while waiting
			maskedButtons = clearButtons(buttons, GamepadButtons::View);

			// Check if Menu is now pressed (combo complete)
			if (menuCurrentlyPressed && !combo.menuPressed) {
				combo.comboState = ComboState::ComboActive;
				result.comboTriggered = true;
				maskedButtons = clearButtons(buttons, GamepadButtons::View | GamepadButtons::Menu);
			}
			break;

		case ComboState::MenuWaiting:
			if (QpcToMs(QpcNow() - combo.startTime) > comboTimeoutMs) {
				combo.comboState = ComboState::None;
				maskedButtons = setButtons(buttons, GamepadButtons::Menu);
				break;
			}

			if (!menuCurrentlyPressed) {
				combo.comboState = ComboState::None;
				maskedButtons = setButtons(buttons, GamepadButtons::Menu);
				break;
			}

			maskedButtons = clearButtons(buttons, GamepadButtons::Menu);

			if (viewCurrentlyPressed && !combo.viewPressed) {
				combo.comboState = ComboState::ComboActive;
				result.comboTriggered = true;
				maskedButtons = clearButtons(buttons, GamepadButtons::View | GamepadButtons::Menu);
			}
			break;

		case ComboState::ComboActive:
			// Remain in combo state while both are held
			if (viewCurrentlyPressed && menuCurrentlyPressed) {
				// Continue masking both buttons
				maskedButtons = clearButtons(buttons, GamepadButtons::View | GamepadButtons::Menu);
			} else {
				// One or both released, reset state
				combo.comboState = ComboState::None;
			}
			break;
		}

		combo.viewPressed = viewCurrentlyPressed;
		combo.menuPressed = menuCurrentlyPressed;
		result.maskedReading = result.currentReading;
		result.maskedReading.Buttons = maskedButtons;

		return result;
	}

	static inline short ClampAxis(float v) {
		if (!std::isfinite(v)) return 0;
		v = std::clamp(v, -1.0f, 1.0f);

		// Map [-1.0,1.0] -> [-32768,32767]
		int s = (int)std::lrintf(v * 32768.0f);
		s = std::clamp(s, -32768, 32767);
		return (short)s;
	}

	static inline unsigned char ClampTrigger(float v) {
		if (!std::isfinite(v)) return 0;
		v = std::clamp(v, 0.0f, 1.0f);

		int t = (int)std::lrintf(v * 255.0f);
		t = std::clamp(t, 0, 255);
		return (unsigned char)t;
	}

	void normalizeAxes() {
		ltX = ClampAxis((float)reading.LeftThumbstickX);
		ltY = ClampAxis((float)reading.LeftThumbstickY);
		rtX = ClampAxis((float)reading.RightThumbstickX);
		rtY = ClampAxis((float)reading.RightThumbstickY);

		lTrig = ClampTrigger((float)reading.LeftTrigger);
		rTrig = ClampTrigger((float)reading.RightTrigger);
	}

	bool hasGamepadReadingChanged() {
		auto p = previousReading;

		if (reading.Buttons != p.Buttons) {
			return true;
		}

		short pltX = ClampAxis((float)p.LeftThumbstickX);
		short pltY = ClampAxis((float)p.LeftThumbstickY);
		short prtX = ClampAxis((float)p.RightThumbstickX);
		short prtY = ClampAxis((float)p.RightThumbstickY);
		if (ltX != pltX || ltY != pltY || rtX != prtX || rtY != prtY) {
			return true;
		}

		unsigned char plTrig = ClampTrigger((float)p.LeftTrigger);
		unsigned char prTrig = ClampTrigger((float)p.RightTrigger);
		if (lTrig != plTrig || rTrig != prTrig) {
			return true;
		}

		return false;
	}
};
