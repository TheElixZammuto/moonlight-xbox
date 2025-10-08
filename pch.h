#pragma once

// Use the C++ standard templated min/max
#define NOMINMAX

#include <wrl.h>
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <dxgi1_4.h>
#include <dxgi1_3.h>
#include <d3d11_4.h>
#include <d3d11_3.h>
#include <d2d1_3.h>
#include <d2d1effects_2.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <algorithm>
#include <memory>
#include <mutex>
#include <agile.h>
#include <concrt.h>
#include <collection.h>
#include "App.xaml.h"

#define IMGUI_USER_CONFIG "Common\imconfig.moonlight.h"
#include <imgui.h>
#include <imgui_impl_uwp.h>
#include <imgui_impl_dx11.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

// Helper for dispatching code to the UI thread
#define DISPATCH_UI(CAPTURE, CODE)                                           \
	Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow   \
		->Dispatcher                                                         \
		->RunAsync(                                                          \
			Windows::UI::Core::CoreDispatcherPriority::High,                 \
			ref new Windows::UI::Core::DispatchedHandler(CAPTURE CODE)       \
		)

// Time helpers
static inline int64_t QpcFreq() {
	static int64_t f = [] {
		LARGE_INTEGER li{};
		QueryPerformanceFrequency(&li);
		return li.QuadPart;
	}();
	return f;
}

static inline int64_t QpcNow() {
	LARGE_INTEGER li{};
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

static inline int64_t UsToQpc(int64_t us) {
	const int64_t f = QpcFreq();
	return (us / INT64_C(1000000)) * f +
	       (us % INT64_C(1000000)) * f / INT64_C(1000000);
}

static inline int64_t QpcToUs(int64_t qpc) {
	const int64_t f = QpcFreq();
	int64_t q = qpc / f;
	int64_t r = qpc % f;
	if (r < 0) {
		--q;
		r += f;
	}
	return q * INT64_C(1000000) + (r * INT64_C(1000000)) / f;
}

static inline double QpcToMsD(double qpc) {
	return qpc * 1000.0 / (double)QpcFreq();
}

static inline double QpcToMs(int64_t qpc) {
    return QpcToMsD(static_cast<double>(qpc));
}

static inline int64_t MsToQpc(double ms) {
	const int64_t us = static_cast<int64_t>(ms * 1000.0 + 0.5);
    return UsToQpc(us);
}

// Log something only once, safe to use in hot areas of the code
#define CONCAT(a, b)   CONCAT2(a, b)
#define CONCAT2(a, b)  a##b
#define LogOnce(fmt, ...)                                    \
    do {                                                     \
        static std::once_flag CONCAT(_onceFlag_, __LINE__);  \
        std::call_once(CONCAT(_onceFlag_, __LINE__), [&] {   \
            Utils::Logf(fmt, ##__VA_ARGS__);                 \
        });                                                  \
    } while (0)

// Frame queue debugging, uncomment FRAME_QUEUE_VERBOSE
// Note: When FQLog is enabled, the spam is intense, so it only logs data for a short time
#if !defined(NDEBUG)
//#define FRAME_QUEUE_VERBOSE
static int __once = 0;
#endif

#ifdef FRAME_QUEUE_VERBOSE
	#define FQLog(fmt, ...) \
        if (++__once > 200 && __once < 1000) \
		    moonlight_xbox_dx::Utils::Logf("[%d] " fmt, ::GetCurrentThreadId(), ##__VA_ARGS__)
#else
  	#if defined(_MSC_VER)
    	#define FQLog(...) __noop
  	#else
		#define FQLog(fmt, ...) do {} while(0)
	#endif
#endif
