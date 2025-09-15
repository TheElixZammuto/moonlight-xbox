#pragma once

// Use the C++ standard templated min/max
#define NOMINMAX

#include <wrl.h>
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <dxgi1_4.h>
#include <dxgi1_3.h>
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
static uint64_t QpcToUs(uint64_t qpc) {
	static LARGE_INTEGER qpcFreq = {0, 0};
	if (qpcFreq.QuadPart == 0) {
		QueryPerformanceFrequency(&qpcFreq);
	}

	return (qpc / qpcFreq.QuadPart) * UINT64_C(1000000) +
	       (qpc % qpcFreq.QuadPart) * UINT64_C(1000000) / qpcFreq.QuadPart;
}

static uint64_t QpcToNs(uint64_t qpc) {
    // Convert QPC units (1/perf_freq seconds) to nanoseconds. This will work
    // without overflow because the QPC value is guaranteed not to roll-over
    // within 100 years, so perf_freq must be less than 2.9*10^9.
	static LARGE_INTEGER qpcFreq = {0, 0};
	if (qpcFreq.QuadPart == 0) {
		QueryPerformanceFrequency(&qpcFreq);
	}

    return (qpc / qpcFreq.QuadPart) * UINT64_C(1000000000) +
           (qpc % qpcFreq.QuadPart) * UINT64_C(1000000000) / qpcFreq.QuadPart;
}

static uint64_t QpcNsNow() {
    LARGE_INTEGER perf_count;
    QueryPerformanceCounter(&perf_count);
    return QpcToNs(perf_count.QuadPart);
}

static double QpcToMs(uint64_t qpc) {
	return (double)QpcToUs(qpc) / 1000.0f;
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

// Frame queue debugging
#if !defined(NDEBUG)
#define FRAME_QUEUE_VERBOSE
#endif

#ifdef FRAME_QUEUE_VERBOSE
	#define FQLog(fmt, ...) \
		moonlight_xbox_dx::Utils::Logf("[%d] " fmt, ::GetCurrentThreadId(), ##__VA_ARGS__)
#else
  	#if defined(_MSC_VER)
    	#define FQLog(...) __noop
  	#else
		#define FQLog(fmt, ...) do {} while(0)
	#endif
#endif
