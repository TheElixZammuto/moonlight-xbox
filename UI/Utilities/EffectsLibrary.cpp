#include "pch.h"
#include "UI\Utilities\EffectsLibrary.h"
#include <DirectXColors.h>
#include <wrl.h>
#include <robuffer.h>
#define MLOG_TAG_OVERRIDE "EffectsLibrary"
#include "../Utils.hpp"
#include "Common\DirectXHelper.h"
#include <vector>
#include <mutex>
#include "UI\Utilities\ImageHelpers.h"
using namespace Windows::Graphics::Imaging;

struct DECLSPEC_UUID("5B0D3235-4DBA-4D44-865E-8F1D0ED9F3E4") IMemoryBufferByteAccess : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetBuffer(BYTE** value, UINT32* capacity) = 0;
};

ID3D11Device* EffectsLibrary::m_device = nullptr;
ID3D11DeviceContext* EffectsLibrary::m_context = nullptr;
ID3D11Multithread* EffectsLibrary::m_multithread = nullptr;
ID3D11VertexShader* EffectsLibrary::m_vs = nullptr;
ID3D11PixelShader* EffectsLibrary::m_blurPS = nullptr;
ID3D11Buffer* EffectsLibrary::m_cb = nullptr;
ID3D11SamplerState* EffectsLibrary::m_sampler = nullptr;
std::mutex EffectsLibrary::m_mutex;
bool EffectsLibrary::m_ownsDevice = false;

void EffectsLibrary::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_device == device && m_context == context)
        return;

    if (m_vs) { m_vs->Release(); m_vs = nullptr; }
    if (m_blurPS) { m_blurPS->Release(); m_blurPS = nullptr; }
    if (m_cb) { m_cb->Release(); m_cb = nullptr; }
    if (m_sampler) { m_sampler->Release(); m_sampler = nullptr; }

    if (m_ownsDevice) {
        if (m_context) { m_context->Release(); m_context = nullptr; }
        if (m_device) { m_device->Release(); m_device = nullptr; }
        if (m_multithread) { m_multithread->SetMultithreadProtected(FALSE); m_multithread->Release(); m_multithread = nullptr; }
        m_ownsDevice = false;
    }

    m_device = device;
    m_context = context;

    m_ownsDevice = false;
    if (m_multithread) { m_multithread->SetMultithreadProtected(FALSE); m_multithread->Release(); m_multithread = nullptr; }
}

bool EffectsLibrary::EnsureDeviceInitialized()
{

    if (m_device != nullptr && m_context != nullptr) return true;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_device != nullptr && m_context != nullptr) return true;

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL createdFL = D3D_FEATURE_LEVEL_11_0;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, featureLevels, _countof(featureLevels),
        D3D11_SDK_VERSION, &dev, &createdFL, &ctx);
    if (FAILED(hr) || dev == nullptr || ctx == nullptr) {
        MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "lazy D3D11CreateDevice failed hr=0x%08x\n", hr);

        return false;
    }

    m_device = dev;
    m_context = ctx;

    Microsoft::WRL::ComPtr<ID3D11Multithread> mt;
    if (SUCCEEDED(m_device->QueryInterface(__uuidof(ID3D11Multithread), reinterpret_cast<void**>(mt.GetAddressOf())))) {
        mt->SetMultithreadProtected(TRUE);

        mt.Get()->AddRef();
        m_multithread = mt.Get();
    }
    m_ownsDevice = true;
    return true;
}

