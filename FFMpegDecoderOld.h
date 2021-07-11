#pragma once
#include <mutex>
extern "C" {
#include "Limelight.h"
}

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
}

AVFrame* GetLastFrame();
void ResetLastFrame();
bool HasSetup();
namespace moonlight_xbox_dx
{
	AVFrame* ffmpeg_get_frame();
	class FFMpegDecoder
	{
	public:
		DECODER_RENDERER_CALLBACKS GetCallbacks();
	};
}