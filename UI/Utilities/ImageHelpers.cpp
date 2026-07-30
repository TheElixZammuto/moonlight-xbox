#include "pch.h"
#include "ImageHelpers.h"
#define MLOG_TAG_OVERRIDE "ImageHelpers"
#include "../Utils.hpp"
#include "UI\Utilities\EffectsLibrary.h"

struct DECLSPEC_UUID("5B0D3235-4DBA-4D44-865E-8F1D0ED9F3E4") IMemoryBufferByteAccess : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetBuffer(BYTE** value, UINT32* capacity) = 0;
};

using namespace Platform;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::Graphics::Imaging;
using namespace concurrency;

static concurrency::task<SoftwareBitmap^> DecodeStreamToSoftwareBitmapAsync(concurrency::task<IRandomAccessStream^> streamTask, const char* context) {
    return streamTask.then([context](task<IRandomAccessStream^> st) -> task<SoftwareBitmap^> {
        try {
            auto stream = st.get();
            if (stream == nullptr || stream->Size == 0) return task_from_result<SoftwareBitmap^>(nullptr);
            return create_task(BitmapDecoder::CreateAsync(stream)).then([](BitmapDecoder^ decoder) -> task<SoftwareBitmap^> {
                if (decoder == nullptr) return task_from_result<SoftwareBitmap^>(nullptr);
                return create_task(decoder->GetSoftwareBitmapAsync()).then([](SoftwareBitmap^ sb) -> SoftwareBitmap^ {
                    try { return ImageHelpers::EnsureBgra8Premultiplied(sb); } catch(...) { MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "EnsureBgra8Premultiplied failed\n"); return nullptr; }
                });
            }).then([](task<SoftwareBitmap^> sbTask) -> SoftwareBitmap^ {
                try { return sbTask.get(); }
                catch (Platform::COMException^ ex) { MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "BitmapDecoder/GetSoftwareBitmap failed hr=0x%08x\n", ex->HResult); return nullptr; }
                catch(...) { MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "BitmapDecoder/GetSoftwareBitmap unknown error\n"); return nullptr; }
            });
        } catch(...) {
            MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "LoadSoftwareBitmapFromUriOrPathAsync: stream task failed (%s)\n", context);
            return task_from_result<SoftwareBitmap^>(nullptr);
        }
    });
}

