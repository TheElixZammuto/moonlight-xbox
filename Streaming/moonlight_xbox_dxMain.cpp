#include "pch.h"
#include "moonlight_xbox_dxMain.h"
#include "Common\DirectXHelper.h"
#include "State\GamepadState.h"
#include "../Plot/ImGuiPlots.h"
#include "Utils.hpp"
#include <Pages/StreamPage.xaml.h>
#include <Pages/AppPage.xaml.h>
#include <Pages/HostSelectorPage.xaml.h>
#include <Streaming\FFMpegDecoder.h>

#include <algorithm>

using namespace moonlight_xbox_dx;
using namespace Concurrency;
using namespace DirectX;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Gaming::Input;
using namespace Windows::System::Threading;
using namespace Windows::UI::ViewManagement::Core;

extern "C" {
#include<Limelight.h>
#include <Common/ModalDialog.xaml.h>
}

// Loads and initializes application assets when the application is loaded.
moonlight_xbox_dxMain::moonlight_xbox_dxMain(const std::shared_ptr<DX::DeviceResources>& deviceResources, StreamPage^ streamPage, MoonlightClient* client, StreamConfiguration^ configuration) :
	m_deviceResources(deviceResources), m_pointerLocationX(0.0f), m_streamPage(streamPage), moonlightClient(client) {
	
  Platform::String ^ appName = configuration->appName ? "'" + configuration->appName + "'" : "App";
	DISPATCH_UI(([streamPage, appName]() {
		streamPage->m_stepText->Text = "Starting " + appName;
	}));

	client->OnFailed = ([this, appName](int status, int error, char *message) {
		std::string msgCopy = message ? message : std::string();
		auto self = this;

		DISPATCH_UI(([msgCopy, self, appName]() {
			auto showErrorDialog = std::make_shared<std::function<void()>>();
			auto showLogsDialog = std::make_shared<std::function<void()>>();

			*showErrorDialog = [self, msgCopy, showLogsDialog, appName]() {
				auto dialog1 = ref new Windows::UI::Xaml::Controls::ContentDialog();
				dialog1->Title = L"Failed to start " + appName;
				dialog1->Content = Utils::StringFromStdString(msgCopy);
				dialog1->PrimaryButtonText = L"OK";
				dialog1->SecondaryButtonText = L"Show Logs";

				concurrency::create_task(::moonlight_xbox_dx::ModalDialog::ShowOnceAsync(dialog1)).then([self, showLogsDialog](concurrency::task<Windows::UI::Xaml::Controls::ContentDialogResult> t) {
					auto result = t.get();
					if (result == Windows::UI::Xaml::Controls::ContentDialogResult::Primary) {
						self->StopRenderLoop();
						self->ExitStreamPage();
					}
					else if (result == Windows::UI::Xaml::Controls::ContentDialogResult::Secondary) {
						(*showLogsDialog)();
					}
				});
			};

			*showLogsDialog = [self, showErrorDialog]() {
                auto dialog2 = ref new Windows::UI::Xaml::Controls::ContentDialog();

                std::wstring m_text = L"";
                std::vector<std::wstring> lines = Utils::GetLogLines();

                for (int i = 0; i < (int)lines.size(); i++) {
                    // Get only the last 8 lines
					// More than that cannot be fully viewed on the screen at the current scaling
                    if ((int)lines.size() - i <= 8) {
                        m_text += lines[i];
                    }
                }

                Utils::showLogs = true;

				dialog2->MaxWidth = 600;
				dialog2->Title = "Logs";
				dialog2->Content = ref new Platform::String(m_text.c_str());
				dialog2->PrimaryButtonText = L"OK";
				dialog2->SecondaryButtonText = L"Show Error";

				concurrency::create_task(::moonlight_xbox_dx::ModalDialog::ShowOnceAsync(dialog2)).then([self, showErrorDialog](concurrency::task<Windows::UI::Xaml::Controls::ContentDialogResult> t) {
					auto result = t.get();
					if (result == Windows::UI::Xaml::Controls::ContentDialogResult::Primary) {
						self->StopRenderLoop();
						self->ExitStreamPage();
					}
					else if (result == Windows::UI::Xaml::Controls::ContentDialogResult::Secondary) {
						(*showErrorDialog)();
					}
				});
			};

			// Start by showing the error dialog
			(*showErrorDialog)();
		}));
	});

	client->OnStatusUpdate = ([streamPage](int status) {
		std::string msg = LiGetFormattedStageName(status);
		DISPATCH_UI(([streamPage, msg]() {
			streamPage->m_stepText->Text = Utils::StringFromStdString(msg);
		}));
	});

	// Register to be notified if the Device is lost or recreated
	m_deviceResources->RegisterDeviceNotify(this);

	// Setup stats object. DeviceResources keeps a reference so that various components such as FFMpegDecoder can get to it
	m_stats = std::make_shared<Stats>();
	m_deviceResources->SetStats(m_stats);

	m_sceneRenderer = std::make_shared<VideoRenderer>(m_deviceResources, moonlightClient, configuration);
	
    client->OnCompleted = ([this, streamPage, configuration]() {
        concurrency::create_task([this]() {
            while (this->m_sceneRenderer && !this->m_sceneRenderer->IsLoadingComplete() && !this->moonlightClient->IsConnectionTerminated()) {
                Sleep(50);
            }
        }).then([this, streamPage, configuration](concurrency::task<void> t) {

			if (this->m_sceneRenderer && this->m_sceneRenderer->IsLoadingSuccessful()) {
				DISPATCH_UI(([streamPage]() {
					Sleep(500);
					streamPage->m_progressRing->IsActive = false;
					streamPage->m_progressView->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
				}));
			}

		});
	});

	client->SetHDR = ([this](bool v) {
		concurrency::create_task([this]() {
			while (this->m_sceneRenderer && !this->m_sceneRenderer->IsLoadingComplete() && !this->moonlightClient->IsConnectionTerminated()) {
				Sleep(50);
			}
		}).then([this, v](concurrency::task<void> t) {
			this->m_sceneRenderer->SetHDR(v);
		});
	});

	m_LogRenderer = std::make_unique<LogRenderer>(m_deviceResources);

	m_statsTextRenderer = std::make_unique<StatsRenderer>(m_deviceResources, m_stats);
	m_statsTextRenderer->SetVisible(configuration->enableStats);

	// We're now connected and can register for gamepad events
	for (int i = 0; i < MAX_GAMEPADS; i++) {
		GamepadState& state = m_GamepadState[i];
		state.Reset();
	}

	// Force disable graphs on Xbox One at 4K, they run too slowly
	// XXX can we run them at a lower resolution?
	if (IsXboxOne() && m_deviceResources->GetPixelHeight() >= 2160) {
		configuration->enableGraphs = false;
	}

	m_deviceResources->SetShowImGui(configuration->enableGraphs);
	ImGuiPlots::instance().setEnabled(configuration->enableGraphs);

	client->OnStatusUpdate = ([streamPage](int status) {
		const char* msg = LiGetStageName(status);
		streamPage->m_statusText->Text = Utils::StringFromStdString(std::string(msg));
		});

	client->OnCompleted = ([streamPage]() {
		// Give stream a moment to stabilize before hiding progress view
		Sleep(500);
		streamPage->m_progressRing->IsActive = false;
		streamPage->m_progressView->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	});

	client->OnFailed = ([streamPage](int status, int error, char* message) {
		streamPage->m_statusText->Text = Utils::StringFromStdString(std::string(message));
	});

	client->SetHDR = ([this](bool v) {
		this->m_sceneRenderer->SetHDR(v);
	});

	client->OnRumble = ([this](unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor) {
		auto& state = this->FindGamepadStateByHostId(controllerNumber);
		if (state.controller == nullptr) return;
		auto gamepads = Gamepad::Gamepads;
		if (state.localId >= gamepads->Size) return;
		auto gamepad = gamepads->GetAt(state.localId);
		float normalizedLow = lowFreqMotor / (float)(256 * 256);
		float normalizedHigh = highFreqMotor / (float)(256 * 256);
		GamepadVibration v = gamepad->Vibration;
		v.LeftMotor = normalizedLow;
		v.RightMotor = normalizedHigh;
		gamepad->Vibration = v;
	});

	client->OnTriggerRumble = ([this](unsigned short controllerNumber, unsigned short leftTriggerMotor, unsigned short rightTriggerMotor) {
		auto& state = this->FindGamepadStateByHostId(controllerNumber);
		if (state.controller == nullptr) return;
		auto gamepads = Gamepad::Gamepads;
		if (state.localId >= gamepads->Size) return;
		auto gamepad = gamepads->GetAt(state.localId);
		float normalizedLeft = leftTriggerMotor / (float)(256 * 256);
		float normalizedRight = rightTriggerMotor / (float)(256 * 256);
		GamepadVibration v = gamepad->Vibration;
		v.LeftTrigger = normalizedLeft;
		v.RightTrigger = normalizedRight;
		gamepad->Vibration = v;
	});

	m_timer.SetFixedTimeStep(false);

	double refreshRate = m_deviceResources->GetUWPRefreshRate();
	m_deviceResources->SetRefreshRate(refreshRate);
	m_deviceResources->SetFrameRate(configuration->FPS);

	// Force refresh of connected gamepads because OnGamepadAdded may not always be called if we reconnect
	streamPage->RequestRefreshGamepads();
}

