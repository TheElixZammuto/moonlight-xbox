#pragma once
#include "pch.h"
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
		int channelCount;
	private:
		OpusMSDecoder* decoder;
		int sampleRate;
		int samplePerFrame;
		opus_int16 pcmBuffer[240 * 6];
	};
}