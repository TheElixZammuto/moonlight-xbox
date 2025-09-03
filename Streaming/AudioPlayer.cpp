#include "pch.h"
#include <opus/opus_multistream.h>
#include <State\MoonlightClient.h>
#include <Streaming\AudioPlayer.h>
#include <Utils.hpp>
#if defined(_DEBUG)
#define MA_DEBUG_OUTPUT
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio.h"

static void AudioPlayer_LogCallback(void* pUserData, ma_uint32 level, const char* pMessage)
{
	if (level <= MA_LOG_LEVEL_INFO) {
		moonlight_xbox_dx::Utils::Logf("[miniaudio] %s", pMessage);
	}
}

namespace moonlight_xbox_dx {
	//Helpers
	AudioPlayer* instance;
	ma_pcm_rb rb;
	ma_device device;
	ma_context context;
	ma_log log;

	int audioInitCallback(int audioConfiguration, const POPUS_MULTISTREAM_CONFIGURATION opusConfig, void* context, int arFlags) {
		return instance->Init(audioConfiguration,opusConfig, context, arFlags);
	}
	void audioStartCallback() {
		instance->Start();
	}
	void audioStopCallback() {
		instance->Stop();
	}
	void audioCleanupCallback() {
		instance->Cleanup();
		instance = NULL;
	}
	void audioSubmitCallback(char* sampleData, int sampleLength) {
		instance->SubmitDU(sampleData,sampleLength);
	}

	AudioPlayer* AudioPlayer::getInstance() {
		if (instance == NULL)instance = new AudioPlayer();
		return instance;
	}

	AUDIO_RENDERER_CALLBACKS AudioPlayer::getDecoder() {
		instance = AudioPlayer::getInstance();
		AUDIO_RENDERER_CALLBACKS decoder_callbacks_sdl;
		LiInitializeAudioCallbacks(&decoder_callbacks_sdl);
		decoder_callbacks_sdl.init = audioInitCallback;
		decoder_callbacks_sdl.start = audioStartCallback;
		decoder_callbacks_sdl.stop = audioStopCallback;
		decoder_callbacks_sdl.cleanup = audioCleanupCallback;
		decoder_callbacks_sdl.decodeAndPlaySample = audioSubmitCallback;
		decoder_callbacks_sdl.capabilities = CAPABILITY_DIRECT_SUBMIT;
		return decoder_callbacks_sdl;
	}

	void requireAudioData(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
	{
		void* buffer;
		ma_uint32 len = frameCount;
		ma_result res = ma_pcm_rb_acquire_read(&rb, &len, &buffer);
		if (res != MA_SUCCESS) {
			Utils::Log("Failed to read audio data\n");
			return;
		}
		if (len > 0) {
			memcpy(pOutput, buffer, len * ma_pcm_rb_get_bpf(&rb));
			res = ma_pcm_rb_commit_read(&rb, len);
			if (res != MA_SUCCESS && res != MA_AT_END) {
				Utils::Log("Failed to read audio data to shared buffer\n");
				return;
			}
		}
	}

	int AudioPlayer::Init(int audioConfiguration, const POPUS_MULTISTREAM_CONFIGURATION opusConfig, void* mnlContext, int arFlags) {
		int rc;
		decoder = opus_multistream_decoder_create(opusConfig->sampleRate, opusConfig->channelCount, opusConfig->streams, opusConfig->coupledStreams, opusConfig->mapping, &rc);
		if (rc != 0) {
			return rc;
		}
		ma_device_config deviceConfig;
		deviceConfig = ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format = ma_format_f32;
		deviceConfig.playback.channels = opusConfig->channelCount;
		deviceConfig.sampleRate = opusConfig->sampleRate;
		this->samplePerFrame = opusConfig->samplesPerFrame;
		this->channelCount = opusConfig->channelCount;
		deviceConfig.dataCallback = requireAudioData;

		// Specify a custom log object in the config so any logs that are posted from ma_context_init() are captured.
		ma_log_init(NULL, &log);
		ma_log_register_callback(&log, ma_log_callback_init(&AudioPlayer_LogCallback, NULL));
		ma_context_config config = ma_context_config_init();
		config.pLog = &log;

		if (ma_context_init(NULL, 1, &config, &context) != MA_SUCCESS) {
			Utils::Log("Failed to create miniaudio context.\n");
			return -3;
		}

		if (ma_device_init(&context, &deviceConfig, &device) != MA_SUCCESS) {
			Utils::Log("Failed to open playback device.\n");
			return -3;
		}

		ma_result r = ma_pcm_rb_init(ma_format_f32, opusConfig->channelCount, opusConfig->samplesPerFrame * 10, NULL, NULL, &rb);
		if (r != MA_SUCCESS) {
			Utils::Log("Failed to create shared buffer\n");
		}
		return r;
	}

	void AudioPlayer::Cleanup() {
		Utils::Log("Audio Cleanup\n");
		if (decoder != NULL) opus_multistream_decoder_destroy(decoder);
		ma_pcm_rb_uninit(&rb);
		ma_device_uninit(&device);
		ma_log_uninit(&log);
		ma_context_uninit(&context);
	}

	int AudioPlayer::SubmitDU(char* sampleData, int sampleLength) {
		void* buffer;
		ma_uint32 bufferLen = (ma_uint32)this->samplePerFrame;
		ma_result r = ma_pcm_rb_acquire_write(&rb, &bufferLen, &buffer);
		if (r != MA_SUCCESS) {
			Utils::Log("Failed to acquire shared buffer\n");
			return -1;
		}
		if (bufferLen < (ma_uint32)this->samplePerFrame || buffer == nullptr) {
			Utils::Logf("Audio buffer overflow (%d > %d)\n", bufferLen, this->samplePerFrame);
			return -1;
		}

		int decodeLen = opus_multistream_decode_float(decoder, (unsigned char*)sampleData,
													  sampleLength, (float *)buffer, this->samplePerFrame, 0);
		if (decodeLen < 0) {
			Utils::Logf("opus_multistream_decode_float failed: %d\n", decodeLen);
			return -1;
		}
		if (decodeLen > 0) {
			r = ma_pcm_rb_commit_write(&rb, (ma_uint32)decodeLen);
			if (r != MA_SUCCESS && r != MA_AT_END) {
				Utils::Log("Failed to write to shared buffer\n");
				return -1;
			}
		}
		return 0;
	}

	void AudioPlayer::Start() {
		if (ma_device_start(&device) != MA_SUCCESS) {
			Utils::Log("Failed to start playback device.\n");
			ma_device_uninit(&device);
		}

	}

	void AudioPlayer::Stop() {
		if (ma_device_stop(&device) != MA_SUCCESS) {
			Utils::Log("Failed to start playback device.\n");
			ma_device_uninit(&device);
		}
	}
}