concurrency::task<SoftwareBitmap^> ImageHelpers::LoadSoftwareBitmapFromUriOrPathAsync(String^ path) {
    if (path == nullptr) {
        MLOG(moonlight_xbox_dx::Utils::LogLevel::Warning, "LoadSoftwareBitmapFromUriOrPathAsync: path is null\n");
        return task_from_result<SoftwareBitmap^>(nullptr);
    }
    const wchar_t* raw = path->Data();
    try {

        size_t rawLen = wcslen(raw);
        if (rawLen >= 4 && _wcsicmp(raw + rawLen - 4, L".svg") == 0) {
            return task_from_result<SoftwareBitmap^>(nullptr);
        }

        if (wcsncmp(raw, L"ms-appx://", 10) == 0 || wcsncmp(raw, L"ms-appdata://", 12) == 0) {
            auto uri = ref new Windows::Foundation::Uri(path);

            concurrency::task<IRandomAccessStream^> streamTask = create_task(StorageFile::GetFileFromApplicationUriAsync(uri))
            .then([](task<StorageFile^> fileTask) -> task<IRandomAccessStream^> {
                try {
                    StorageFile^ file = fileTask.get();
                    return create_task(file->OpenReadAsync()).then([](task<IRandomAccessStreamWithContentType^> sTask) -> IRandomAccessStream^ {
                        try { auto s = sTask.get(); return safe_cast<IRandomAccessStream^>(s); } catch(...) { return nullptr; }
                    });
                } catch(...) {
                    MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "LoadSoftwareBitmapFromUriOrPathAsync: GetFileFromApplicationUriAsync failed\n");
                    return task_from_result<IRandomAccessStream^>(nullptr);
                }
            });
            return DecodeStreamToSoftwareBitmapAsync(streamTask, "ms-appx/ms-appdata");
        }

        if ((wcslen(raw) >= 2 && raw[1] == L':') || (wcslen(raw) >= 2 && raw[0] == L'\\' && raw[1] == L'\\')) {

            try {
                auto localPath = Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data();
                size_t lpLen = wcslen(localPath);
                if (_wcsnicmp(raw, localPath, lpLen) == 0) {

                    const wchar_t* rel = raw + lpLen;
                    if (*rel == L'\\' || *rel == L'/') ++rel;
                    Platform::String^ relStr = ref new Platform::String(rel);
                    concurrency::task<IRandomAccessStream^> streamTask = create_task(Windows::Storage::ApplicationData::Current->LocalFolder->GetFileAsync(relStr)).then([](StorageFile^ file) -> task<IRandomAccessStream^> {
                        if (file == nullptr) return task_from_result<IRandomAccessStream^>(nullptr);
                        return create_task(file->OpenReadAsync()).then([](IRandomAccessStreamWithContentType^ s) -> IRandomAccessStream^ { return safe_cast<IRandomAccessStream^>(s); });
                    });
                    return DecodeStreamToSoftwareBitmapAsync(streamTask, "LocalFolder-relative path");
                }
            } catch(...) {
                MLOG(moonlight_xbox_dx::Utils::LogLevel::Warning, "LoadSoftwareBitmapFromUriOrPathAsync: LocalFolder-relative path attempt failed, falling back\n");
            }

            concurrency::task<IRandomAccessStream^> streamTask = create_task(StorageFile::GetFileFromPathAsync(path)).then([](task<StorageFile^> fileTask) -> task<IRandomAccessStream^> {
                try {
                    StorageFile^ file = fileTask.get();
                    return create_task(file->OpenReadAsync()).then([](task<IRandomAccessStreamWithContentType^> sTask) -> IRandomAccessStream^ {
                        try { auto s = sTask.get(); return safe_cast<IRandomAccessStream^>(s); } catch(...) { return nullptr; }
                    });
                } catch(...) {
                    MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "LoadSoftwareBitmapFromUriOrPathAsync: GetFileFromPathAsync failed\n");
                    return task_from_result<IRandomAccessStream^>(nullptr);
                }
            });
            return DecodeStreamToSoftwareBitmapAsync(streamTask, "file path");
        }

        MLOGF(moonlight_xbox_dx::Utils::LogLevel::Warning, "LoadSoftwareBitmapFromUriOrPathAsync: unsupported path='%S'\n", raw);
        return task_from_result<SoftwareBitmap^>(nullptr);
    } catch(...) {
        MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "LoadSoftwareBitmapFromUriOrPathAsync: exception during load\n");
        return task_from_result<SoftwareBitmap^>(nullptr);
    }
}

static void UnpremultiplyBgraPixel(uint8_t pb, uint8_t pg, uint8_t pr, uint8_t pa,
                                    uint8_t& outB, uint8_t& outG, uint8_t& outR, uint8_t& outA) {
    if (pa == 0) { outB = outG = outR = outA = 0; return; }
    uint32_t r = (uint32_t)pr * 255 + (pa / 2);
    uint32_t g = (uint32_t)pg * 255 + (pa / 2);
    uint32_t b = (uint32_t)pb * 255 + (pa / 2);
    outR = (uint8_t)std::min<uint32_t>(255, r / pa);
    outG = (uint8_t)std::min<uint32_t>(255, g / pa);
    outB = (uint8_t)std::min<uint32_t>(255, b / pa);
    outA = pa;
}

