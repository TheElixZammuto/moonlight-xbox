#pragma once
#include "pch.h"
#include <atomic>
#include <cstdint>
extern "C" {
#include <Limelight.h>
}
#include "third_party/miniaudio.h"

#define MAX_CHANNEL_COUNT 8
#define MAX_SAMPLES_PER_FRAME (48000 / 1000 * 120)

// Min/Max buffer sizes the user can choose
#define DEFAULT_BUFFER_MS 30
#define MIN_BUFFER_MS 10
#define MAX_BUFFER_MS 50

// 2x MAX_BUFFER_MS guarantees the user's setting is safe for both
// 5ms (surround) and 10ms (stereo) packet durations.
#define RB_CAPACITY_MS (MAX_BUFFER_MS * 2)

namespace moonlight_xbox_dx
{
	enum class AudioSubmitResult {
		Queued,
		DroppedLatency,  // dropped to reduce latency
		DroppedOverflow, // ring buffer couldn't take the whole packet
		Error,           // miniaudio ring buffer API failure
	};

	// PCM audio renderer built on miniaudio. Opus decoding lives in the
	// moonlight-common-c callback glue in AudioPlayer.cpp; this class only
	// manages the playback device and the ring buffer feeding it.
	class AudioPlayer {
	public:
		static AudioPlayer &instance();
		static AUDIO_RENDERER_CALLBACKS getDecoder();

		bool prepareForPlayback(const POPUS_MULTISTREAM_CONFIGURATION opusConfig);
		void start();
		void stop();
		void cleanup();

		// Returns the scratch buffer decoded PCM should be written into,
		// with *size set to its capacity in bytes.
		void *getAudioBuffer(int *size);

		// Queues the first bytesWritten bytes of the scratch buffer for
		// playback. Packets are queued whole or not at all.
		AudioSubmitResult submitAudio(int bytesWritten);

		int GetAudioBufferMs();
		int SetAudioBufferMs(int ms);

		// Thread-safe accessors for the stats overlay / plots.
		void AddGlitchCount() { glitchCount.fetch_add(1, std::memory_order_relaxed); }
		uint32_t GetGlitchCount() const { return glitchCount.load(std::memory_order_relaxed); }
		uint32_t GetLatencyDropCount() const { return latencyDropCount.load(std::memory_order_relaxed); }

	private:
		AudioPlayer() = default;
		AudioPlayer(const AudioPlayer &) = delete;
		AudioPlayer &operator=(const AudioPlayer &) = delete;

		// miniaudio device data callback; pUserData is the AudioPlayer.
		// Runs on the WASAPI thread: no logging, allocation, or locks.
		static void deviceDataCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount);

		// Tears down device, ring buffer, context and log (but not the Opus
		// decoder, which is owned by the moonlight-common-c init/cleanup pair).
		void releaseDevice();

		ma_pcm_rb rb{};
		ma_device device{};
		ma_context context{};
		ma_log log{};

		// Track what actually got initialized so cleanup() / error unwinding
		// only tears down what exists; prepareForPlayback releases anything a
		// previous session left behind before re-initializing.
		bool rbInitialized = false;
		bool deviceInitialized = false;
		bool contextInitialized = false;
		bool logInitialized = false;

		float decodeBuffer[MAX_CHANNEL_COUNT * MAX_SAMPLES_PER_FRAME];
		int channelCount = 0;
		int samplesPerFrame = 0;
		ma_uint32 sampleRate = 0;
		ma_uint32 framesPerMs = 0;
		ma_uint32 rbCapacityFrames = 0;
		std::atomic<int> bufferSizeMs{DEFAULT_BUFFER_MS};

		std::atomic<uint32_t> glitchCount{0};
		std::atomic<uint32_t> latencyDropCount{0};
		std::atomic<uint32_t> callbackErrorCount{0};
	};
}
