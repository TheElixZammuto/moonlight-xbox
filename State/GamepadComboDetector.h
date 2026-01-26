#pragma once

#include <array>

class GamepadComboDetector {
  public:
	struct ComboResult {
		Windows::Gaming::Input::GamepadReading currentReading; // untouched reading
		Windows::Gaming::Input::GamepadReading maskedReading;  // reading with pending combo buttons masked out
		bool comboTriggered;                                   // True when combo completes
	};

	GamepadComboDetector() = default;
	ComboResult GetComboResult(int gamepadIndex, int comboTimeout = 125);
	void Reset(int gamepadIndex);

  private:
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
	std::array<GamepadComboState, 8> m_comboStates;
};

// TODO: refactor to contain only 1 gamepad and fit better with GamepadState
