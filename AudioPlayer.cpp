#include "pch.h"
#include <AudioPlayer.h>
#include <opus/opus_multistream.h>
#include <xaudio2.h>
namespace moonlight_xbox_dx {
	//Helpers
	AudioPlayer* instance;

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

	int AudioPlayer::Init(int audioConfiguration, const POPUS_MULTISTREAM_CONFIGURATION opusConfig, void* context, int arFlags) {
		HRESULT hr;
		int rc;
		decoder = opus_multistream_decoder_create(opusConfig->sampleRate, opusConfig->channelCount, opusConfig->streams, opusConfig->coupledStreams, opusConfig->mapping, &rc);

		channelCount = opusConfig->channelCount;
		//Initialize XAudio2
		if (FAILED(hr = XAudio2Create(xAudio.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR))) {
			return hr;
		}
		if (FAILED(hr = xAudio->CreateMasteringVoice(&xAudioMasteringVoice, opusConfig->channelCount, opusConfig->sampleRate, 0))) {
			return hr;
		}
		this->opusConfig = opusConfig;
		PCMWAVEFORMAT  wfx = { 0 };
		wfx.wf.wFormatTag = WAVE_FORMAT_PCM;
		wfx.wf.nChannels = opusConfig->channelCount;
		wfx.wf.nSamplesPerSec = opusConfig->sampleRate;
		wfx.wBitsPerSample = 16;
		wfx.wf.nAvgBytesPerSec = (wfx.wf.nSamplesPerSec * wfx.wBitsPerSample *wfx.wf.nChannels) / 8;
		wfx.wf.nBlockAlign = (wfx.wf.nChannels * wfx.wBitsPerSample) / 8;
		if (FAILED(hr = xAudio->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*) &wfx))) {
			return hr;
		}
		return 0;
	}

	void AudioPlayer::Cleanup() {
		if (decoder != NULL) opus_multistream_decoder_destroy(decoder);
	}

	int AudioPlayer::SubmitDU(char* sampleData, int sampleLength) {
		HRESULT hr;
		int decodeLen = opus_multistream_decode(decoder,(unsigned char*)sampleData, sampleLength, pcmBuffer, 240, 0);
		if (decodeLen > 0) {
			XAUDIO2_BUFFER audioBuffer = { 0 };
			audioBuffer.pAudioData = (byte*)pcmBuffer;
			audioBuffer.AudioBytes = 1440;
			audioBuffer.PlayLength = 0;
			audioBuffer.PlayBegin = 0;
			audioBuffer.Flags = 0;
			audioBuffer.LoopCount = 0;
			audioBuffer.pContext = NULL;
			if (FAILED(hr = pSourceVoice->SubmitSourceBuffer(&audioBuffer))) {
				return hr;
			}
		}
		else {
			
		}
		return 0;
	}

	void AudioPlayer::Start() {
		HRESULT hr;
		if (FAILED(hr = pSourceVoice->Start(0))) {
			OutputDebugStringA("Error while starting");
		}
	}

	void AudioPlayer::Stop() {
		pSourceVoice->Stop();
	}
}