#include "pch.h"
#include <AudioPlayer.h>
#include <opus/opus_multistream.h>
#include <xaudio2.h>
#include <MoonlightClient.h>
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
		
		//Initialize XAudio2
		if (FAILED(hr = XAudio2Create(xAudio.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR))) {
			return hr;
		}
		if (FAILED(hr = xAudio->CreateMasteringVoice(&xAudioMasteringVoice, opusConfig->channelCount, opusConfig->sampleRate, 0))) {
			return hr;
		}
		this->channelCount = opusConfig->channelCount;
		this->sampleCount = opusConfig->samplesPerFrame;
		WAVEFORMATEX  wfx = { 0 };
		wfx.wFormatTag = WAVE_FORMAT_PCM;
		wfx.nChannels = 2;
		wfx.nSamplesPerSec = 48000L;
		wfx.nAvgBytesPerSec = 192000L;
		wfx.nBlockAlign = 4;
		wfx.wBitsPerSample = 16;
		wfx.cbSize = 0;
		if (FAILED(hr = xAudio->CreateSourceVoice(&pSourceVoice, &wfx))) {
			return hr;
		}
		Platform::String^ folderString = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
		folderString = folderString->Concat(folderString, "\\test.pcm");
		char folder[2048];
		wcstombs_s(NULL, folder, folderString->Data(), 2047);
		file = fopen(folder, "w");
		return 0;
	}

	void AudioPlayer::Cleanup() {
		if (decoder != NULL) opus_multistream_decoder_destroy(decoder);
	}

	int AudioPlayer::SubmitDU(char* sampleData, int sampleLength) {
		HRESULT hr;
		int decodeLen = opus_multistream_decode(decoder,(unsigned char*)sampleData, sampleLength, pcmBuffer[bufferIndex], sampleCount, 0);
		if (decodeLen > 0) {
			if (file != NULL) {
				fwrite(pcmBuffer[bufferIndex], sizeof(opus_int16), decodeLen * this->channelCount, file);
			}
			xaudioBuffers[bufferIndex];
			xaudioBuffers[bufferIndex].pAudioData = (byte*)(pcmBuffer[bufferIndex]);
			xaudioBuffers[bufferIndex].AudioBytes = decodeLen * this->channelCount * sizeof(opus_int16);
			xaudioBuffers[bufferIndex].PlayBegin = 0;
			xaudioBuffers[bufferIndex].PlayLength = 0;
			xaudioBuffers[bufferIndex].Flags = 0;
			xaudioBuffers[bufferIndex].LoopCount = 0;
			xaudioBuffers[bufferIndex].pContext = NULL;
			if (FAILED(hr = pSourceVoice->SubmitSourceBuffer(&(xaudioBuffers[bufferIndex])))) {
				return hr;
			}
			XAUDIO2_PERFORMANCE_DATA state;
			xAudio->GetPerformanceData(&state);
			char msg[2048];
			sprintf(msg, "Got queued this number of buffers %d\n", state.GlitchesSinceEngineStarted);
			MoonlightClient::GetInstance()->InsertLog(msg);
			bufferIndex = (bufferIndex + 1) % BUFFER_COUNT;
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