moonlight_xbox_dxMain::~moonlight_xbox_dxMain()
{
	// Deregister device notification
	m_deviceResources->RegisterDeviceNotify(nullptr);
}

void moonlight_xbox_dxMain::CreateDeviceDependentResources()
{
}

// Updates application state when the window size changes (e.g. device orientation change)
void moonlight_xbox_dxMain::CreateWindowSizeDependentResources()
{
	m_sceneRenderer->CreateWindowSizeDependentResources();
	m_LogRenderer->CreateWindowSizeDependentResources();
	m_statsTextRenderer->CreateWindowSizeDependentResources();
}

void moonlight_xbox_dxMain::StartRenderLoop()
{
	// If the animation render loop is already running then do not start another thread.
	if (m_renderLoopWorker != nullptr && m_renderLoopWorker->Status == AsyncStatus::Started) {
		return;
	}

	// Create a task that will be run on a background thread.
	auto workItemHandler = ref new WorkItemHandler([this](IAsyncAction^ action) {
		if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL)) {
			Utils::Logf("Failed to set render thread priority: %d\n", GetLastError());
		}

		int64_t t0 = 0, t1 = 0, t2 = 0, t3 = 0;
		int64_t lastFramePts = 0, lastPresentTime = 0;
		double frametimeMs = 0.0, hostFrametimeMs = 0.0;
		const double bufferMs = 1.5;   // safety wait time to avoid missing deadline
		const double alphaUp = 0.25;   // react faster when renderMs spikes upward
		const double alphaDown = 0.05; // decay slowly when renderMs drops
		double ewmaRenderMs = 3.0;     // Initial guess for render cost

		// Calculate the updated frame and render once per vertical blanking interval.
		while (action->Status == AsyncStatus::Started && !moonlightClient->IsConnectionTerminated()) {
			// Get overall deadline we must hit by the Present for this frame
			int64_t deadline = Pacer::instance().getNextVBlankQpc(&t0);

			// wait for a frame + avg render time + safety buffer
			double maxWaitMs = std::max(0.0, QpcToMs(deadline - t0) - ewmaRenderMs - bufferMs);
			Pacer::instance().waitForFrame(maxWaitMs);
			t1 = QpcNow();

			{
				critical_section::scoped_lock lock(m_criticalSection);
				Update();

				bool rendered = false;
				{
					// ffmpeg and Render both use the same D3D context
					auto guard = FFMpegDecoder::Lock();
					rendered = Render();
					t2 = QpcNow();
				}

				// Whether we rendered a new frame or not, wait until vblank for pacing
				// This is out of the lock and won't block the decoder
				bool hitDeadline = Pacer::instance().waitBeforePresent(deadline);
				t3 = QpcNow();

				if (!rendered) {
					// (Immediate pacing mode only) We're receiving a lower framerate
					// and no frame was available, we don't call Present here and the
					// previous frame will be re-displayed by DWM. On Xbox One this may cause
					// corrupted frames or tearing.
					continue;
				}

				{
					// lock is required around Present
					auto guard = FFMpegDecoder::Lock();
					m_deviceResources->Present();
				}

				// Graph frametime only for new frames
				bool isRepeatFrame = true;
				int64_t currentFramePts = Pacer::instance().getCurrentFramePts();
				if (currentFramePts != lastFramePts) {
					if (lastPresentTime > 0) {
						hostFrametimeMs = ((double)currentFramePts - lastFramePts) / 90.0;
						frametimeMs = QpcToMs(t3 - lastPresentTime);
						ImGuiPlots::instance().observeFloat(PLOT_FRAMETIME, static_cast<float>(frametimeMs));
					}
					lastPresentTime = t3;
					lastFramePts = currentFramePts;
					isRepeatFrame = false;
				}

				// Weighted avg of time spent in Render(), more weight given to a slower render time
				// If we missed our present deadline this frame, aggressively weight this higher so maxWaitMs is smaller.
				// This is clamped to the deadline to prevent outliers
				double renderMs = QpcToMs(t2 - t1);
				double clampedRenderMs = std::clamp(renderMs, 0.0, QpcToMs(deadline - t0));
				double alpha = (clampedRenderMs > ewmaRenderMs) ? alphaUp : alphaDown;
				if (!hitDeadline) alpha *= 2.0;
				ewmaRenderMs = (clampedRenderMs * alpha) + (ewmaRenderMs * (1.0 - alpha));

				// Track high-level render loop stats
				double preWaitMs = QpcToMs(t1 - t0);
				double beforePresentMs = QpcToMs(t3 - t2);
				m_deviceResources->GetStats()->SubmitRenderStats(preWaitMs, renderMs, beforePresentMs, hitDeadline);

				FQLog("render loop %.3fms %s%s%s pts:%.3fs frametime(c:%02.3fms h:%02.3fms) (Deadline %.3fms PreWait %.3fms (max %.3fms) + Render %.3fms (avg %.3f) + Present %.3fms)\n",
				      QpcToMs(t3 - t0),                             // loop time
				      hitDeadline ? " " : "M",                      // missed deadline?
				      isRepeatFrame ? "R" : " ",                    // repeated frame?
				      preWaitMs > maxWaitMs + bufferMs ? "W" : " ", // we waited too long for a frame (including buffer)
				      (double)currentFramePts / 90000.0,            // host's timestamp (in seconds)
				      frametimeMs,                                  // effective client frametime not counting repeated frames
				      hostFrametimeMs,                              // host frametime
				      QpcToMs(deadline - t0),                       // deadline time window until next vblank
				      preWaitMs,                                    // prewait (time spent waiting for new frame to arrive)
				      maxWaitMs,                                    // max wait allowed this frame
				      renderMs,                                     // render time this frame
				      ewmaRenderMs,                                 // average of render time used to control prewait
				      beforePresentMs);                             // wait time to align present to vblank
			}
		}

		// we've lost the connection, clean up
		StopRenderLoop(); // also stops input
		Disconnect();

		DISPATCH_UI([this]() {
			ExitStreamPage();
		});
	});
	m_renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
	if (m_inputLoopWorker != nullptr && m_inputLoopWorker->Status == AsyncStatus::Started) {
		return;
	}
	auto inputItemHandler = ref new WorkItemHandler([this](IAsyncAction^ action)
		{
			const int pollingHz = 500;
			const int64_t pollIntervalQpc = MsToQpc(1000.0 / pollingHz);
			int64_t lastProcessInput = 0;

			while (action->Status == AsyncStatus::Started)
			{
				int64_t now = QpcNow();
				if (now - lastProcessInput >= pollIntervalQpc) {
					lastProcessInput = now;
					ProcessInput();

					if (m_streamPage->ShouldRefreshGamepads()) {
						// Process added/removed gamepads
						RefreshGamepads();
					}
				}
				else {
					const int64_t nextPoll = lastProcessInput + pollIntervalQpc;
					SleepUntilQpc(nextPoll, 500);
				}
			}
		}
	});

	// Run task on a dedicated high priority background thread.
	m_inputLoopWorker = ThreadPool::RunAsync(inputItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);

	moonlightClient->OnCompleted(); // hide Initializing spinny
}

