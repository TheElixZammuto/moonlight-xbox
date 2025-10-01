#pragma once

#include "ShaderStructures.h"
#include "Common\StepTimer.h"
#include "State\MoonlightClient.h"
#include "State\StreamConfiguration.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace moonlight_xbox_dx
{

	typedef struct _Rect {
		int x, y, w, h;
	} IRECT;

	typedef struct _Frect {
		float x, y, w, h;
	} FRECT;
	// This sample renderer instantiates a basic rendering pipeline.
	class VideoRenderer
	{
	public:
		VideoRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources,MoonlightClient *client,StreamConfiguration ^sConfig);
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::StepTimer const& timer);
		void scaleSourceToDestinationSurface(IRECT* src, IRECT* dst);
		void screenSpaceToNormalizedDeviceCoords(IRECT* src, FRECT* dst, int viewportWidth, int viewportHeight);
		void EnsureYuvTargets(ID3D11Device* dev, DXGI_FORMAT fmt, UINT w, UINT h);
		bool Render(AVFrame* frame);
		void bindColorConversion(AVFrame* frame);
		void SetHDR(bool enabled);
		void Stop();
		ID3D11Texture2D* GenerateTexture();


	private:
		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// Direct3D resources for cube geometry.
		Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShaderBT601,m_pixelShaderBT2020,m_pixelShaderGeneric;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_constantBuffer;
		Microsoft::WRL::ComPtr<ID3D11SamplerState>	samplerState;
		D3D11_TEXTURE2D_DESC						renderTextureDesc;
		Windows::Graphics::Display::Core::HdmiDisplayMode^ m_lastDisplayMode;
		Windows::Graphics::Display::Core::HdmiDisplayMode^ m_currentDisplayMode;

		Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_yuvTexture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srvY;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srvUV;
		DXGI_FORMAT                                      m_yuvFormat = DXGI_FORMAT_UNKNOWN;
		UINT                                             m_yuvWidth = 0, m_yuvHeight = 0;

		// System resources for cube geometry.
		ModelViewProjectionConstantBuffer	m_constantBufferData;

		// Variables used with the rendering loop.
		AVColorTransferCharacteristic m_LastColorTrc;
		DXGI_HDR_METADATA_HDR10 m_lastHdr10;
		std::atomic<bool> m_loadingComplete;
		bool	m_LastFullRange;
		int		m_LastColorSpace;
		MoonlightClient *client;
		StreamConfiguration^ configuration;
	};
}

