// Copyright (c) Microsoft. All rights reserved.
// Adapted from the DeviceEnumerationAndPairing sample for notifications only.

// clang-format off
#include "pch.h"
// clang-format on
#include "DeviceWatcherHelper.h"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::Devices::Enumeration;

void DeviceWatcherHelper::StartWatcher(DeviceWatcher^ watcher)
{
	Reset();
	deviceWatcher = watcher;

	addedToken = watcher->Added += ref new TypedEventHandler<DeviceWatcher^, DeviceInformation^>(this, &DeviceWatcherHelper::Watcher_DeviceAdded);
	updatedToken = watcher->Updated += ref new TypedEventHandler<DeviceWatcher^, DeviceInformationUpdate^>(this, &DeviceWatcherHelper::Watcher_DeviceUpdated);
	removedToken = watcher->Removed += ref new TypedEventHandler<DeviceWatcher^, DeviceInformationUpdate^>(this, &DeviceWatcherHelper::Watcher_DeviceRemoved);
	enumerationCompletedToken = watcher->EnumerationCompleted += ref new TypedEventHandler<DeviceWatcher^, Object^>(this, &DeviceWatcherHelper::Watcher_EnumerationCompleted);
	stoppedToken = watcher->Stopped += ref new TypedEventHandler<DeviceWatcher^, Object^>(this, &DeviceWatcherHelper::Watcher_Stopped);

	try
	{
		watcher->Start();
	}
	catch (...)
	{
		Reset();
		throw;
	}
}

void DeviceWatcherHelper::StopWatcher()
{
	if (IsWatcherStarted(deviceWatcher))
	{
		deviceWatcher->Stop();
	}
}

void DeviceWatcherHelper::Reset()
{
	auto watcher = deviceWatcher;
	if (watcher == nullptr)
	{
		return;
	}

	// Invalidate queued callbacks before stopping and detach to break event cycles.
	deviceWatcher = nullptr;
	watcher->Added -= addedToken;
	watcher->Updated -= updatedToken;
	watcher->Removed -= removedToken;
	watcher->EnumerationCompleted -= enumerationCompletedToken;
	watcher->Stopped -= stoppedToken;
	if (IsWatcherStarted(watcher))
	{
		watcher->Stop();
	}
}

bool DeviceWatcherHelper::IsWatcherStarted(DeviceWatcher^ watcher)
{
	if (watcher == nullptr)
	{
		return false;
	}

	auto status = watcher->Status;
	return status == DeviceWatcherStatus::Started ||
		status == DeviceWatcherStatus::EnumerationCompleted;
}

bool DeviceWatcherHelper::IsWatcherRunning()
{
	return deviceWatcher != nullptr &&
		(IsWatcherStarted(deviceWatcher) || deviceWatcher->Status == DeviceWatcherStatus::Stopping);
}

void DeviceWatcherHelper::Watcher_DeviceAdded(DeviceWatcher^ sender, DeviceInformation^ info)
{
	DispatchWatcherEvent(sender, ref new DispatchedHandler([this, sender, info]()
		{
			DeviceAdded(sender, info);
		}));
}

void DeviceWatcherHelper::Watcher_DeviceUpdated(DeviceWatcher^ sender, DeviceInformationUpdate^ info)
{
	DispatchWatcherEvent(sender, ref new DispatchedHandler([this, sender, info]()
		{
			DeviceUpdated(sender, info);
		}));
}

void DeviceWatcherHelper::Watcher_DeviceRemoved(DeviceWatcher^ sender, DeviceInformationUpdate^ info)
{
	DispatchWatcherEvent(sender, ref new DispatchedHandler([this, sender, info]()
		{
			DeviceRemoved(sender, info);
		}));
}

void DeviceWatcherHelper::Watcher_EnumerationCompleted(DeviceWatcher^ sender, Object^ args)
{
	DispatchWatcherEvent(sender, ref new DispatchedHandler([this, sender, args]()
		{
			EnumerationCompleted(sender, args);
		}));
}

void DeviceWatcherHelper::Watcher_Stopped(DeviceWatcher^ sender, Object^ args)
{
	DispatchWatcherEvent(sender, ref new DispatchedHandler([this, sender, args]()
		{
			Stopped(sender, args);
		}));
}

void DeviceWatcherHelper::DispatchWatcherEvent(DeviceWatcher^ sender, DispatchedHandler^ callback)
{
	dispatcher->RunAsync(CoreDispatcherPriority::Low, ref new DispatchedHandler(
		[this, sender, callback]()
		{
			// Reset or a new StartWatcher may have run while this callback was queued.
			if (sender == deviceWatcher)
			{
				callback();
			}
		}));
}