static SoftwareBitmap^ BuildStraightAlphaBitmap(SoftwareBitmap^ bitmap) {
    try {
        unsigned int w = bitmap->PixelWidth;
        unsigned int h = bitmap->PixelHeight;

        SoftwareBitmap^ src = ImageHelpers::EnsureBgra8Premultiplied(bitmap);
        if (src == nullptr) src = bitmap;

        auto out = ref new SoftwareBitmap(BitmapPixelFormat::Bgra8, (int)w, (int)h, BitmapAlphaMode::Straight);

        bool fastPath = false;
        try {
            auto srcBuf = src->LockBuffer(BitmapBufferAccessMode::Read);
            auto outBuf = out->LockBuffer(BitmapBufferAccessMode::Write);
            auto srcRef = srcBuf->CreateReference();
            auto outRef = outBuf->CreateReference();
            Microsoft::WRL::ComPtr<IMemoryBufferByteAccess> srcAccess;
            Microsoft::WRL::ComPtr<IMemoryBufferByteAccess> outAccess;
            IUnknown* srcUnk = reinterpret_cast<IUnknown*>(srcRef);
            IUnknown* outUnk = reinterpret_cast<IUnknown*>(outRef);
            BYTE* srcData = nullptr; UINT32 srcCap = 0;
            BYTE* outData = nullptr; UINT32 outCap = 0;
            if (srcUnk != nullptr && outUnk != nullptr && SUCCEEDED(srcUnk->QueryInterface(IID_PPV_ARGS(&srcAccess))) && SUCCEEDED(outUnk->QueryInterface(IID_PPV_ARGS(&outAccess)))
                && SUCCEEDED(srcAccess->GetBuffer(&srcData, &srcCap)) && SUCCEEDED(outAccess->GetBuffer(&outData, &outCap))) {
                auto srcDesc = srcBuf->GetPlaneDescription(0);
                auto outDesc = outBuf->GetPlaneDescription(0);
                for (unsigned int y = 0; y < h; ++y) {
                    uint8_t* srow = srcData + srcDesc.StartIndex + (size_t)y * srcDesc.Stride;
                    uint8_t* orow = outData + outDesc.StartIndex + (size_t)y * outDesc.Stride;
                    for (unsigned int x = 0; x < w; ++x) {
                        UnpremultiplyBgraPixel(srow[x*4 + 0], srow[x*4 + 1], srow[x*4 + 2], srow[x*4 + 3],
                                                orow[x*4 + 0], orow[x*4 + 1], orow[x*4 + 2], orow[x*4 + 3]);
                    }
                }
                fastPath = true;
            }
        } catch(...) { fastPath = false; }

        if (!fastPath) {

            try {
                unsigned int sh = src->PixelHeight;
                auto srcBuf2 = src->LockBuffer(BitmapBufferAccessMode::Read);
                auto srcDesc2 = srcBuf2->GetPlaneDescription(0);
                std::vector<uint8_t> srcTmp((size_t)sh * (size_t)srcDesc2.Stride);
                try {
                    auto ib = ref new Windows::Storage::Streams::Buffer((unsigned int)srcTmp.size());
                    src->CopyToBuffer(ib);
                    auto reader = Windows::Storage::Streams::DataReader::FromBuffer(ib);
                    reader->ReadBytes(Platform::ArrayReference<uint8_t>(srcTmp.data(), (unsigned int)srcTmp.size()));
                } catch(...) {

                    MLOG(moonlight_xbox_dx::Utils::LogLevel::Warning, "BuildStraightAlphaBitmap: failed to read src buffer (fallback path)\n");
                }

                auto outDesc = out->LockBuffer(BitmapBufferAccessMode::Write)->GetPlaneDescription(0);
                std::vector<uint8_t> outTmp((size_t)h * (size_t)outDesc.Stride);
                for (unsigned int y = 0; y < h; ++y) {
                    uint8_t* srow = srcTmp.data() + (size_t)y * srcDesc2.Stride;
                    uint8_t* orow = outTmp.data() + (size_t)y * outDesc.Stride;
                    for (unsigned int x = 0; x < w; ++x) {
                        UnpremultiplyBgraPixel(srow[x*4 + 0], srow[x*4 + 1], srow[x*4 + 2], srow[x*4 + 3],
                                                orow[x*4 + 0], orow[x*4 + 1], orow[x*4 + 2], orow[x*4 + 3]);
                    }
                }

                try {
                    auto writer = ref new Windows::Storage::Streams::DataWriter();
                    writer->WriteBytes(Platform::ArrayReference<uint8_t>(outTmp.data(), (unsigned int)outTmp.size()));
                    auto buf = writer->DetachBuffer();
                    out->CopyFromBuffer(buf);
                } catch(...) {
                    MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "BuildStraightAlphaBitmap: failed to write unpremultiplied buffer into out bitmap\n");
                }
            } catch(...) {
                MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "BuildStraightAlphaBitmap: fallback unpremultiply path failed\n");
            }
        }

        return out;
    } catch(...) {

        return bitmap;
    }
}