bool EffectsLibrary::EnsureBlurShadersCompiled()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_vs != nullptr && m_blurPS != nullptr) return true;

    const char* vsSrc = "struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };\n"
        "VSOut VS(uint vid : SV_VertexID) { VSOut o; float2 pos[3] = { float2(-1,-1), float2(-1,3), float2(3,-1) }; o.pos = float4(pos[vid], 0.0f, 1.0f); o.uv = pos[vid] * 0.5f + 0.5f; return o; }\n";
    const char* psSrc =
        "Texture2D srcTex : register(t0); SamplerState samp : register(s0); cbuffer BlurCB : register(b0) { float2 texSize; float sigma; int direction; };\n"
        "struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };\n"
        "float4 PS(VSOut i) : SV_TARGET {\n"
        "    float s = max(sigma, 0.0001f);\n"
        "    int radius = (int)ceil(3.0f * s);\n"
        "    float2 texel = float2(1.0/texSize.x, 1.0/texSize.y);\n"
        "    float2 step = (direction==0) ? float2(texel.x,0) : float2(0,texel.y);\n"
        "    float4 sum = float4(0,0,0,0);\n"
        "    float wsum = 0.0f;\n"
        "    float twoSigmaSq = 2.0f * s * s;\n"
        "    for (int k = -radius; k <= radius; ++k) {\n"
        "        float wk = exp(-((float)(k*k)) / twoSigmaSq);\n"
        "        sum += srcTex.SampleLevel(samp, i.uv + step * k, 0) * wk;\n"
        "        wsum += wk;\n"
        "    }\n"
        "    return sum / wsum;\n"
        "}\n";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
    HRESULT hr = D3DCompile(vsSrc, strlen(vsSrc), nullptr, nullptr, nullptr, "VS", "vs_4_0", flags, 0, vsBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errBlob) MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "EnsureBlurShadersCompiled: VS compile error: %s\n", (const char*)errBlob->GetBufferPointer());
        return false;
    }
    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
    if (FAILED(hr)) { MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "EnsureBlurShadersCompiled: CreateVertexShader failed hr=0x%08x\n", hr); return false; }

    hr = D3DCompile(psSrc, strlen(psSrc), nullptr, nullptr, nullptr, "PS", "ps_4_0", flags, 0, psBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errBlob) MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "EnsureBlurShadersCompiled: PS compile error: %s\n", (const char*)errBlob->GetBufferPointer());
        return false;
    }
    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_blurPS);
    if (FAILED(hr)) { MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "EnsureBlurShadersCompiled: CreatePixelShader failed hr=0x%08x\n", hr); return false; }

    if (m_sampler == nullptr) {
        D3D11_SAMPLER_DESC sd = {};

        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

        sd.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MinLOD = 0;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        sd.BorderColor[0] = 0.0f;
        sd.BorderColor[1] = 0.0f;
        sd.BorderColor[2] = 0.0f;
        sd.BorderColor[3] = 0.0f;
        m_device->CreateSamplerState(&sd, &m_sampler);
    }

    if (m_cb == nullptr) {
        D3D11_BUFFER_DESC cbd = {};
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbd.ByteWidth = sizeof(BlurCB);
        m_device->CreateBuffer(&cbd, nullptr, &m_cb);
    }
    return true;
}

static SoftwareBitmap^ EnsureBgra8Premultiplied(Windows::Graphics::Imaging::SoftwareBitmap^ bmp)
{
    if (bmp == nullptr) return nullptr;
    if (bmp->BitmapPixelFormat == BitmapPixelFormat::Bgra8 && bmp->BitmapAlphaMode == BitmapAlphaMode::Premultiplied)
        return bmp;
    return ImageHelpers::EnsureBgra8Premultiplied(bmp);
}