void moonlight_xbox_dxMain::StopRenderLoop()
{
	m_renderLoopWorker->Cancel();
	m_inputLoopWorker->Cancel();
}

// Updates the application state once per frame.
void moonlight_xbox_dxMain::Update()
{
	// Update scene objects.
	m_timer.Tick([&]()
		{
			m_sceneRenderer->Update(m_timer);
			m_LogRenderer->Update(m_timer);
			m_statsTextRenderer->Update(m_timer);
		});
}

// Gamepad handling

static inline bool isPressed(GamepadButtons buttons, GamepadButtons b) {
	return (buttons & b) == b;
}

// new button press
static inline bool PressedEdge(GamepadReading& r, GamepadReading& p, GamepadButtons b) {
	return isPressed(r.Buttons, b) && !isPressed(p.Buttons, b);
}

// new button release
static inline bool ReleaseEdge(GamepadReading& r, GamepadReading& p, GamepadButtons b) {
	return !isPressed(r.Buttons, b) && isPressed(p.Buttons, b);
}

static inline GamepadReading EmptyReading() {
	return GamepadReading{};
}

// Process all input from the user before updating game state
void moonlight_xbox_dxMain::ProcessInput() {
	auto gamepads = Windows::Gaming::Input::Gamepad::Gamepads;
	uint16_t gamepadCount = gamepads->Size;
	moonlightClient->SetGamepadCount(gamepadCount);

	for (UINT i = 0; i < gamepadCount; i++) {
		auto& state = this->FindGamepadState(i);
		auto result = state.GetComboResult(50); // hold buttons for a short time for View + Menu combo

		if (result.comboTriggered) {
			DISPATCH_UI([this], {
				Windows::UI::Xaml::Controls::Flyout::ShowAttachedFlyout(m_streamPage->m_flyoutButton);
			});

			// send an empty controller packet, otherwise Sunshine may see View being kept held down,
			// triggering the "Home/Guide Button Emulation Timeout" to send a Guide button press after a few seconds.
			SendGamepadReadingForState(state, EmptyReading());

			// disable future input until the flyout is closed
			insideFlyout = true;
			continue;
		}

		if (insideFlyout) {
			state.reading = EmptyReading();
			state.previousReading = EmptyReading();
			continue;
		}

		// GetComboResult() will have masked off our combo buttons if they are pending
		auto reading = result.maskedReading;
		auto prevReading = state.previousReading;

		// If mouse mode is enabled the gamepad acts as a mouse, instead we pass the raw events to the host
		if (keyboardMode) {
			auto appState = GetApplicationState();
			double multiplier = ((double)appState->MouseSensitivity) / ((double)4.0f);

			// B to close
			if (PressedEdge(reading, prevReading, GamepadButtons::B)) {
				if (GetApplicationState()->EnableKeyboard) {
					m_streamPage->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this]() {
						                                   m_streamPage->m_keyboardView->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
					                                   }));
					keyboardMode = false;
				} else {
					CoreInputView::GetForCurrentView()->TryHide();
				}
			}
			// X to backspace
			if (PressedEdge(reading, prevReading, GamepadButtons::X)) {
				moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::Back, 0);
			} else if (ReleaseEdge(reading, prevReading, GamepadButtons::X)) {
				moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::Back, 0);
			}
			// Y to Space
			if (PressedEdge(reading, prevReading, GamepadButtons::Y)) {
				moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::Space, 0);
			} else if (ReleaseEdge(reading, prevReading, GamepadButtons::Y)) {
				moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::Space, 0);
			}
			// LB to Left
			if (PressedEdge(reading, prevReading, GamepadButtons::LeftShoulder)) {
				moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::Left, 0);
			} else if (ReleaseEdge(reading, prevReading, GamepadButtons::LeftShoulder)) {
				moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::Left, 0);
			}
			// RB to Right
			if (PressedEdge(reading, prevReading, GamepadButtons::RightShoulder)) {
				moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::Right, 0);
			} else if (ReleaseEdge(reading, prevReading, GamepadButtons::RightShoulder)) {
				moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::Right, 0);
			}
			// Start to Enter
			if (PressedEdge(reading, prevReading, GamepadButtons::Menu)) {
				moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::Enter, 0);
			} else if (ReleaseEdge(reading, prevReading, GamepadButtons::Menu)) {
				moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::Enter, 0);
			}
			// Move with right stick
			if (isPressed(reading.Buttons, GamepadButtons::LeftThumbstick)) {
				moonlightClient->SendScroll((float)pow(reading.RightThumbstickY * multiplier * 2, 3));
				moonlightClient->SendScrollH((float)pow(reading.RightThumbstickX * multiplier * 2, 3));
			} else {
				// Move with right stick instead of the left one in KB mode
				double x = reading.RightThumbstickX;
				if (abs(x) < 0.1)
					x = 0;
				else
					x = x + (x > 0 ? 1 : -1); // Add 1 to make sure < 0 values do not make everything broken
				double y = reading.RightThumbstickY;
				if (abs(y) < 0.1)
					y = 0;
				else
					y = (y * -1) + (y > 0 ? -1 : 1); // Add 1 to make sure < 0 values do not make everything broken
				moonlightClient->SendMousePosition((float)pow(x * multiplier, 3), (float)pow(y * multiplier, 3));
			}
			if (reading.LeftTrigger > 0.25 && state.previousReading.LeftTrigger < 0.25) {
				moonlightClient->SendMousePressed(BUTTON_LEFT);
			} else if (reading.LeftTrigger < 0.25 && state.previousReading.LeftTrigger > 0.25) {
				moonlightClient->SendMouseReleased(BUTTON_LEFT);
			}
			if (reading.RightTrigger > 0.25 && state.previousReading.RightTrigger < 0.25) {
				moonlightClient->SendMousePressed(BUTTON_RIGHT);
			} else if (reading.RightTrigger < 0.25 && state.previousReading.RightTrigger > 0.25) {
				moonlightClient->SendMouseReleased(BUTTON_RIGHT);
			}
		} else if (mouseMode) {
			auto appState = GetApplicationState();
			// Position
			double multiplier = ((double)appState->MouseSensitivity) / ((double)4.0f);
			double x = reading.LeftThumbstickX;
			if (abs(x) < 0.1)
				x = 0;
			else
				x = x + (x > 0 ? 1 : -1); // Add 1 to make sure < 0 values do not make everything broken
			double y = reading.LeftThumbstickY;
			if (abs(y) < 0.1)
				y = 0;
			else
				y = (y * -1) + (y > 0 ? -1 : 1); // Add 1 to make sure < 0 values do not make everything broken
			moonlightClient->SendMousePosition((float)pow(x * multiplier, 3), (float)pow(y * multiplier, 3));

			// Left Click (A or LT)
			if (PressedEdge(reading, prevReading, GamepadButtons::A) || (reading.LeftTrigger > 0.25 && state.previousReading.LeftTrigger < 0.25)) {
				moonlightClient->SendMousePressed(BUTTON_LEFT);
			} else if (ReleaseEdge(reading, prevReading, GamepadButtons::A) || (reading.LeftTrigger < 0.25 && state.previousReading.LeftTrigger > 0.25)) {
				moonlightClient->SendMouseReleased(BUTTON_LEFT);
			}
			// Right Click (X or RT)
			if (PressedEdge(reading, prevReading, GamepadButtons::X) || (reading.RightTrigger > 0.25 && state.previousReading.RightTrigger < 0.25)) {
				moonlightClient->SendMousePressed(BUTTON_RIGHT);
			} else if (ReleaseEdge(reading, prevReading, GamepadButtons::X) || (reading.RightTrigger < 0.25 && state.previousReading.RightTrigger > 0.25)) {
				moonlightClient->SendMouseReleased(BUTTON_RIGHT);
			}
			// Keyboard (Y)
			if (PressedEdge(reading, prevReading, GamepadButtons::Y)) {
				if (GetApplicationState()->EnableKeyboard) {
					m_streamPage->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this]() {
						                                   m_streamPage->m_keyboardView->Visibility = Windows::UI::Xaml::Visibility::Visible;
					                                   }));
					keyboardMode = true;
				} else {
					CoreInputView::GetForCurrentView()->TryShow(CoreInputViewKind::Keyboard);
				}
			}
			// Scroll
			moonlightClient->SendScroll((float)pow(reading.RightThumbstickY * multiplier * 2, 3));
			moonlightClient->SendScrollH((float)pow(reading.RightThumbstickX * multiplier * 2, 3));
			// Xbox/Guide Button (B)
			if (PressedEdge(reading, prevReading, GamepadButtons::B)) {
				moonlightClient->SendGuide(state.hostId, true);
			} else if (ReleaseEdge(reading, prevReading, GamepadButtons::B)) {
				moonlightClient->SendGuide(state.hostId, false);
			}
		} else {
			SendGamepadReadingForState(state, reading);

			// Uncomment to debug gamepad state
			// state.DumpState();
		}
		state.previousReading = reading;
	}
}