static Platform::Array<unsigned char>^ ExtractStraightAlphaPixels(SoftwareBitmap^ bitmap, unsigned int outW, unsigned int outH) {
    try {
        SoftwareBitmap^ src = ImageHelpers::EnsureBgra8Premultiplied(bitmap);
        auto buf = src->LockBuffer(BitmapBufferAccessMode::Read);
        auto desc = buf->GetPlaneDescription(0);
        std::vector<uint8_t> srcBytes((size_t)outH * (size_t)desc.Stride);
        try {
            auto ib = ref new Windows::Storage::Streams::Buffer((unsigned int)srcBytes.size());
            src->CopyToBuffer(ib);
            auto reader = Windows::Storage::Streams::DataReader::FromBuffer(ib);
            reader->ReadBytes(Platform::ArrayReference<uint8_t>(srcBytes.data(), (unsigned int)srcBytes.size()));
        } catch(...) { srcBytes.clear(); }

        if (srcBytes.empty()) return nullptr;

        size_t pixCount = (size_t)outW * (size_t)outH;
        std::vector<uint8_t> outPixels(pixCount * 4);
        for (unsigned int y = 0; y < outH; ++y) {
            uint8_t* srow = srcBytes.data() + (size_t)y * desc.Stride;
            for (unsigned int x = 0; x < outW; ++x) {
                size_t idx = ((size_t)y * outW + x) * 4;
                UnpremultiplyBgraPixel(srow[x*4 + 0], srow[x*4 + 1], srow[x*4 + 2], srow[x*4 + 3],
                                        outPixels[idx + 0], outPixels[idx + 1], outPixels[idx + 2], outPixels[idx + 3]);
            }
        }
        auto pixelArr = ref new Platform::Array<unsigned char>((unsigned int)outPixels.size());
        memcpy(pixelArr->Data, outPixels.data(), outPixels.size());
        return pixelArr;
    } catch(...) {
        return nullptr;
    }
}

concurrency::task<IRandomAccessStream^> ImageHelpers::EncodeSoftwareBitmapToPngStreamAsync(SoftwareBitmap^ bitmap) {
    if (bitmap == nullptr) return task_from_result<IRandomAccessStream^>(nullptr);
    try {
        auto stream = ref new InMemoryRandomAccessStream();

        SoftwareBitmap^ encodeBitmap = BuildStraightAlphaBitmap(bitmap);

        unsigned int outW = bitmap->PixelWidth;
        unsigned int outH = bitmap->PixelHeight;
        Platform::Array<unsigned char>^ pixelArr = ExtractStraightAlphaPixels(encodeBitmap != nullptr ? encodeBitmap : bitmap, outW, outH);

        return concurrency::create_task(BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId, stream)).then([pixelArr, outW, outH, stream, encodeBitmap](BitmapEncoder^ encoder) -> concurrency::task<void> {
            bool usedSetPixelData = false;
            try {
                if (pixelArr != nullptr) {
                    encoder->SetPixelData(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Straight, outW, outH, 96.0, 96.0, pixelArr);
                    usedSetPixelData = true;
                }
            } catch(...) { usedSetPixelData = false; }

            if (!usedSetPixelData) {

                try {

                    if (encodeBitmap != nullptr) encoder->SetSoftwareBitmap(encodeBitmap);
                    else encoder->SetSoftwareBitmap(EnsureBgra8Premultiplied(ref new SoftwareBitmap(BitmapPixelFormat::Bgra8, outW, outH, BitmapAlphaMode::Premultiplied)));
                } catch(...) {
                    MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "EncodeSoftwareBitmapToPngStreamAsync: SetSoftwareBitmap fallback failed, encoder has no pixel data\n");
                }
            }
            return concurrency::create_task(encoder->FlushAsync());
        }).then([stream]() -> IRandomAccessStream^ {
            try {
                stream->Seek(0);
            } catch(...) {
                MLOG(moonlight_xbox_dx::Utils::LogLevel::Warning, "EncodeSoftwareBitmapToPngStreamAsync: exception seeking stream\n");
            }
            return stream;
        });
    } catch(...) {
        MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "EncodeSoftwareBitmapToPngStreamAsync: exception encoding\n");
        return task_from_result<IRandomAccessStream^>(nullptr);
    }
}

SoftwareBitmap^ ImageHelpers::EnsureBgra8Premultiplied(SoftwareBitmap^ bitmap) {
    if (bitmap == nullptr) return nullptr;
    if (bitmap->BitmapPixelFormat != BitmapPixelFormat::Bgra8 || bitmap->BitmapAlphaMode != BitmapAlphaMode::Premultiplied) {
        try {
            auto conv = SoftwareBitmap::Convert(bitmap, BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied);
            return conv;
        } catch(...) {
            MLOG(moonlight_xbox_dx::Utils::LogLevel::Warning, "EnsureBgra8Premultiplied: conversion failed\n");
            return bitmap;
        }
    }
    return bitmap;
}