bool EffectsLibrary::BoxBlurSoftwareBitmap(Windows::Graphics::Imaging::SoftwareBitmap^ bitmap, int radius) {
    using namespace Windows::Graphics::Imaging;
    if (bitmap == nullptr) return false;
    try {
        auto buffer = bitmap->LockBuffer(BitmapBufferAccessMode::ReadWrite);
        auto reference = buffer->CreateReference();
        Microsoft::WRL::ComPtr<IMemoryBufferByteAccess> bufferByteAccess;
        HRESULT hr = S_OK;
        IUnknown* unk = reinterpret_cast<IUnknown*>(reference);
        if (unk == nullptr) {
            MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "BoxBlurSoftwareBitmap: CreateReference returned null IUnknown\n");
            return false;
        }
        hr = unk->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));
        BYTE* data = nullptr; UINT32 capacity = 0;
        bool usedFastPath = false;
        if (!FAILED(hr) && bufferByteAccess != nullptr) {
            hr = bufferByteAccess->GetBuffer(&data, &capacity);
            if (!FAILED(hr) && data != nullptr) {
                usedFastPath = true;
            } else {
                MLOGF(moonlight_xbox_dx::Utils::LogLevel::Warning, "BoxBlurSoftwareBitmap: GetBuffer failed hr=0x%08x\n", hr);
            }
        } else {
            MLOGF(moonlight_xbox_dx::Utils::LogLevel::Warning, "BoxBlurSoftwareBitmap: QueryInterface failed hr=0x%08x\n", hr);
        }
        auto desc = buffer->GetPlaneDescription(0);
        int width = desc.Width;
        int height = desc.Height;
        int stride = desc.Stride;
        int start = desc.StartIndex;
        if (width <= 0 || height <= 0 || stride <= 0) return false;
        size_t planeSize = (size_t)height * (size_t)stride;
        std::vector<uint8_t> src(planeSize);
        if (usedFastPath) {
            memcpy(src.data(), data + start, planeSize);
        } else {

            reference = nullptr;
            buffer = nullptr;
            try {
                auto ibuf = ref new Windows::Storage::Streams::Buffer((unsigned int)planeSize);
                bitmap->CopyToBuffer(ibuf);
                auto reader = Windows::Storage::Streams::DataReader::FromBuffer(ibuf);
                reader->ReadBytes(Platform::ArrayReference<uint8_t>(src.data(), (unsigned int)planeSize));
            } catch(...) {
                MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "BoxBlurSoftwareBitmap: fallback CopyToBuffer/DataReader failed\n");
                return false;
            }
        }

        std::vector<uint8_t> tmp(src.size());
        std::vector<uint8_t> dst(src.size());

        for (int y = 0; y < height; ++y) {
            const uint8_t* row = src.data() + y * stride;
            std::vector<uint32_t> prefB(width + 1, 0), prefG(width + 1, 0), prefR(width + 1, 0), prefA(width + 1, 0);
            for (int x = 0; x < width; ++x) {
                uint8_t b = row[x * 4 + 0];
                uint8_t g = row[x * 4 + 1];
                uint8_t r = row[x * 4 + 2];
                uint8_t a = row[x * 4 + 3];
                prefB[x + 1] = prefB[x] + b;
                prefG[x + 1] = prefG[x] + g;
                prefR[x + 1] = prefR[x] + r;
                prefA[x + 1] = prefA[x] + a;
            }
            for (int x = 0; x < width; ++x) {
                int x0 = std::max(0, x - radius);
                int x1 = std::min(width - 1, x + radius);
                int count = x1 - x0 + 1;
                uint32_t sumB = prefB[x1 + 1] - prefB[x0];
                uint32_t sumG = prefG[x1 + 1] - prefG[x0];
                uint32_t sumR = prefR[x1 + 1] - prefR[x0];
                uint32_t sumA = prefA[x1 + 1] - prefA[x0];
                uint8_t* outPx = tmp.data() + y * stride + x * 4;
                outPx[0] = (uint8_t)(sumB / count);
                outPx[1] = (uint8_t)(sumG / count);
                outPx[2] = (uint8_t)(sumR / count);
                outPx[3] = (uint8_t)(sumA / count);
            }
        }

        for (int x = 0; x < width; ++x) {
            std::vector<uint32_t> prefB(height + 1, 0), prefG(height + 1, 0), prefR(height + 1, 0), prefA(height + 1, 0);
            for (int y = 0; y < height; ++y) {
                uint8_t* px = tmp.data() + y * stride + x * 4;
                prefB[y + 1] = prefB[y] + px[0];
                prefG[y + 1] = prefG[y] + px[1];
                prefR[y + 1] = prefR[y] + px[2];
                prefA[y + 1] = prefA[y] + px[3];
            }
            for (int y = 0; y < height; ++y) {
                int y0 = std::max(0, y - radius);
                int y1 = std::min(height - 1, y + radius);
                int count = y1 - y0 + 1;
                uint32_t sumB = prefB[y1 + 1] - prefB[y0];
                uint32_t sumG = prefG[y1 + 1] - prefG[y0];
                uint32_t sumR = prefR[y1 + 1] - prefR[y0];
                uint32_t sumA = prefA[y1 + 1] - prefA[y0];
                uint8_t* outPx = dst.data() + y * stride + x * 4;
                outPx[0] = (uint8_t)(sumB / count);
                outPx[1] = (uint8_t)(sumG / count);
                outPx[2] = (uint8_t)(sumR / count);
                outPx[3] = (uint8_t)(sumA / count);
            }
        }
        if (usedFastPath) {
            memcpy(data + start, dst.data(), planeSize);
        } else {
            try {
                auto writer = ref new Windows::Storage::Streams::DataWriter();
                writer->WriteBytes(Platform::ArrayReference<uint8_t>(dst.data(), (unsigned int)planeSize));
                auto outBuf = writer->DetachBuffer();
                bitmap->CopyFromBuffer(outBuf);
            } catch(...) {
                MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "BoxBlurSoftwareBitmap: fallback CopyFromBuffer failed\n");
                return false;
            }
        }

        return true;
    } catch(...) {
    MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "BoxBlurSoftwareBitmap: exception while blurring\n");
        return false;
    }
}