uint16_t moonlight_xbox_dxMain::MakeActiveMask() {
	uint16_t activeMask = 0;
	for (int i = 0; i < MAX_GAMEPADS; ++i) {
		if (m_GamepadState[i].controller != nullptr) {
			activeMask |= (1 << m_GamepadState[i].hostId);
		}
	}
	return activeMask;
}

void moonlight_xbox_dxMain::DumpGamepads() {
	// list all controllers with their connected status
	for (int i = 0; i < MAX_GAMEPADS; ++i) {
		if (m_GamepadState[i].controller != nullptr) {
			Utils::Logf("  Gamepad #%d: hostId %d\n", m_GamepadState[i].localId, m_GamepadState[i].hostId);
		}
	}
}

void moonlight_xbox_dxMain::RefreshGamepads() {
	auto gamepads = Gamepad::Gamepads;
	const int count = gamepads->Size;
	const int64_t now = QpcNow();

	// For all connected Gamepads, ensure our mapping is correct
	for (int localId = 0; localId < count; ++localId) {
		auto gamepad = gamepads->GetAt(localId);
		bool found = false;

		// Do we know about this gamepad already?
		for (int i = 0; i < MAX_GAMEPADS; ++i) {
			if (m_GamepadState[i].controller == gamepad) {
				auto& state = m_GamepadState[i];
				// update localId and send arrival packet if necessary
				state.localId = localId;
				state.lastRefreshedQpc = now;
				if (!state.didSendArrival) {
					SendGamepadArrival(state);
					state.didSendArrival = true;
					Utils::Logf("RefreshGamepads: sent arrival packet for Gamepad #%d\n", localId);
				}
				found = true;
				break;
			}
		}

		// It's a new gamepad
		if (!found) {
			for (int i = 0; i < MAX_GAMEPADS; ++i) {
				if (m_GamepadState[i].controller == nullptr) {
					// Save the new controller at the first open slot
					auto& state = m_GamepadState[i];
					state.Reset();
					state.controller = gamepad;
					state.localId = localId;
					state.hostId = i;
					state.lastRefreshedQpc = now;
					state.reading = EmptyReading();
					state.previousReading = EmptyReading();
					SendGamepadArrival(state);
					state.didSendArrival = true;
					Utils::Logf("RefreshGamepads: added new Gamepad #%d in host slot %d\n", state.localId, state.hostId);
					break;
				}
			}
		}
	}

	// Lastly, remove any leftover controllers that are no longer connected
	for (int i = 0; i < MAX_GAMEPADS; ++i) {
		auto& state = m_GamepadState[i];
		if (state.controller != nullptr && state.lastRefreshedQpc != now) {
			// Send a disconnect packet and reset this state slot
			uint16_t activeMaskMinus = MakeActiveMask();
			activeMaskMinus &= ~(1 << state.hostId);
			LiSendMultiControllerEvent(state.hostId, activeMaskMinus, 0, 0, 0, 0, 0, 0, 0);
			Utils::Logf("RefreshGamepads: removed Gamepad #%d from host slot %d\n", state.localId, state.hostId);
			state.Reset();
		}
	}
}