SoftwareBitmap^ ImageHelpers::ResizeSoftwareBitmap(SoftwareBitmap^ src, unsigned int width, unsigned int height) {
    if (src == nullptr) return nullptr;
    try {
        src = EnsureBgra8Premultiplied(src);
        unsigned int srcW = src->PixelWidth;
        unsigned int srcH = src->PixelHeight;
        if (srcW == width && srcH == height) return src;

        auto outBmp = ref new SoftwareBitmap(BitmapPixelFormat::Bgra8, width, height, BitmapAlphaMode::Premultiplied);

        auto srcBuf = src->LockBuffer(BitmapBufferAccessMode::Read);
        auto srcDesc = srcBuf->GetPlaneDescription(0);
        auto dstBuf = outBmp->LockBuffer(BitmapBufferAccessMode::Write);
        auto dstDesc = dstBuf->GetPlaneDescription(0);

        auto srcRef = srcBuf->CreateReference();
        auto dstRef = dstBuf->CreateReference();
        Microsoft::WRL::ComPtr<IMemoryBufferByteAccess> srcAccess;
        Microsoft::WRL::ComPtr<IMemoryBufferByteAccess> dstAccess;
        IUnknown* srcUnk = reinterpret_cast<IUnknown*>(srcRef);
        IUnknown* dstUnk = reinterpret_cast<IUnknown*>(dstRef);
        BYTE* srcData = nullptr; UINT32 srcCap = 0;
        BYTE* dstData = nullptr; UINT32 dstCap = 0;
        bool haveFast = srcUnk != nullptr && dstUnk != nullptr
            && SUCCEEDED(srcUnk->QueryInterface(IID_PPV_ARGS(&srcAccess))) && SUCCEEDED(dstUnk->QueryInterface(IID_PPV_ARGS(&dstAccess)))
            && SUCCEEDED(srcAccess->GetBuffer(&srcData, &srcCap)) && SUCCEEDED(dstAccess->GetBuffer(&dstData, &dstCap));

        if (haveFast) {
            for (unsigned int y = 0; y < height; ++y) {
                unsigned int sy = (unsigned int)((uint64_t)y * srcH / height);
                uint8_t* srcRow = srcData + srcDesc.StartIndex + sy * srcDesc.Stride;
                uint8_t* dstRow = dstData + dstDesc.StartIndex + y * dstDesc.Stride;
                for (unsigned int x = 0; x < width; ++x) {
                    unsigned int sx = (unsigned int)((uint64_t)x * srcW / width);
                    uint8_t* pSrc = srcRow + sx * 4;
                    uint8_t* pDst = dstRow + x * 4;
                    pDst[0] = pSrc[0]; pDst[1] = pSrc[1]; pDst[2] = pSrc[2]; pDst[3] = pSrc[3];
                }
            }
            return outBmp;
        }

        std::vector<uint8_t> srcBufData((size_t)srcH * (size_t)srcDesc.Stride);
        try {
            auto ib = ref new Windows::Storage::Streams::Buffer((unsigned int)srcBufData.size());
            src->CopyToBuffer(ib);
            auto reader = Windows::Storage::Streams::DataReader::FromBuffer(ib);
            reader->ReadBytes(Platform::ArrayReference<uint8_t>(srcBufData.data(), (unsigned int)srcBufData.size()));
        } catch(...) {
            MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "ResizeSoftwareBitmap: failed to read src buffer\n");
            return nullptr;
        }

        std::vector<uint8_t> dstTmp((size_t)height * (size_t)dstDesc.Stride);
        for (unsigned int y = 0; y < height; ++y) {
            unsigned int sy = (unsigned int)((uint64_t)y * srcH / height);
            uint8_t* srcRow = srcBufData.data() + sy * srcDesc.Stride;
            uint8_t* dstRow = dstTmp.data() + y * dstDesc.Stride;
            for (unsigned int x = 0; x < width; ++x) {
                unsigned int sx = (unsigned int)((uint64_t)x * srcW / width);
                uint8_t* pSrc = srcRow + sx * 4;
                uint8_t* pDst = dstRow + x * 4;
                pDst[0] = pSrc[0]; pDst[1] = pSrc[1]; pDst[2] = pSrc[2]; pDst[3] = pSrc[3];
            }
        }

        try {
            auto writer = ref new Windows::Storage::Streams::DataWriter();
            writer->WriteBytes(Platform::ArrayReference<uint8_t>(dstTmp.data(), (unsigned int)dstTmp.size()));
            auto buf = writer->DetachBuffer();

            dstRef = nullptr; dstBuf = nullptr;
            outBmp->CopyFromBuffer(buf);
            return outBmp;
        } catch(...) {
            MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "ResizeSoftwareBitmap: failed to write dst buffer\n");
            return nullptr;
        }
    } catch(...) {
        MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "ResizeSoftwareBitmap: exception during resize\n");
        return nullptr;
    }
}