static void CopyBgraRegion(uint8_t* dst, size_t dstRowPitch, const uint8_t* src, size_t srcRowPitch, int pad, bool returnPadded, int copyWidth, int copyHeight)
{
    size_t srcOffsetRow = returnPadded ? 0 : (size_t)pad * srcRowPitch;
    size_t srcOffsetCol = returnPadded ? 0 : (size_t)pad * 4;
    for (int y = 0; y < copyHeight; ++y) {
        const uint8_t* srcRow = src + srcOffsetRow + (size_t)y * srcRowPitch + srcOffsetCol;
        size_t copyBytes = std::min((size_t)copyWidth * 4, dstRowPitch);
        memcpy(dst + (size_t)y * dstRowPitch, srcRow, copyBytes);
        if (dstRowPitch > copyBytes) memset(dst + (size_t)y * dstRowPitch + copyBytes, 0, dstRowPitch - copyBytes);
    }
}

SoftwareBitmap^ EffectsLibrary::GpuBoxBlurSoftwareBitmap(SoftwareBitmap^ bitmap, int radius, bool returnPadded)
{
    using namespace Windows::Graphics::Imaging;
    if (bitmap == nullptr) return nullptr;

    if ((m_device == nullptr || m_context == nullptr) && !EnsureDeviceInitialized()) {
        MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: D3D device/context not initialized\n");
        return nullptr;
    }

    bitmap = EnsureBgra8Premultiplied(bitmap);

    try {
        auto buffer = bitmap->LockBuffer(BitmapBufferAccessMode::Read);
        auto reference = buffer->CreateReference();
        Microsoft::WRL::ComPtr<IMemoryBufferByteAccess> bufferByteAccess;
        HRESULT hr = S_OK;
        IUnknown* unk = reinterpret_cast<IUnknown*>(reference);
        BYTE* data = nullptr; UINT32 capacity = 0;
        bool usedFastPath = false;
        if (unk != nullptr) hr = unk->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));
        if (!FAILED(hr) && bufferByteAccess != nullptr) {
            hr = bufferByteAccess->GetBuffer(&data, &capacity);
            if (!FAILED(hr) && data != nullptr) usedFastPath = true;
        }

        auto desc = buffer->GetPlaneDescription(0);
        int width = desc.Width;
        int height = desc.Height;
        int stride = desc.Stride;
        int start = desc.StartIndex;
        if (width <= 0 || height <= 0 || stride <= 0) return nullptr;
        size_t planeSize = (size_t)height * (size_t)stride;
        std::vector<uint8_t> src(planeSize);
        if (usedFastPath) {
            memcpy(src.data(), data + start, planeSize);
        } else {

            reference = nullptr;
            buffer = nullptr;
            try {
                auto ibuf = ref new Windows::Storage::Streams::Buffer((unsigned int)planeSize);
                bitmap->CopyToBuffer(ibuf);
                auto reader = Windows::Storage::Streams::DataReader::FromBuffer(ibuf);
                reader->ReadBytes(Platform::ArrayReference<uint8_t>(src.data(), (unsigned int)planeSize));
            } catch(...) {
                MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: fallback CopyToBuffer failed\n");
                return nullptr;
            }
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTex;
        D3D11_TEXTURE2D_DESC texDesc = {};

        int pad = 54;
        int paddedW = width + pad * 2;
        int paddedH = height + pad * 2;
        int paddedStride = paddedW * 4;
        texDesc.Width = paddedW;
        texDesc.Height = paddedH;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        texDesc.CPUAccessFlags = 0;

        std::vector<uint8_t> paddedSrc((size_t)paddedH * (size_t)paddedStride);

        memset(paddedSrc.data(), 0, paddedSrc.size());

        for (int y = 0; y < height; ++y) {
            uint8_t* dstRow = paddedSrc.data() + ((size_t)(y + pad) * (size_t)paddedStride);
            uint8_t* srcRow = src.data() + ((size_t)y * (size_t)stride);

            memcpy(dstRow + pad * 4, srcRow, width * 4);
        }

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = paddedSrc.data();
        initData.SysMemPitch = paddedStride;

        hr = m_device->CreateTexture2D(&texDesc, &initData, srcTex.GetAddressOf());
        if (FAILED(hr) || srcTex == nullptr) {
            MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: CreateTexture2D(src padded) failed hr=0x%08x\n", hr);
            return nullptr;
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcSRV;
        hr = m_device->CreateShaderResourceView(srcTex.Get(), nullptr, srcSRV.GetAddressOf());
        if (FAILED(hr) || srcSRV == nullptr) {
            MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: CreateShaderResourceView(src) failed hr=0x%08x\n", hr);
            return nullptr;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> rtA, rtB;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtvA, rtvB;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srvB;

        D3D11_TEXTURE2D_DESC rtDesc = texDesc;
        rtDesc.Width = paddedW;
        rtDesc.Height = paddedH;
        rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        rtDesc.Usage = D3D11_USAGE_DEFAULT;
        rtDesc.CPUAccessFlags = 0;

        hr = m_device->CreateTexture2D(&rtDesc, nullptr, rtA.GetAddressOf());
        if (FAILED(hr)) { MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: CreateTexture2D(rtA) failed hr=0x%08x\n", hr); return nullptr; }
        hr = m_device->CreateTexture2D(&rtDesc, nullptr, rtB.GetAddressOf());
        if (FAILED(hr)) { MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: CreateTexture2D(rtB) failed hr=0x%08x\n", hr); return nullptr; }
        hr = m_device->CreateRenderTargetView(rtA.Get(), nullptr, rtvA.GetAddressOf());
        if (FAILED(hr)) { MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: CreateRenderTargetView(rtA) failed hr=0x%08x\n", hr); return nullptr; }
        hr = m_device->CreateRenderTargetView(rtB.Get(), nullptr, rtvB.GetAddressOf());
        if (FAILED(hr)) { MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: CreateRenderTargetView(rtB) failed hr=0x%08x\n", hr); return nullptr; }
        hr = m_device->CreateShaderResourceView(rtB.Get(), nullptr, srvB.GetAddressOf());
        if (FAILED(hr)) { MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: CreateSRV(rtB) failed hr=0x%08x\n", hr); return nullptr; }

        if (!EnsureBlurShadersCompiled()) return nullptr;

        D3D11_VIEWPORT vp = {};
        vp.TopLeftX = 0.f;
        vp.TopLeftY = 0.f;
        vp.Width = (float)paddedW;
        vp.Height = (float)paddedH;
        vp.MinDepth = 0.f;
        vp.MaxDepth = 1.f;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_context->IASetInputLayout(nullptr);
            m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_context->VSSetShader(m_vs, nullptr, 0);
            m_context->PSSetShader(m_blurPS, nullptr, 0);
            ID3D11SamplerState* samps[1] = { m_sampler };
            m_context->PSSetSamplers(0, 1, samps);

            BlurCB cbdata;
            cbdata.texSize = DirectX::XMFLOAT2((float)paddedW, (float)paddedH);
            cbdata.sigma = (float)radius;

            cbdata.direction = 0;
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (m_cb) {
                if (SUCCEEDED(m_context->Map(m_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                    memcpy(mapped.pData, &cbdata, sizeof(cbdata));
                    m_context->Unmap(m_cb, 0);
                }
                ID3D11Buffer* cbs[1] = { m_cb };
                m_context->PSSetConstantBuffers(0, 1, cbs);
            }

            ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
            ID3D11ShaderResourceView* srvSrcPad = srcSRV.Get();
            m_context->PSSetShaderResources(0, 1, &srvSrcPad);
            ID3D11RenderTargetView* rtvBptr = rtvB.Get();
            m_context->OMSetRenderTargets(1, &rtvBptr, nullptr);
            m_context->RSSetViewports(1, &vp);
            m_context->Draw(3, 0);

            ID3D11RenderTargetView* nullRTVArr[1] = { nullptr };
            m_context->OMSetRenderTargets(1, nullRTVArr, nullptr);

            m_context->PSSetShaderResources(0, 1, nullSRV);
            cbdata.direction = 1;
            if (m_cb) {
                if (SUCCEEDED(m_context->Map(m_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                    memcpy(mapped.pData, &cbdata, sizeof(cbdata));
                    m_context->Unmap(m_cb, 0);
                }
                ID3D11Buffer* cbs2[1] = { m_cb };
                m_context->PSSetConstantBuffers(0, 1, cbs2);
            }
            ID3D11ShaderResourceView* srvBptr = srvB.Get();
            m_context->PSSetShaderResources(0, 1, &srvBptr);
            ID3D11RenderTargetView* rtvAptr = rtvA.Get();
            m_context->OMSetRenderTargets(1, &rtvAptr, nullptr);
            m_context->RSSetViewports(1, &vp);
            m_context->Draw(3, 0);

            m_context->PSSetShaderResources(0, 1, nullSRV);

            ID3D11RenderTargetView* nullRTV[1] = { nullptr };
            m_context->OMSetRenderTargets(1, nullRTV, nullptr);
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
        D3D11_TEXTURE2D_DESC stagingDesc = rtDesc;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.MiscFlags = 0;
        hr = m_device->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf());
        if (FAILED(hr)) { MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: CreateTexture2D(staging) failed hr=0x%08x\n", hr); return nullptr; }
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_context->CopyResource(staging.Get(), rtA.Get());

            D3D11_MAPPED_SUBRESOURCE mapSR = {};
            hr = m_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapSR);
            if (FAILED(hr)) {
                MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: Map staging failed hr=0x%08x\n", hr);
                return nullptr;
            }
            bool stagingMapped = true;

            int copyWidth = returnPadded ? paddedW : width;
            int copyHeight = returnPadded ? paddedH : height;
            uint8_t* srcPtr = (uint8_t*)mapSR.pData;
            size_t srcRowPitch = mapSR.RowPitch;

            try {
                auto outBmp = ref new SoftwareBitmap(BitmapPixelFormat::Bgra8, copyWidth, copyHeight, BitmapAlphaMode::Premultiplied);
                auto outBuf = outBmp->LockBuffer(BitmapBufferAccessMode::Write);
                auto outRef = outBuf->CreateReference();
                Microsoft::WRL::ComPtr<IMemoryBufferByteAccess> outAccess;
                IUnknown* outUnk = reinterpret_cast<IUnknown*>(outRef);

                static std::atomic<int> s_outAccessAvailable(-1);
                HRESULT outQiHr = E_FAIL;
                BYTE* outData = nullptr; UINT32 outCap = 0;

                if (s_outAccessAvailable.load() == 1) {
                    if (outUnk != nullptr) outQiHr = outUnk->QueryInterface(IID_PPV_ARGS(&outAccess));
                    if (SUCCEEDED(outQiHr) && outAccess) outAccess->GetBuffer(&outData, &outCap);
                } else if (s_outAccessAvailable.load() == 0) {
                    outQiHr = E_NOINTERFACE;
                } else {
                    try {
                        if (outUnk != nullptr) outQiHr = outUnk->QueryInterface(IID_PPV_ARGS(&outAccess));
                    } catch (...) {
                        outQiHr = RPC_E_DISCONNECTED;
                    }
                    if (SUCCEEDED(outQiHr) && outAccess) {
                        outAccess->GetBuffer(&outData, &outCap);
                        s_outAccessAvailable.store(1);
                    } else {
                        s_outAccessAvailable.store(0);
                    }
                }

                if (SUCCEEDED(outQiHr) && outData != nullptr) {
                    auto outDesc = outBuf->GetPlaneDescription(0);
                    uint8_t* dstPtr = outData + outDesc.StartIndex;
                    size_t dstRowPitch = outDesc.Stride;
                    if (srcPtr == nullptr) {
                        if (stagingMapped) { m_context->Unmap(staging.Get(), 0); stagingMapped = false; }
                        MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: invalid mapped source pointer (null)\n");
                        return nullptr;
                    }
                    if (srcRowPitch < (size_t)width * 4) {
                        if (stagingMapped) { m_context->Unmap(staging.Get(), 0); stagingMapped = false; }
                        MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: unexpected srcRowPitch=%u (width*4=%u)\n", (unsigned)srcRowPitch, (unsigned)(width*4));
                        return nullptr;
                    }
                    if (outCap < outDesc.StartIndex + dstRowPitch * (size_t)height) {
                        if (stagingMapped) { m_context->Unmap(staging.Get(), 0); stagingMapped = false; }
                        MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: output buffer capacity too small (cap=%u required=%zu)\n", outCap, outDesc.StartIndex + dstRowPitch * (size_t)height);
                        return nullptr;
                    }
                    CopyBgraRegion(dstPtr, dstRowPitch, srcPtr, srcRowPitch, pad, returnPadded, copyWidth, copyHeight);
                    if (stagingMapped) { m_context->Unmap(staging.Get(), 0); stagingMapped = false; }
                    return outBmp;
                }

                try {
                    if (srcPtr == nullptr) {
                        if (stagingMapped) { m_context->Unmap(staging.Get(), 0); stagingMapped = false; }
                        MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: invalid mapped source pointer (null) in fallback\n");
                        return nullptr;
                    }
                    if (srcRowPitch < (size_t)4) {
                        if (stagingMapped) { m_context->Unmap(staging.Get(), 0); stagingMapped = false; }
                        MLOGF(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: unexpected small srcRowPitch=%u in fallback\n", (unsigned)srcRowPitch);
                        return nullptr;
                    }
                    auto outDesc = outBuf->GetPlaneDescription(0);
                    size_t dstRowPitch = outDesc.Stride;
                    size_t allocSize = (size_t)copyHeight * dstRowPitch;
                    if (allocSize == 0 || allocSize / (size_t)copyHeight != dstRowPitch) {
                        if (stagingMapped) { m_context->Unmap(staging.Get(), 0); stagingMapped = false; }
                        MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: integer overflow or zero detected allocating tmpBuf\n");
                        return nullptr;
                    }
                    std::vector<uint8_t> tmpBuf(allocSize);
                    CopyBgraRegion(tmpBuf.data(), dstRowPitch, srcPtr, srcRowPitch, pad, returnPadded, copyWidth, copyHeight);
                    if (stagingMapped) { m_context->Unmap(staging.Get(), 0); stagingMapped = false; }

                    auto writer = ref new Windows::Storage::Streams::DataWriter();
                    writer->WriteBytes(Platform::ArrayReference<uint8_t>(tmpBuf.data(), (unsigned int)tmpBuf.size()));
                    auto outIBuf = writer->DetachBuffer();
                    return SoftwareBitmap::CreateCopyFromBuffer(outIBuf, BitmapPixelFormat::Bgra8, copyWidth, copyHeight, BitmapAlphaMode::Premultiplied);
                } catch(...) {
                    if (stagingMapped) { m_context->Unmap(staging.Get(), 0); stagingMapped = false; }
                    MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: fallback CopyFromBuffer failed\n");
                    return nullptr;
                }
            } catch(...) {
                if (stagingMapped) { m_context->Unmap(staging.Get(), 0); stagingMapped = false; }
                MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: exception while creating output bitmap\n");
                return nullptr;
            }
        }

    } catch(...) {
        MLOG(moonlight_xbox_dx::Utils::LogLevel::Error, "GpuBoxBlurSoftwareBitmap: unexpected exception\n");
        return nullptr;
    }
}

