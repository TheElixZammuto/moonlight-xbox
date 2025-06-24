#pragma once
#include <string>
namespace moonlight_xbox_dx
{
    ref class StreamConfiguration {
	public:
		property Platform::String^ hostname;
		property int appID;
		property int width;
		property int height;
		property int bitrate;
		property int FPS;
		property bool supportsHevc;
		property Platform::String^ audioConfig;
		property Platform::String^ videoCodec;
		property bool enableHDR;
		property bool playAudioOnPC;
		property bool enableVsync;
		property bool enableSOPS;
	};

	moonlight_xbox_dx::StreamConfiguration^ GetStreamConfig();
	void SetStreamConfig(StreamConfiguration^ config);

}