SoftwareBitmap^ ImageHelpers::ResizeSoftwareBitmapUniformToFill(SoftwareBitmap^ src, unsigned int width, unsigned int height) {
    if (src == nullptr) return nullptr;
    try {
        unsigned int srcW = src->PixelWidth;
        unsigned int srcH = src->PixelHeight;
        if (srcW == 0 || srcH == 0) return nullptr;
        double srcAspect = (double)srcW / (double)srcH;
        double dstAspect = (double)width / (double)height;

        unsigned int cropX = 0, cropY = 0, cropW = srcW, cropH = srcH;
        if (srcAspect > dstAspect) {

            cropH = srcH;
            cropW = (unsigned int)std::ceil(dstAspect * (double)cropH);
            if (cropW > srcW) cropW = srcW;
            cropX = (srcW - cropW) / 2;
            cropY = 0;
        } else if (srcAspect < dstAspect) {

            cropW = srcW;
            cropH = (unsigned int)std::ceil((double)cropW / dstAspect);
            if (cropH > srcH) cropH = srcH;
            cropY = (srcH - cropH) / 2;
            cropX = 0;
        } else {

        }

        if (cropX == 0 && cropY == 0 && cropW == srcW && cropH == srcH) {
            return ResizeSoftwareBitmap(src, width, height);
        }

        auto cropped = ref new SoftwareBitmap(BitmapPixelFormat::Bgra8, cropW, cropH, BitmapAlphaMode::Premultiplied);

        auto srcBuf = src->LockBuffer(BitmapBufferAccessMode::Read);
        auto srcDesc = srcBuf->GetPlaneDescription(0);
        auto dstBuf = cropped->LockBuffer(BitmapBufferAccessMode::Write);
        auto dstDesc = dstBuf->GetPlaneDescription(0);

        try {

            std::vector<uint8_t> srcData((size_t)srcH * (size_t)srcDesc.Stride);
            try {
                auto ib = ref new Windows::Storage::Streams::Buffer((unsigned int)srcData.size());
                src->CopyToBuffer(ib);
                auto reader = Windows::Storage::Streams::DataReader::FromBuffer(ib);
                reader->ReadBytes(Platform::ArrayReference<uint8_t>(srcData.data(), (unsigned int)srcData.size()));
            } catch(...) {
                MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "ResizeSoftwareBitmapUniformToFill: failed to read src buffer\n");
                return nullptr;
            }

            std::vector<uint8_t> dstTmp((size_t)cropH * (size_t)dstDesc.Stride);
            for (unsigned int y = 0; y < cropH; ++y) {
                uint8_t* srcRow = srcData.data() + (size_t)(cropY + y) * srcDesc.Stride;
                uint8_t* dstRow = dstTmp.data() + (size_t)y * dstDesc.Stride;
                for (unsigned int x = 0; x < cropW; ++x) {
                    uint8_t* pSrc = srcRow + (size_t)(cropX + x) * 4;
                    uint8_t* pDst = dstRow + (size_t)x * 4;
                    pDst[0] = pSrc[0]; pDst[1] = pSrc[1]; pDst[2] = pSrc[2]; pDst[3] = pSrc[3];
                }
            }

            try {
                auto writer = ref new Windows::Storage::Streams::DataWriter();
                writer->WriteBytes(Platform::ArrayReference<uint8_t>(dstTmp.data(), (unsigned int)dstTmp.size()));
                auto buf = writer->DetachBuffer();
                dstBuf = nullptr;
                cropped->CopyFromBuffer(buf);
            } catch(...) {
                MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "ResizeSoftwareBitmapUniformToFill: failed to write cropped buffer\n");
                return nullptr;
            }
        } catch(...) {
            MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "ResizeSoftwareBitmapUniformToFill: unexpected exception\n");
            return nullptr;
        }

        return ResizeSoftwareBitmap(cropped, width, height);
    } catch(...) {
        MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "ResizeSoftwareBitmapUniformToFill: exception during operation\n");
        return nullptr;
    }
}

