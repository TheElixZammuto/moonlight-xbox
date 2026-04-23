#include "pch.h"
#include "VideoUpscaler.h"
#include "..\Common\DirectXHelper.h"
#include "Utils.hpp"
#include <d3dcompiler.h>
#include <string>

#pragma comment(lib, "d3dcompiler")

using namespace moonlight_xbox_dx;

VideoUpscaler::VideoUpscaler(const std::shared_ptr<DX::DeviceResources>& deviceResources)
    : m_deviceResources(deviceResources), m_outWidth(0), m_outHeight(0) {
}

VideoUpscaler::~VideoUpscaler() {
    Cleanup();
}

bool VideoUpscaler::CreateConstantBuffer(const void* data, size_t size, ID3D11Buffer** buffer) {
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.ByteWidth = static_cast<UINT>((size + 15) & ~15); 
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    HRESULT hr = m_deviceResources->GetD3DDevice()->CreateBuffer(&desc, &initData, buffer);
    return SUCCEEDED(hr);
}

bool VideoUpscaler::CreateCoefTexture(const void* data, int width, int height, int rowPitch, DXGI_FORMAT format, ID3D11Texture2D** tex, ID3D11ShaderResourceView** srv) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;
    initData.SysMemPitch = rowPitch;

    auto device = m_deviceResources->GetD3DDevice();
    if (FAILED(device->CreateTexture2D(&desc, &initData, tex))) return false;
    if (FAILED(device->CreateShaderResourceView(*tex, nullptr, srv))) return false;
    return true;
}

