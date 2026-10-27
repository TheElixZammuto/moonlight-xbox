// pch.h must come first: with /Yu, MSVC discards everything above it.
#include "pch.h"
#include "TracyBootstrap.h"


#include <atomic>
#include <chrono>
#include <mutex>
#include <stdlib.h>
#include <string.h>
#include <thread>

using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::System::Profile;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Media;

namespace moonlight_xbox_dx {
	namespace TracyBootstrap {

#ifdef TRACY_ENABLE

		// >>> Set this to the IP address of the machine running `third_party/tracy-xbox/extra/relay/tracy-relay.py`
        // and the Tracy server.
		static const char *const TracyServerAddress = "192.168.2.16";

		namespace {
			bool initialized = false;

			// Everything the Tracy client cannot look up for itself once it is running inside an
			// app container: it has no access to RtlGetVersion, the process environment block, or
			// the executable path, so the host info it reports is nearly empty on Xbox.
			void ReportAppInfo() {
				auto version = AnalyticsInfo::VersionInfo;
				const unsigned long long raw = _wcstoui64(version->DeviceFamilyVersion->Data(), nullptr, 10);

				PackageVersion packageVersion = Package::Current->Id->Version;

				char info[512];
				const int length = snprintf(info, sizeof(info),
				                            "%S on %S %llu.%llu.%llu.%llu (package %u.%u.%u.%u)",
				                            Package::Current->DisplayName->Data(),
				                            version->DeviceFamily->Data(),
				                            (raw >> 48) & 0xFFFF,
				                            (raw >> 32) & 0xFFFF,
				                            (raw >> 16) & 0xFFFF,
				                            raw & 0xFFFF,
				                            (unsigned)packageVersion.Major, (unsigned)packageVersion.Minor,
				                            (unsigned)packageVersion.Build, (unsigned)packageVersion.Revision);

				if (length > 0) {
					// snprintf reports the untruncated length.
					const int clamped = length < (int)sizeof(info) ? length : (int)sizeof(info) - 1;
					TracyAppInfo(info, (size_t)clamped);
				}
			}
		} // namespace

		void Initialize() {
			if (initialized) return;
			initialized = true;

			tracy::SetConnectAddress(TracyServerAddress);

			ReportAppInfo();
		}

#else

		void Initialize() {}

#endif

	} // namespace Profiling
} // namespace moonlight_xbox_dx