bool ImageHelpers::AdjustSaturation(SoftwareBitmap^ bmp, float saturation) {
    if (bmp == nullptr) return false;
    try {
        auto b = EnsureBgra8Premultiplied(bmp);
        if (b == nullptr) return false;
        auto buf = b->LockBuffer(BitmapBufferAccessMode::ReadWrite);
        auto desc = buf->GetPlaneDescription(0);
        unsigned int w = b->PixelWidth; unsigned int h = b->PixelHeight;

        if (saturation == 1.0f) return true;

        const float satf = saturation;

        try {
            auto ref = buf->CreateReference();
            Microsoft::WRL::ComPtr<IMemoryBufferByteAccess> access;
            IUnknown* unk = reinterpret_cast<IUnknown*>(ref);
            BYTE* raw = nullptr; UINT32 cap = 0;
            if (unk != nullptr && SUCCEEDED(unk->QueryInterface(IID_PPV_ARGS(&access)))
                && SUCCEEDED(access->GetBuffer(&raw, &cap)) && raw != nullptr) {
                for (unsigned int y = 0; y < h; ++y) {
                    uint8_t* row = raw + desc.StartIndex + (size_t)y * desc.Stride;
                    for (unsigned int x = 0; x < w; ++x) {
                        uint8_t b_px = row[x*4 + 0];
                        uint8_t g_px = row[x*4 + 1];
                        uint8_t r_px = row[x*4 + 2];
                        uint8_t a_px = row[x*4 + 3];
                        if (a_px == 0) continue;

                        unsigned int invA = std::max<unsigned int>(1, a_px);
                        float ur = (float)((uint32_t)r_px * 255u) / (float)invA;
                        float ug = (float)((uint32_t)g_px * 255u) / (float)invA;
                        float ub = (float)((uint32_t)b_px * 255u) / (float)invA;

                        float gray = ur * 0.299f + ug * 0.587f + ub * 0.114f;

                        float nrf = gray + satf * (ur - gray);
                        float ngf = gray + satf * (ug - gray);
                        float nbf = gray + satf * (ub - gray);

                        int nri = (int)std::round(nrf);
                        int ngi = (int)std::round(ngf);
                        int nbi = (int)std::round(nbf);
                        nri = nri < 0 ? 0 : (nri > 255 ? 255 : nri);
                        ngi = ngi < 0 ? 0 : (ngi > 255 ? 255 : ngi);
                        nbi = nbi < 0 ? 0 : (nbi > 255 ? 255 : nbi);

                        uint8_t pr = (uint8_t)((uint32_t)nri * a_px / 255u);
                        uint8_t pg = (uint8_t)((uint32_t)ngi * a_px / 255u);
                        uint8_t pb = (uint8_t)((uint32_t)nbi * a_px / 255u);

                        row[x*4 + 0] = pb; row[x*4 + 1] = pg; row[x*4 + 2] = pr; row[x*4 + 3] = a_px;
                    }
                }
                return true;
            }
        } catch(...) {
            MLOG(moonlight_xbox_dx::Utils::LogLevel::Warning, "AdjustSaturation: fast-path buffer access failed, falling back\n");
        }

        try {
            buf = nullptr;
            std::vector<uint8_t> data((size_t)h * desc.Stride);
            auto ib = ref new Windows::Storage::Streams::Buffer((unsigned int)data.size());
            b->CopyToBuffer(ib);
            auto reader = Windows::Storage::Streams::DataReader::FromBuffer(ib);
            reader->ReadBytes(Platform::ArrayReference<uint8_t>(data.data(), (unsigned int)data.size()));

            for (unsigned int y = 0; y < h; ++y) {
                uint8_t* row = data.data() + (size_t)y * desc.Stride;
                for (unsigned int x = 0; x < w; ++x) {
                    uint8_t b_px = row[x*4 + 0];
                    uint8_t g_px = row[x*4 + 1];
                    uint8_t r_px = row[x*4 + 2];
                    uint8_t a_px = row[x*4 + 3];
                    if (a_px == 0) continue;

                    unsigned int invA = std::max<unsigned int>(1, a_px);
                    float ur = (float)((uint32_t)r_px * 255u) / (float)invA;
                    float ug = (float)((uint32_t)g_px * 255u) / (float)invA;
                    float ub = (float)((uint32_t)b_px * 255u) / (float)invA;

                    float gray = ur * 0.299f + ug * 0.587f + ub * 0.114f;

                    float nrf = gray + satf * (ur - gray);
                    float ngf = gray + satf * (ug - gray);
                    float nbf = gray + satf * (ub - gray);

                    int nri = (int)std::round(nrf);
                    int ngi = (int)std::round(ngf);
                    int nbi = (int)std::round(nbf);
                    nri = nri < 0 ? 0 : (nri > 255 ? 255 : nri);
                    ngi = ngi < 0 ? 0 : (ngi > 255 ? 255 : ngi);
                    nbi = nbi < 0 ? 0 : (nbi > 255 ? 255 : nbi);

                    uint8_t pr = (uint8_t)((uint32_t)nri * a_px / 255u);
                    uint8_t pg = (uint8_t)((uint32_t)ngi * a_px / 255u);
                    uint8_t pb = (uint8_t)((uint32_t)nbi * a_px / 255u);

                    row[x*4 + 0] = pb; row[x*4 + 1] = pg; row[x*4 + 2] = pr; row[x*4 + 3] = a_px;
                }
            }

            auto writer = ref new Windows::Storage::Streams::DataWriter();
            writer->WriteBytes(Platform::ArrayReference<uint8_t>(data.data(), (unsigned int)data.size()));
            auto outBuf = writer->DetachBuffer();
            b->CopyFromBuffer(outBuf);
            return true;
        } catch(...) { return false; }
    } catch(...) { return false; }
}