void moonlight_xbox_dxMain::SendGamepadArrival(GamepadState& state) {
	// Only ever send this once
	if (state.didSendArrival) return;

	uint8_t type = IsXbox() ? LI_CTYPE_XBOX : LI_CTYPE_UNKNOWN;
	uint32_t supportedButtonFlags
		= A_FLAG | B_FLAG | X_FLAG | Y_FLAG
		| BACK_FLAG | PLAY_FLAG | LS_CLK_FLAG | RS_CLK_FLAG
		| UP_FLAG | DOWN_FLAG | LEFT_FLAG | RIGHT_FLAG
		| LB_FLAG | RB_FLAG;
	uint32_t capabilities = LI_CCAP_ANALOG_TRIGGERS | LI_CCAP_RUMBLE | LI_CCAP_TRIGGER_RUMBLE;
	int rc = LiSendControllerArrivalEvent(state.hostId, MakeActiveMask(), type, supportedButtonFlags, capabilities);
	if (rc != 0) {
		Utils::Logf("LiSendControllerArrivalEvent error: %d\n", rc);
	}
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool moonlight_xbox_dxMain::Render()
{
	// Don't try to render anything before the first Update.
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	// Render the scene objects.
	bool showImGui = m_deviceResources->GetShowImGui();

	// ImGui setup and update handling (which we don't use)
	if (showImGui) {
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplUwp_NewFrame(m_deviceResources->GetPixelWidth(), m_deviceResources->GetPixelHeight());
		ImGui::NewFrame();
	}

	bool shouldPresent = Pacer::instance().renderOnMainThread(m_sceneRenderer);
	if (shouldPresent) {
		// avoid useless rendering without an underlying frame change
		m_LogRenderer->Render();
		m_statsTextRenderer->Render(showImGui);
	}

	if (showImGui) {
		ImGui::EndFrame();

		if (shouldPresent) {
			RenderImGui();
			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}
	}

	return shouldPresent;
}

// Set this to true to use the ImGui demo/debug tools.
// Normal ImGui code can be used anywhere in the other Render() methods.
void moonlight_xbox_dxMain::RenderImGui()
{
	bool show_demo_window = false;
	bool show_metrics = false;

	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	if (show_demo_window) {
		ImGui::ShowDemoWindow(&show_demo_window);
	}

	if (show_metrics) {
		ImGui::ShowMetricsWindow(&show_metrics);
	}
}

// Notifies renderers that device resources need to be released.
void moonlight_xbox_dxMain::OnDeviceLost()
{
	m_sceneRenderer->ReleaseDeviceDependentResources();
	m_LogRenderer->ReleaseDeviceDependentResources();
	m_statsTextRenderer->ReleaseDeviceDependentResources();
}

// Notifies renderers that device resources may now be recreated.
void moonlight_xbox_dxMain::OnDeviceRestored()
{
	m_sceneRenderer->CreateDeviceDependentResources();
	m_LogRenderer->CreateDeviceDependentResources();
	m_statsTextRenderer->CreateDeviceDependentResources();

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

void moonlight_xbox_dxMain::SetFlyoutOpened(bool value) {
	insideFlyout = value;
}

void moonlight_xbox_dxMain::Disconnect() {
	moonlightClient->StopStreaming();
	m_sceneRenderer->Stop();
}

void moonlight_xbox_dxMain::CloseApp() {
	moonlightClient->StopApp();
}

void moonlight_xbox_dxMain::ExitStreamPage() {
	
	bool reachedAppPage = false;

	try {
		auto rootFrame = dynamic_cast<Windows::UI::Xaml::Controls::Frame ^>(Windows::UI::Xaml::Window::Current->Content);
		if (!rootFrame) return;

		auto current = dynamic_cast<AppPage ^>(rootFrame->Content);
		if (current != nullptr) {
			reachedAppPage = true;
		}

		try {
			rootFrame->GoBack();
		} catch (...) {
			Utils::Log("ExitStreamPage: Failed to GoBack()\n");
		}

		if (!reachedAppPage) {
			if (dynamic_cast<AppPage ^>(rootFrame->Content) != nullptr) reachedAppPage = true;
		}

		if (!reachedAppPage) {
			try {
				rootFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSelectorPage::typeid));
			} catch (...) {
				rootFrame->Content = nullptr;
				Utils::Log("ExitStreamPage: Failed to return to HostSelectorPage\n");
			}
		}
	} catch (...) {
		Utils::Log("ExitStreamPage: An error occurred\n");
	}
}

