#pragma once

#include "GamepadComboDetector.h"
#include <algorithm>

struct GamepadState {
	Windows::Gaming::Input::Gamepad^ controller;
	uint32_t localId;    // index into Gamepad::Gamepads vector, used during add/remove
	uint8_t hostId;      // index this host is mapped to on the host, used for all commands
	bool didSendArrival; // do we need to send LiSendControllerArrivalEvent?

	Windows::Gaming::Input::GamepadReading reading;
	Windows::Gaming::Input::GamepadReading previousReading;
	GamepadComboDetector comboDetector;

	void Reset() {
		auto emptyReading = Windows::Gaming::Input::GamepadReading{};
		comboDetector.Reset(localId);
		controller = nullptr;
		localId = hostId = 0;
		didSendArrival = false;
		reading = emptyReading;
		previousReading = emptyReading;
	}

	GamepadComboDetector::ComboResult GetComboResult(int comboTimeoutMs) {
		// This also calls GetCurrentReading()
		if (controller != nullptr) {
			return comboDetector.GetComboResult(localId, comboTimeoutMs);
		}
		auto emptyReading = Windows::Gaming::Input::GamepadReading{};
		return GamepadComboDetector::ComboResult{emptyReading, emptyReading, false};
	}

	static inline short ClampAxis(float v) {
		if (!(v == v)) return 0;
		v = std::max(-1.0f, std::min(1.0f, v));
		return static_cast<short>(std::lrintf(v * 32767.0f));
	}

	static inline unsigned char ClampTrigger(float v) {
		if (!(v == v)) return 0;
		v = std::max(-1.0f, std::min(1.0f, v));
		return static_cast<unsigned char>(std::lrintf(v * 255.0f));
	}

	bool hasGamepadReadingChanged() {
		auto a = reading;
		auto b = previousReading;

		if (a.Buttons != b.Buttons) {
			return true;
		}

		short altX = ClampAxis(static_cast<float>(a.LeftThumbstickX));
		short altY = ClampAxis(static_cast<float>(a.LeftThumbstickY));
		short artX = ClampAxis(static_cast<float>(a.RightThumbstickX));
		short artY = ClampAxis(static_cast<float>(a.RightThumbstickY));
		short bltX = ClampAxis(static_cast<float>(b.LeftThumbstickX));
		short bltY = ClampAxis(static_cast<float>(b.LeftThumbstickY));
		short brtX = ClampAxis(static_cast<float>(b.RightThumbstickX));
		short brtY = ClampAxis(static_cast<float>(b.RightThumbstickY));
		if (altX != bltX || altY != bltY || artX != brtX || artY != brtY) {
			return true;
		}

		unsigned char alTrig = ClampTrigger(static_cast<float>(a.LeftTrigger));
		unsigned char arTrig = ClampTrigger(static_cast<float>(a.RightTrigger));
		unsigned char blTrig = ClampTrigger(static_cast<float>(b.LeftTrigger));
		unsigned char brTrig = ClampTrigger(static_cast<float>(b.RightTrigger));
		if (alTrig != blTrig || arTrig != brTrig) {
			return true;
		}

		return false;
	}
};
