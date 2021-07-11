#pragma once

extern "C" {
	#include <libavcodec/avcodec.h>
}
namespace moonlight_xbox_dx {
	class MoonlightClient
	{
	public:
		void Init(std::shared_ptr<DX::DeviceResources> res,int width, int height);
		AVFrame* GetLastFrame();
	};
}