void moonlight_xbox_dxMain::OnKeyDown(unsigned short virtualKey, char modifiers)
{
	if (this == nullptr || moonlightClient == nullptr)return;
	moonlightClient->KeyDown(virtualKey, modifiers);
}


void moonlight_xbox_dxMain::OnKeyUp(unsigned short virtualKey, char modifiers)
{
	if (this == nullptr || moonlightClient == nullptr)return;
	moonlightClient->KeyUp(virtualKey, modifiers);
}

void moonlight_xbox_dxMain::SendGuideButton(int duration) {
	concurrency::create_async([duration,this]() {
		moonlightClient->SendGuide(0, true);
		Sleep(duration);
		moonlightClient->SendGuide(0, false);
	});
}

void moonlight_xbox_dxMain::SendWinAltB() {
	// Win-Alt-B = Toggle HDR
	concurrency::create_async([this]() {
		moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::LeftWindows, 0);
		moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::Menu, 0);
		moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::B, 0);
		Sleep(100);
		moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::B, 0);
		moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::Menu, 0);
		moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::LeftWindows, 0);
	});
}

bool moonlight_xbox_dxMain::ToggleLogs() {
	bool visible = m_LogRenderer->GetVisible();

	DISPATCH_UI([&] {
		m_LogRenderer->ToggleVisible();
	});

	return visible ? false : true;
}

