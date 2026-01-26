#include "pch.h"
#include "GamepadComboDetector.h"

using Windows::Gaming::Input::Gamepad;
using Windows::Gaming::Input::GamepadButtons;
using Windows::Gaming::Input::GamepadReading;

using namespace moonlight_xbox_dx;

static inline bool isPressed(GamepadButtons buttons, GamepadButtons b) {
	return (buttons & b) == b;
}

static inline GamepadButtons clearButtons(GamepadButtons buttons, GamepadButtons mask) {
	return static_cast<GamepadButtons>(
	    static_cast<uint32_t>(buttons) & ~static_cast<uint32_t>(mask));
}

static inline GamepadButtons setButtons(GamepadButtons buttons, GamepadButtons mask) {
	return static_cast<GamepadButtons>(
		static_cast<uint32_t>(buttons) | static_cast<uint32_t>(mask));
}

static inline GamepadReading EmptyReading() {
	return GamepadReading{};
}

GamepadComboDetector::ComboResult GamepadComboDetector::GetComboResult(int gamepadIndex, int comboTimeoutMs) {
	ComboResult result{};
	result.comboTriggered = false;
	result.currentReading = EmptyReading();
	result.maskedReading = EmptyReading();

	auto gamepads = Gamepad::Gamepads;
	if (gamepads == nullptr || gamepadIndex < 0 || gamepadIndex > 7 || gamepadIndex >= static_cast<int>(gamepads->Size)) {
		// Return a default result. Caller can decide what to do.
		return result;
	}

	Gamepad^ gamepad = gamepads->GetAt(gamepadIndex);
	if (gamepad == nullptr) {
		return result;
	}

	// State is per-gamepad
	auto& combo = m_comboStates[gamepadIndex];

	result.currentReading = gamepad->GetCurrentReading();
	GamepadButtons buttons = result.currentReading.Buttons;

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

void GamepadComboDetector::Reset(int gamepadIndex) {
	auto& combo = m_comboStates[gamepadIndex];
	combo.comboState = ComboState::None;
	combo.viewPressed = false;
	combo.menuPressed = false;
	combo.startTime = 0;
}
