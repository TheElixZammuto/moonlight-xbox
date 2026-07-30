
#pragma once
#include "pch.h"
#include <ppltasks.h>
#include <windows.storage.streams.h>
#include <windows.graphics.imaging.h>

namespace ImageHelpers {

concurrency::task<Windows::Graphics::Imaging::SoftwareBitmap^> LoadSoftwareBitmapFromUriOrPathAsync(Platform::String^ path);

concurrency::task<Windows::Storage::Streams::IRandomAccessStream^> EncodeSoftwareBitmapToPngStreamAsync(Windows::Graphics::Imaging::SoftwareBitmap^ bitmap);

Windows::Graphics::Imaging::SoftwareBitmap^ EnsureBgra8Premultiplied(Windows::Graphics::Imaging::SoftwareBitmap^ bitmap);

Windows::Graphics::Imaging::SoftwareBitmap^ ResizeSoftwareBitmap(Windows::Graphics::Imaging::SoftwareBitmap^ src, unsigned int width, unsigned int height);

Windows::Graphics::Imaging::SoftwareBitmap^ ResizeSoftwareBitmapUniformToFill(Windows::Graphics::Imaging::SoftwareBitmap^ src, unsigned int width, unsigned int height);

bool AdjustSaturation(Windows::Graphics::Imaging::SoftwareBitmap^ bmp, float saturation);

concurrency::task<Windows::Storage::Streams::IRandomAccessStream^> CreateMaskedBlurredPngStreamAsync(
	Windows::Graphics::Imaging::SoftwareBitmap^ src,
	unsigned int targetW,
	unsigned int targetH,
	double dpi,
	float blurDip);

}