bool moonlight_xbox_dxMain::ToggleStats() {
	bool visible = m_statsTextRenderer->GetVisible();

	DISPATCH_UI([&] {
		m_statsTextRenderer->ToggleVisible();
	});

	return visible ? false : true;
}

/// Gamepad Handling

GamepadState& moonlight_xbox_dxMain::FindGamepadState(uint32_t localId) {
	int i = 0;
	for (i = 0; i < MAX_GAMEPADS; i++) {
		if (m_GamepadState[i].controller != nullptr && m_GamepadState[i].localId == localId) {
			return m_GamepadState[i];
		}
    }

	static GamepadState nullState;
	return nullState;
}

GamepadState& moonlight_xbox_dxMain::FindGamepadStateByHostId(uint32_t hostId) {
	int i = 0;
	for (i = 0; i < MAX_GAMEPADS; i++) {
		if (m_GamepadState[i].controller != nullptr && m_GamepadState[i].hostId == hostId) {
			return m_GamepadState[i];
		}
    }

	static GamepadState nullState;
	return nullState;
}

GamepadState& moonlight_xbox_dxMain::FindGamepadStateByGamepad(Gamepad^ gamepad) {
	int i = 0;
	for (i = 0; i < MAX_GAMEPADS; i++) {
        if (m_GamepadState[i].controller == gamepad) {
            return m_GamepadState[i];
        }
    }

	static GamepadState nullState;
	return nullState;
}

