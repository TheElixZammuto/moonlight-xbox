#pragma once
#include "pch.h"
#include <xaudio2.h>
extern "C" {
#include <Limelight.h>
#include <opus/opus_multistream.h>
}
#define BUFFER_COUNT 64
namespace moonlight_xbox_dx
{

	class AudioPlayer {
	public:
		int Init(int audioConfiguration, const POPUS_MULTISTREAM_CONFIGURATION opusConfig, void* context, int arFlags);
		void Start();
		void Stop();
		void Cleanup();
		int SubmitDU(char* sampleData, int sampleLength);
		static AudioPlayer* getInstance();
		static AUDIO_RENDERER_CALLBACKS getDecoder();
		bool setup = false;
	private:
		OpusMSDecoder* decoder;
		Microsoft::WRL::ComPtr<IXAudio2> xAudio;
		IXAudio2MasteringVoice *xAudioMasteringVoice;
		IXAudio2SourceVoice* pSourceVoice;
		int channelCount;
		int sampleCount;
		opus_int16 pcmBuffer[240 * 6][BUFFER_COUNT];
		XAUDIO2_BUFFER xaudioBuffers[BUFFER_COUNT];
		int bufferIndex = 0;
		FILE* file;
	};
}