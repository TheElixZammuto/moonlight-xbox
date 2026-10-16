// Copyright (c) Microsoft. All rights reserved.
// Adapted from the DeviceEnumerationAndPairing sample for notifications only.

#pragma once

namespace moonlight_xbox_dx
{
	// Call StartWatcher, StopWatcher, and Reset on the dispatcher thread.
	public ref class DeviceWatcherHelper sealed
	{
	public:
		DeviceWatcherHelper(Windows::UI::Core::CoreDispatcher^ dispatcher) :
			dispatcher(dispatcher)
		{
		}

		// Forward the original watcher payloads on the dispatcher thread.
		event Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher^, Windows::Devices::Enumeration::DeviceInformation^>^ DeviceAdded;
		event Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher^, Windows::Devices::Enumeration::DeviceInformationUpdate^>^ DeviceUpdated;
		event Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher^, Windows::Devices::Enumeration::DeviceInformationUpdate^>^ DeviceRemoved;
		event Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher^, Platform::Object^>^ EnumerationCompleted;
		event Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher^, Platform::Object^>^ Stopped;

		void StartWatcher(Windows::Devices::Enumeration::DeviceWatcher^ watcher);
		void StopWatcher();
		void Reset();
		bool IsWatcherRunning();

	private:
		Windows::Devices::Enumeration::DeviceWatcher^ deviceWatcher;
		Windows::UI::Core::CoreDispatcher^ dispatcher;
		Windows::Foundation::EventRegistrationToken addedToken;
		Windows::Foundation::EventRegistrationToken updatedToken;
		Windows::Foundation::EventRegistrationToken removedToken;
		Windows::Foundation::EventRegistrationToken enumerationCompletedToken;
		Windows::Foundation::EventRegistrationToken stoppedToken;

		static bool IsWatcherStarted(Windows::Devices::Enumeration::DeviceWatcher^ watcher);
		void Watcher_DeviceAdded(Windows::Devices::Enumeration::DeviceWatcher^ sender, Windows::Devices::Enumeration::DeviceInformation^ info);
		void Watcher_DeviceUpdated(Windows::Devices::Enumeration::DeviceWatcher^ sender, Windows::Devices::Enumeration::DeviceInformationUpdate^ info);
		void Watcher_DeviceRemoved(Windows::Devices::Enumeration::DeviceWatcher^ sender, Windows::Devices::Enumeration::DeviceInformationUpdate^ info);
		void Watcher_EnumerationCompleted(Windows::Devices::Enumeration::DeviceWatcher^ sender, Platform::Object^ args);
		void Watcher_Stopped(Windows::Devices::Enumeration::DeviceWatcher^ sender, Platform::Object^ args);
		void DispatchWatcherEvent(Windows::Devices::Enumeration::DeviceWatcher^ sender, Windows::UI::Core::DispatchedHandler^ callback);
	};
}
