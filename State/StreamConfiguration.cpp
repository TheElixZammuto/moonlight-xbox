#include "pch.h"
#include <Utils.hpp>
#include <State\StreamConfiguration.h>

using namespace moonlight_xbox_dx;
moonlight_xbox_dx::StreamConfiguration^ __streamConfig;

moonlight_xbox_dx::StreamConfiguration^ moonlight_xbox_dx::GetStreamConfig() {
	return __streamConfig;
}

void moonlight_xbox_dx::SetStreamConfig(StreamConfiguration^ config) {
	__streamConfig = config;
}