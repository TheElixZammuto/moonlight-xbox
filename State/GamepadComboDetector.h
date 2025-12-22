#pragma once

class GamepadComboDetector {
  public:
	struct ComboResult {
		Windows::Gaming::Input::GamepadReading currentReading; // untouched reading
		Windows::Gaming::Input::GamepadReading maskedReading;  // reading with pending combo buttons masked out
		bool comboTriggered;                                   // True when combo completes
	};

	GamepadComboDetector() = default;
	ComboResult GetComboResult(int gamepadIndex, int comboTimeout);

  private:
	enum class ComboState {
		None,
		ViewWaiting,
		MenuWaiting,
		ComboActive
	};

	struct GamepadComboState {
		bool viewPressed;
		bool menuPressed;
		int64_t startTime;
		ComboState comboState;
	};
	std::array<GamepadComboState, 8> m_comboStates;
};
