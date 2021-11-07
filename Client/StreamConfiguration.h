#pragma once
#include <string>
ref class StreamConfiguration {
public:
	property Platform::String^ hostname;
	property int appID;
	property int width;
	property int height;
	property int bitrate;
	property int FPS;
	property Platform::String^ audioConfig;
};
