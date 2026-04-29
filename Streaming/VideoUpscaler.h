#pragma once

#include "pch.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include "..\Common\DeviceResources.h"

// Define NIS generic architecture mapping based on GPU
#include <NIS_Config.h>

namespace moonlight_xbox_dx {

    class VideoUpscaler {
    public:
        VideoUpscaler(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        ~VideoUpscaler();

        // Initialize the shaders, buffers, and textures for a fixed input/output resolution
        bool Initialize(int inWidth, int inHeight, int outWidth, int outHeight, DXGI_FORMAT format, bool isHDR);

        // Processes the input SRV through NIS scaling and sharpening pass
        ID3D11ShaderResourceView* ProcessFrame(ID3D11ShaderResourceView* inputSRV);

        // Cleans up all D3D resources
        void Cleanup();

    private:
        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_nisShader;

        Microsoft::WRL::ComPtr<ID3D11Buffer> m_nisCB;

        // NIS Coefficient textures
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_coefScaleTex;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_coefScaleSRV;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_coefUsmTex;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_coefUsmSRV;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_finalOutputTex;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_finalOutputSRV;
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_finalOutputUAV;

        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;

        int m_outWidth;
        int m_outHeight;
        uint32_t m_dispatchX;
        uint32_t m_dispatchY;

        // Helper to initialize constant buffers
        bool CreateConstantBuffer(const void* data, size_t size, ID3D11Buffer** buffer);
        bool CreateCoefTexture(const void* data, int width, int height, int rowPitch, DXGI_FORMAT format, ID3D11Texture2D** tex, ID3D11ShaderResourceView** srv);
    };
}