void moonlight_xbox_dxMain::SendGamepadReadingForState(GamepadState& state, GamepadReading& reading) {
	// This method must NOT change the reading
	state.reading = reading;
	state.normalizeAxes();
	if (state.hasGamepadReadingChanged()) {
		int buttonFlags = 0;
		GamepadButtons buttons[] = {GamepadButtons::A, GamepadButtons::B, GamepadButtons::X, GamepadButtons::Y, GamepadButtons::DPadLeft, GamepadButtons::DPadRight, GamepadButtons::DPadUp, GamepadButtons::DPadDown, GamepadButtons::LeftShoulder, GamepadButtons::RightShoulder, GamepadButtons::Menu, GamepadButtons::View, GamepadButtons::LeftThumbstick, GamepadButtons::RightThumbstick};
		int LiButtonFlags[] = {A_FLAG, B_FLAG, X_FLAG, Y_FLAG, LEFT_FLAG, RIGHT_FLAG, UP_FLAG, DOWN_FLAG, LB_FLAG, RB_FLAG, PLAY_FLAG, BACK_FLAG, LS_CLK_FLAG, RS_CLK_FLAG};
		for (int i = 0; i < 14; i++) {
			if ((reading.Buttons & buttons[i]) == buttons[i]) {
				buttonFlags |= LiButtonFlags[i];
			}
		}

		LiSendMultiControllerEvent(
			state.hostId,
			MakeActiveMask(),
			buttonFlags,
			state.lTrig, state.rTrig,
			state.ltX, state.ltY,
			state.rtX, state.rtY);
		state.previousReading = reading;
	}
}
