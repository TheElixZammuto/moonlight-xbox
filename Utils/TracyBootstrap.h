#pragma once

namespace moonlight_xbox_dx {
	namespace TracyBootstrap {
		// Reports the things the Tracy client cannot discover from inside an app container
		// (OS build, device family, package version) and sets relay IP address.
		void Initialize();
	} // namespace Profiling
} // namespace moonlight_xbox_dx
