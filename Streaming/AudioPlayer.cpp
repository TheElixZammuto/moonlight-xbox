#include "pch.h"
#include <opus/opus_multistream.h>
#include <Client\MoonlightClient.h>
#include <Streaming\AudioPlayer.h>
#include <Utils.hpp>
#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio.h"
namespace moonlight_xbox_dx {
	//Helpers
	AudioPlayer* instance;
	ma_pcm_rb rb;
	ma_device device;

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
			res = ma_pcm_rb_commit_read(&rb, len, buffer);
			if (res != MA_SUCCESS && res != MA_AT_END) {
				Utils::Log("Failed to read audio data to shared buffer\n");
				return;
			}
		}
	}

	int AudioPlayer::Init(int audioConfiguration, const POPUS_MULTISTREAM_CONFIGURATION opusConfig, void* context, int arFlags) {
		HRESULT hr;
		int rc;
		decoder = opus_multistream_decoder_create(opusConfig->sampleRate, opusConfig->channelCount, opusConfig->streams, opusConfig->coupledStreams, opusConfig->mapping, &rc);
		if (rc != 0) {
			return rc;
		}
		ma_result result;
		ma_device_config deviceConfig;
		deviceConfig = ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format = ma_format_s16;
		deviceConfig.playback.channels = opusConfig->channelCount;
		deviceConfig.sampleRate = opusConfig->sampleRate;
		this->samplePerFrame = opusConfig->samplesPerFrame;
		this->channelCount = opusConfig->channelCount;
		deviceConfig.dataCallback = requireAudioData;
		if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
			Utils::Log("Failed to open playback device.\n");
			return -3;
		}
		ma_result r = ma_pcm_rb_init(ma_format_s16, opusConfig->channelCount, 240 * 10, NULL, NULL, &rb);
		if (r != MA_SUCCESS) {
			Utils::Log("Failed to create shared buffer\n");
		}
		return r;
	}

	void AudioPlayer::Cleanup() {
		if (decoder != NULL) opus_multistream_decoder_destroy(decoder);
		ma_device_uninit(&device);
		ma_pcm_rb_uninit(&rb);
	}

	

	int AudioPlayer::SubmitDU(char* sampleData, int sampleLength) {
		int desiredSize = sizeof(short) * this->samplePerFrame * this->channelCount;
		HRESULT hr;
		int decodeLen = opus_multistream_decode(decoder,(unsigned char*)sampleData, sampleLength,pcmBuffer, 240, 0);
		if (decodeLen > 0) {
			void* buffer;
			ma_uint32 len = decodeLen;
			ma_result r = ma_pcm_rb_acquire_write(&rb, &len, &buffer);
			if (r != MA_SUCCESS) {
				Utils::Log("Failed to acquire shared buffer\n");
				return -1;
			}
			memcpy(buffer, pcmBuffer, len * ma_pcm_rb_get_bpf(&rb));
			r = ma_pcm_rb_commit_write(&rb, len, buffer);
			if (r != MA_SUCCESS) {
				Utils::Log("Failed to write to shared buffer\n");
				return -1;
			}
		}
		else {
			
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