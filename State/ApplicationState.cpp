#include "pch.h"
#include "ApplicationState.h"

Concurrency::task<void> moonlight_xbox_dx::ApplicationState::Init()
{
	auto that = this;
	return Concurrency::create_task([that]() {
		for (auto h : that->hosts) {
			h->UpdateStats();
		}
	});
}