// Initialize NIS components: Load shaders, setup constants and allocate output textures.
bool VideoUpscaler::Initialize(int inWidth, int inHeight, int outWidth, int outHeight, DXGI_FORMAT format, bool isHDR) {
    Cleanup();

    if (outWidth == 0 || outHeight == 0 || inWidth == 0 || inHeight == 0) {
        return false;
    }

    m_outWidth = outWidth;
    m_outHeight = outHeight;

    auto device = m_deviceResources->GetD3DDevice();

    // Check if Half-Precision (FP16) is supported
    D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT featureSupport = {};
    device->CheckFeatureSupport(D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &featureSupport, sizeof(featureSupport));
    bool supportsFP16 = (featureSupport.AllOtherShaderStagesMinPrecision & D3D11_SHADER_MIN_PRECISION_16_BIT) != 0;

    // Calculate optimal NIS parameters
    NISGPUArchitecture gpuArch = supportsFP16 ? NISGPUArchitecture::NVIDIA_Generic_fp16 : NISGPUArchitecture::NVIDIA_Generic;
    NISOptimizer opt(true, gpuArch);
    
    UINT blockWidth = opt.GetOptimalBlockWidth();
    UINT blockHeight = opt.GetOptimalBlockHeight();
    UINT threadGroupSize = opt.GetOptimalThreadGroupSize();

    std::string sBlockWidth = std::to_string(blockWidth);
    std::string sBlockHeight = std::to_string(blockHeight);
    std::string sThreadGroupSize = std::to_string(threadGroupSize);
    std::string sHDRMode = isHDR ? "2" : "0"; // 2 for PQ HDR
    std::string sUseFP16 = supportsFP16 ? "1" : "0";

    // 1. Compile NIS shader from HLSL source
    try {
        std::wstring fullShaderPath = Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data();
        fullShaderPath += L"\\third_party\\NVIDIAImageScaling\\NIS\\NIS_Main.hlsl";

        D3D_SHADER_MACRO defines[] = {
            { "NIS_SCALER",             "1" },
            { "NIS_HDR_MODE",           sHDRMode.c_str() },
            { "NIS_BLOCK_WIDTH",        sBlockWidth.c_str() },
            { "NIS_BLOCK_HEIGHT",       sBlockHeight.c_str() },
            { "NIS_THREAD_GROUP_SIZE",  sThreadGroupSize.c_str() },
            { "NIS_USE_HALF_PRECISION", sUseFP16.c_str() },
            { nullptr, nullptr }
        };

        Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
        
        HRESULT hr = D3DCompileFromFile(
            fullShaderPath.c_str(),
            defines,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "main",
            "cs_5_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3,
            0,
            &shaderBlob,
            &errorBlob
        );

        if (FAILED(hr)) {
            if (errorBlob) {
                Utils::Logf("NIS Shader compilation failed: %s\n", (char*)errorBlob->GetBufferPointer());
            } else {
                Utils::Logf("NIS Shader compilation failed with HRESULT 0x%X\n", hr);
            }
            return false;
        }

        DX::ThrowIfFailed(device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, m_nisShader.ReleaseAndGetAddressOf()), "Create NIS Compute Shader");
    } catch (Platform::Exception^ e) {
        Utils::Logf("Failed to compile or create NIS shader. HRESULT: 0x%X\n", e->HResult);
        return false;
    } catch (...) {
        Utils::Log("Failed to compile NIS shader with unknown exception\n");
        return false;
    }

    // 2. Initialize NIS configuration constants
    NISConfig config = {};
    float sharpness = 0.25f; 
    NISHDRMode hdrMode = isHDR ? NISHDRMode::PQ : NISHDRMode::None;
    
    NVScalerUpdateConfig(config, sharpness, 0, 0, inWidth, inHeight, inWidth, inHeight, 0, 0, outWidth, outHeight, outWidth, outHeight, hdrMode);
    
    if (!CreateConstantBuffer(&config, sizeof(config), m_nisCB.ReleaseAndGetAddressOf())) return false;

    // 3. Create coefficient textures
    const int rowPitch = supportsFP16 ? kFilterSize * sizeof(uint16_t) : kFilterSize * sizeof(float);
    DXGI_FORMAT coefFormat = supportsFP16 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT;
    const void* scaleData = supportsFP16 ? (const void*)coef_scale_fp16 : (const void*)coef_scale;
    const void* usmData = supportsFP16 ? (const void*)coef_usm_fp16 : (const void*)coef_usm;

    if (!CreateCoefTexture(scaleData, kFilterSize / 4, kPhaseCount, rowPitch, coefFormat, m_coefScaleTex.ReleaseAndGetAddressOf(), m_coefScaleSRV.ReleaseAndGetAddressOf())) return false;
    if (!CreateCoefTexture(usmData, kFilterSize / 4, kPhaseCount, rowPitch, coefFormat, m_coefUsmTex.ReleaseAndGetAddressOf(), m_coefUsmSRV.ReleaseAndGetAddressOf())) return false;

    Utils::Logf("VideoUpscaler: Initialized NIS (%dx%d -> %dx%d, HDR: %d, FP16: %d, Block: %ux%u)\n", inWidth, inHeight, outWidth, outHeight, isHDR, supportsFP16, blockWidth, blockHeight);

    // 4. Create output texture (supporting UAV and SRV)
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = outWidth;
    texDesc.Height = outHeight;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, m_finalOutputTex.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;
    
    hr = device->CreateShaderResourceView(m_finalOutputTex.Get(), nullptr, m_finalOutputSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;
    
    hr = device->CreateUnorderedAccessView(m_finalOutputTex.Get(), nullptr, m_finalOutputUAV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        Utils::Logf("Failed to create UAV for NIS final texture. HRESULT: 0x%X\n", hr);
        return false;
    }

    // 5. Create Sampler (Linear Clamp)
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    hr = device->CreateSamplerState(&sampDesc, m_sampler.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    // 6. Calculate Dispatch arguments
    m_dispatchX = static_cast<UINT>(std::ceil(m_outWidth / float(blockWidth)));
    m_dispatchY = static_cast<UINT>(std::ceil(m_outHeight / float(blockHeight)));

    return true;
}

// Dispatches the compute shader to apply NIS to the input frame.
ID3D11ShaderResourceView* VideoUpscaler::ProcessFrame(ID3D11ShaderResourceView* inputSRV) {
    if (!m_nisShader || !inputSRV) return inputSRV;

    auto ctx = m_deviceResources->GetD3DDeviceContext();

    ID3D11SamplerState* samplers[] = { m_sampler.Get() };
    ctx->CSSetSamplers(0, 1, samplers);

    // NIS Pass (Upscale + Sharpen)
    ctx->CSSetShader(m_nisShader.Get(), nullptr, 0);
    ctx->CSSetConstantBuffers(0, 1, m_nisCB.GetAddressOf());
    
    ID3D11ShaderResourceView* srvs[] = { inputSRV, m_coefScaleSRV.Get(), m_coefUsmSRV.Get() };
    ctx->CSSetShaderResources(0, 3, srvs);
    ctx->CSSetUnorderedAccessViews(0, 1, m_finalOutputUAV.GetAddressOf(), nullptr);

    ctx->Dispatch(m_dispatchX, m_dispatchY, 1);

    // Clean up CS state
    ID3D11UnorderedAccessView* nullUAV[] = { nullptr };
    ctx->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr };
    ctx->CSSetShaderResources(0, 3, nullSRVs);

    return m_finalOutputSRV.Get();
}

// Release all associated resources to prevent leaks.
void VideoUpscaler::Cleanup() {
    m_nisShader.Reset();
    m_nisCB.Reset();
    m_coefScaleTex.Reset();
    m_coefScaleSRV.Reset();
    m_coefUsmTex.Reset();
    m_coefUsmSRV.Reset();
    m_finalOutputTex.Reset();
    m_finalOutputSRV.Reset();
    m_finalOutputUAV.Reset();
    m_sampler.Reset();
}
