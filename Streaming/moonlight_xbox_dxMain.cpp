#include "pch.h"
#include "moonlight_xbox_dxMain.h"
#include "Common\DirectXHelper.h"
#include "../Plot/ImGuiPlots.h"
#include "Utils.hpp"
#include <Pages/StreamPage.xaml.h>
#include <Streaming\FFMpegDecoder.h>
using namespace Windows::Gaming::Input;


using namespace moonlight_xbox_dx;
using namespace DirectX;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;
using namespace Windows::UI::ViewManagement::Core;

extern "C" {
#include<Limelight.h>
}

// Loads and initializes application assets when the application is loaded.
moonlight_xbox_dxMain::moonlight_xbox_dxMain(const std::shared_ptr<DX::DeviceResources>& deviceResources, StreamPage^ streamPage, MoonlightClient* client, StreamConfiguration^ configuration) :

	m_deviceResources(deviceResources), m_pointerLocationX(0.0f), m_streamPage(streamPage), moonlightClient(client)
{
	// Register to be notified if the Device is lost or recreated
	m_deviceResources->RegisterDeviceNotify(this);

	// Setup stats object. DeviceResources keeps a reference so that various components such as FFMpegDecoder can get to it
	m_stats = std::make_shared<Stats>();
	m_deviceResources->SetStats(m_stats);

	m_sceneRenderer = std::make_shared<VideoRenderer>(m_deviceResources, moonlightClient, configuration);

	m_LogRenderer = std::make_unique<LogRenderer>(m_deviceResources);

	m_statsTextRenderer = std::make_unique<StatsRenderer>(m_deviceResources, m_stats);
	m_statsTextRenderer->SetVisible(configuration->enableStats);

	// Force disable graphs on Xbox One at 4K, they run too slowly
	// XXX can we run them at a lower resolution?
	if (IsXboxOne() && m_deviceResources->GetPixelHeight() >= 2160) {
		configuration->enableGraphs = false;
	}

	m_deviceResources->SetShowImGui(configuration->enableGraphs);
	ImGuiPlots::instance().setEnabled(configuration->enableGraphs);

	streamPage->m_progressView->Visibility = Windows::UI::Xaml::Visibility::Visible;

	client->OnStatusUpdate = ([streamPage](int status) {
		const char* msg = LiGetStageName(status);
		streamPage->m_statusText->Text = Utils::StringFromStdString(std::string(msg));
		});

	client->OnCompleted = ([streamPage]() {
		streamPage->m_progressView->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
		});

	client->OnFailed = ([streamPage](int status, int error, char* message) {
		streamPage->m_statusText->Text = Utils::StringFromStdString(std::string(message));
		});

	client->SetHDR = ([this](bool v) {
		this->m_sceneRenderer->SetHDR(v);
	});

	m_timer.SetFixedTimeStep(false);

	double refreshRate = m_deviceResources->GetUWPRefreshRate();
	m_deviceResources->SetRefreshRate(refreshRate);
	m_deviceResources->SetFrameRate(configuration->FPS);
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
	if (m_renderLoopWorker != nullptr && m_renderLoopWorker->Status == AsyncStatus::Started)
	{
		return;
	}

	// Create a task that will be run on a background thread.
	auto workItemHandler = ref new WorkItemHandler([this](IAsyncAction ^ action) {
		if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL)) {
			Utils::Logf("Failed to set render thread priority: %d\n", GetLastError());
		}

		int64_t t0 = 0, t1 = 0, t2 = 0, t3 = 0;
		int64_t lastFramePts = 0, lastPresentTime = 0;
		double frametimeMs = 0.0;
		const double bufferMs  = 1.5;  // safety wait time to avoid missing deadline
		const double alphaUp   = 0.25; // react faster when renderMs spikes upward
		const double alphaDown = 0.05; // decay slowly when renderMs drops
		double ewmaRenderMs = 3.0; // Initial guess for render cost

		// Calculate the updated frame and render once per vertical blanking interval.
		while (action->Status == AsyncStatus::Started && !moonlightClient->IsConnectionTerminated()) {
			// Get overall deadline we must hit by the Present for this frame
			int64_t deadline = Pacer::instance().getNextVBlankQpc(&t0);

			// wait for a frame + avg render time + safety buffer
			double maxWaitMs = std::max(0.0, QpcToMs(deadline - t0) - ewmaRenderMs - bufferMs);
			Pacer::instance().waitForFrame(maxWaitMs);

			{
				critical_section::scoped_lock lock(m_criticalSection);

				t1 = QpcNow();
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
				Pacer::instance().waitBeforePresent(deadline);
				t3 = QpcNow();

				if (!rendered) {
					// we're receiving a lower framerate so no frame was available,
					// we don't call Present here and the previous frame is re-displayed
					continue;
				}

				{
					// lock is required around Present
					auto guard = FFMpegDecoder::Lock();
					m_deviceResources->Present();
				}

				// Graph frametime only for new frames
				int64_t currentFramePts = Pacer::instance().getCurrentFramePts();
				if (currentFramePts != lastFramePts) {
					if (lastPresentTime > 0) {
						frametimeMs = QpcToMs(t3 - lastPresentTime);
						ImGuiPlots::instance().observeFloat(PLOT_FRAMETIME, static_cast<float>(frametimeMs));
					}
					lastPresentTime = t3;
					lastFramePts = currentFramePts;
				}

				// Weighted avg of time spent in Render(), more weight given to a slower render time
				double renderMs = std::clamp(QpcToMs(t2 - t1), 0.0, QpcToMs(deadline - t0));
				double alpha = (renderMs > ewmaRenderMs) ? alphaUp : alphaDown;
				ewmaRenderMs = (renderMs * alpha) + (ewmaRenderMs * (1.0 - alpha));

				// Track high-level render loop stats
				m_deviceResources->GetStats()->SubmitRenderStats(
				    QpcToUs(t1 - t0),
				    QpcToUs(t2 - t1),
				    QpcToUs(t3 - t2));

				FQLog("render loop %.3fms frametime %.3fms (PreWait %.3fms + Render %.3fms (avg %.3f) + Present %.3fms)\n",
				      QpcToMs(t3 - t0),
				      frametimeMs,
				      QpcToMs(t1 - t0),
				      renderMs,
				      ewmaRenderMs,
				      QpcToMs(t3 - t2));
			}
		}

		// we've lost the connection, clean up
		StopRenderLoop(); // also stops input
		Disconnect();

		// Navigate back to home
		DISPATCH_UI([],{
			auto rootFrame = dynamic_cast<Windows::UI::Xaml::Controls::Frame^>(Windows::UI::Xaml::Window::Current->Content);
			if (rootFrame) {
				rootFrame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSelectorPage::typeid));
			}
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
				}
				else {
					const int64_t nextPoll = lastProcessInput + pollIntervalQpc;
					SleepUntilQpc(nextPoll, 500);
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

inline bool isPressed(Windows::Gaming::Input::GamepadButtons b, Windows::Gaming::Input::GamepadButtons x) {
	return (b & x) == x;
}

// Process all input from the user before updating game state
void moonlight_xbox_dxMain::ProcessInput()
{
	auto gamepads = Windows::Gaming::Input::Gamepad::Gamepads;
	if (gamepads->Size == 0)return;
	moonlightClient->SetGamepadCount(gamepads->Size);
	auto state = GetApplicationState();
	//Position
	double multiplier = ((double)state->MouseSensitivity) / ((double)4.0f);
	for (UINT i = 0; i < gamepads->Size; i++) {
		Windows::Gaming::Input::Gamepad^ gamepad = gamepads->GetAt(i);
		auto reading = gamepad->GetCurrentReading();
		//If this combination is pressed on gamed we should handle some magic things :)
		bool alternateCombination = GetApplicationState()->AlternateCombination;
		bool isCurrentlyPressed = true;
		GamepadButtons magicKey[] = { GamepadButtons::Menu,GamepadButtons::View };
		if (alternateCombination) {
			magicKey[0] = GamepadButtons::LeftShoulder;
			magicKey[1] = GamepadButtons::RightShoulder;
			if (reading.LeftTrigger < 0.25 || reading.RightTrigger < 0.25)isCurrentlyPressed = false;
		}
		for (auto k : magicKey) {
			if ((reading.Buttons & k) != k) {
				isCurrentlyPressed = false;
				break;
			}
		}
		if (isCurrentlyPressed) {
			DISPATCH_UI([this], {
				Windows::UI::Xaml::Controls::Flyout::ShowAttachedFlyout(m_streamPage->m_flyoutButton);
			});

			// send an empty controller packet, otherwise Sunshine may see View being kept held down,
			// triggering the "Home/Guide Button Emulation Timeout" to send a Guide button press after a few seconds.
			static Windows::Gaming::Input::GamepadReading emptyReading{};
			ZeroMemory(&emptyReading, sizeof(GamepadReading));
			moonlightClient->SendGamepadReading(i, emptyReading);

			// disable all input until the flyout is closed
			insideFlyout = true;
		}
		if (insideFlyout) {
			return;
		}

		//If mouse mode is enabled the gamepad acts as a mouse, instead we pass the raw events to the host
		if (keyboardMode) {
			//B to close
			if (isPressed(reading.Buttons, GamepadButtons::B) && !isPressed(previousReading[i].Buttons, GamepadButtons::B)) {
				if (GetApplicationState()->EnableKeyboard) {
					m_streamPage->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this]() {
						m_streamPage->m_keyboardView->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
						}));
					keyboardMode = false;
				}
				else {
					CoreInputView::GetForCurrentView()->TryHide();
				}
			}
			//X to backspace
			if (isPressed(reading.Buttons, GamepadButtons::X) && !isPressed(previousReading[i].Buttons, GamepadButtons::X)) {
				moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::Back, 0);
			}
			else if (isPressed(previousReading[i].Buttons, GamepadButtons::X)) {
				moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::Back, 0);
			}
			//Y to Space
			if (isPressed(reading.Buttons, GamepadButtons::Y) && !isPressed(previousReading[i].Buttons, GamepadButtons::Y)) {
				moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::Space, 0);
			}
			else if (isPressed(previousReading[i].Buttons, GamepadButtons::Y)) {
				moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::Space, 0);
			}
			//LB to Left
			if (isPressed(reading.Buttons, GamepadButtons::LeftShoulder) && !isPressed(previousReading[i].Buttons, GamepadButtons::LeftShoulder)) {
				moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::Left, 0);
			}
			else if (isPressed(previousReading[i].Buttons, GamepadButtons::LeftShoulder)) {
				moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::Left, 0);
			}
			//RB to Right
			if (isPressed(reading.Buttons, GamepadButtons::RightShoulder) && !isPressed(previousReading[i].Buttons, GamepadButtons::RightShoulder)) {
				moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::Right, 0);
			}
			else if (isPressed(previousReading[i].Buttons, GamepadButtons::RightShoulder)) {
				moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::Right, 0);
			}
			//Start to Enter
			if (isPressed(reading.Buttons, GamepadButtons::Menu) && !isPressed(previousReading[i].Buttons, GamepadButtons::Menu)) {
				moonlightClient->KeyDown((unsigned short)Windows::System::VirtualKey::Enter, 0);
			}
			else if (isPressed(previousReading[i].Buttons, GamepadButtons::Menu)) {
				moonlightClient->KeyUp((unsigned short)Windows::System::VirtualKey::Right, 0);
			}
			//Move with right stick
			if (isPressed(reading.Buttons, GamepadButtons::LeftThumbstick)) {
				moonlightClient->SendScroll(pow(reading.RightThumbstickY * multiplier * 2, 3));
				moonlightClient->SendScrollH(pow(reading.RightThumbstickX * multiplier * 2, 3));
			}
			else {
				//Move with right stick instead of the left one in KB mode
				double x = reading.RightThumbstickX;
				if (abs(x) < 0.1) x = 0;
				else x = x + (x > 0 ? 1 : -1); //Add 1 to make sure < 0 values do not make everything broken
				double y = reading.RightThumbstickY;
				if (abs(y) < 0.1) y = 0;
				else y = (y * -1) + (y > 0 ? -1 : 1); //Add 1 to make sure < 0 values do not make everything broken
				moonlightClient->SendMousePosition(pow(x * multiplier, 3), pow(y * multiplier, 3));
			}
			if (reading.LeftTrigger > 0.25 && previousReading[i].LeftTrigger < 0.25) {
				moonlightClient->SendMousePressed(BUTTON_LEFT);
			}
			else if (reading.LeftTrigger < 0.25 && previousReading[i].LeftTrigger > 0.25) {
				moonlightClient->SendMouseReleased(BUTTON_LEFT);
			}
			if (reading.RightTrigger > 0.25 && previousReading[i].RightTrigger < 0.25) {
				moonlightClient->SendMousePressed(BUTTON_RIGHT);
			}
			else if (reading.RightTrigger < 0.25 && previousReading[i].RightTrigger > 0.25) {
				moonlightClient->SendMouseReleased(BUTTON_RIGHT);
			}
		}
		else if (mouseMode) {
			auto state = GetApplicationState();
			//Position
			double multiplier = ((double)state->MouseSensitivity) / ((double)4.0f);
			double x = reading.LeftThumbstickX;
			if (abs(x) < 0.1) x = 0;
			else x = x + (x > 0 ? 1 : -1); //Add 1 to make sure < 0 values do not make everything broken
			double y = reading.LeftThumbstickY;
			if (abs(y) < 0.1) y = 0;
			else y = (y * -1) + (y > 0 ? -1 : 1); //Add 1 to make sure < 0 values do not make everything broken
			moonlightClient->SendMousePosition(pow(x * multiplier, 3), pow(y * multiplier, 3));
			//Left Click
			if (isPressed(reading.Buttons, GamepadButtons::A) && !isPressed(previousReading[i].Buttons, GamepadButtons::A)) {
				moonlightClient->SendMousePressed(BUTTON_LEFT);
			}
			else if (isPressed(previousReading[i].Buttons, GamepadButtons::A)) {
				moonlightClient->SendMouseReleased(BUTTON_LEFT);
			}
			//Right Click
			if (isPressed(reading.Buttons, GamepadButtons::X) && !isPressed(previousReading[i].Buttons, GamepadButtons::X)) {
				moonlightClient->SendMousePressed(BUTTON_RIGHT);
			}
			else if (isPressed(previousReading[i].Buttons, GamepadButtons::X)) {
				moonlightClient->SendMouseReleased(BUTTON_RIGHT);
			}
			//Left Trigger Click
			if (reading.LeftTrigger > 0.25 && previousReading[i].LeftTrigger < 0.25) {
				moonlightClient->SendMousePressed(BUTTON_LEFT);
			}
			else if (reading.LeftTrigger < 0.25 && previousReading[i].LeftTrigger > 0.25) {
				moonlightClient->SendMouseReleased(BUTTON_LEFT);
			}
			//Right Trigger Click
			if (reading.RightTrigger > 0.25 && previousReading[i].RightTrigger < 0.25) {
				moonlightClient->SendMousePressed(BUTTON_RIGHT);
			}
			else if (reading.RightTrigger < 0.25 && previousReading[i].RightTrigger > 0.25) {
				moonlightClient->SendMouseReleased(BUTTON_RIGHT);
			}
			//Keyboard
			if (!isPressed(reading.Buttons, GamepadButtons::Y) && isPressed(previousReading[i].Buttons, GamepadButtons::Y)) {
				if (GetApplicationState()->EnableKeyboard) {
					m_streamPage->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this]() {
						m_streamPage->m_keyboardView->Visibility = Windows::UI::Xaml::Visibility::Visible;
						}));
					keyboardMode = true;
				}
				else {
					CoreInputView::GetForCurrentView()->TryShow(CoreInputViewKind::Keyboard);
				}
			}
			//Scroll
			moonlightClient->SendScroll(pow(reading.RightThumbstickY * multiplier * 2, 3));
			moonlightClient->SendScrollH(pow(reading.RightThumbstickX * multiplier * 2, 3));
			//Xbox/Guide Button
			//Right Click
			if (isPressed(reading.Buttons, GamepadButtons::B) && !isPressed(previousReading[i].Buttons, GamepadButtons::B)) {
				moonlightClient->SendGuide(i, true);
			}
			else if (isPressed(previousReading[i].Buttons, GamepadButtons::B)) {
				moonlightClient->SendGuide(i, false);
			}
		}
		else {
			moonlightClient->SendGamepadReading(i, reading);
		}
		previousReading[i] = reading;
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

	DISPATCH_UI([&], {
		m_LogRenderer->ToggleVisible();
	});

	return visible ? false : true;
}

bool moonlight_xbox_dxMain::ToggleStats() {
	bool visible = m_statsTextRenderer->GetVisible();

	DISPATCH_UI([&], {
		m_statsTextRenderer->ToggleVisible();
	});

	return visible ? false : true;
}

void moonlight_xbox_dx::usleep(unsigned int usec) {
	HANDLE timer;
	LARGE_INTEGER ft;

	ft.QuadPart = -(10 * (__int64)usec);

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	if (timer == 0) return;
	SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}