concurrency::task<Windows::Storage::Streams::IRandomAccessStream^> ImageHelpers::CreateMaskedBlurredPngStreamAsync(
    SoftwareBitmap^ src,
    unsigned int targetW,
    unsigned int targetH,
    double dpi,
    float blurDip) {
    if (src == nullptr) return task_from_result<IRandomAccessStream^>(nullptr);

    try {

        unsigned int tW = targetW != 0 ? targetW : src->PixelWidth;
        unsigned int tH = targetH != 0 ? targetH : src->PixelHeight;

        auto raster = ResizeSoftwareBitmapUniformToFill(src, tW, tH);
        if (raster != nullptr) src = raster;

        unsigned int radiusPx = 0;
        try { radiusPx = (unsigned int)std::round((double)blurDip * dpi / 96.0); } catch(...) { radiusPx = (unsigned int)std::round((double)blurDip); }

        SoftwareBitmap^ preBlurBitmap = src;

        try {
            auto gpuResult = ::EffectsLibrary::GpuBoxBlurSoftwareBitmap(preBlurBitmap, (int)radiusPx, true);
            if (gpuResult != nullptr) {

                try { AdjustSaturation(gpuResult, 1.0f); } catch(...) {
                    MLOG(moonlight_xbox_dx::Utils::LogLevel::Warning, "CreateMaskedBlurredPngStreamAsync: AdjustSaturation on GPU result failed\n");
                }
                return create_task(EncodeSoftwareBitmapToPngStreamAsync(gpuResult)).then([](IRandomAccessStream^ s) -> IRandomAccessStream^ {
                    if (s != nullptr) {
                        try { s->Seek(0); } catch(...) {
                            MLOG(moonlight_xbox_dx::Utils::LogLevel::Warning, "CreateMaskedBlurredPngStreamAsync: failed to seek GPU-path stream\n");
                        }
                    }
                    return s;
                });
            }
        } catch(...) {
            MLOG(moonlight_xbox_dx::Utils::LogLevel::Warning, "CreateMaskedBlurredPngStreamAsync: GPU blur failed, falling back to CPU\n");
        }

        try {
            auto cpuTarget = preBlurBitmap;
            ::EffectsLibrary::BoxBlurSoftwareBitmap(cpuTarget, (int)radiusPx);
            try { AdjustSaturation(cpuTarget, 1.0f); } catch(...) {
                MLOG(moonlight_xbox_dx::Utils::LogLevel::Warning, "CreateMaskedBlurredPngStreamAsync: AdjustSaturation on CPU result failed\n");
            }
            return create_task(EncodeSoftwareBitmapToPngStreamAsync(cpuTarget)).then([](IRandomAccessStream^ s) -> IRandomAccessStream^ {
                if (s != nullptr) {
                    try { s->Seek(0); } catch(...) {
                        MLOG(moonlight_xbox_dx::Utils::LogLevel::Warning, "CreateMaskedBlurredPngStreamAsync: failed to seek CPU-path stream\n");
                    }
                }
                return s;
            });
        } catch(...) {
            MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "CreateMaskedBlurredPngStreamAsync: CPU blur fallback also failed\n");
        }

        return task_from_result<IRandomAccessStream^>(nullptr);
    } catch(...) {
        return task_from_result<IRandomAccessStream^>(nullptr);
    }
}
