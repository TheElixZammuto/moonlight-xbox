#pragma once

#include "Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "Common\StepTimer.h"
#include "State\MoonlightClient.h"
#include "State\StreamConfiguration.h"

namespace moonlight_xbox_dx
{

	typedef struct _Rect {
		int x, y, w, h;
	} RECT;

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
		void scaleSourceToDestinationSurface(RECT* src, RECT* dst);
		void screenSpaceToNormalizedDeviceCoords(RECT* src, FRECT* dst, int viewportWidth, int viewportHeight);
		bool Render();
		void bindColorConversion(AVFrame* frame);
		void SetHDR(bool enabled);
		void Stop();
		ID3D11Texture2D* GenerateTexture();
		void ToggleRecording();
		bool initComplete;
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
		Microsoft::WRL::ComPtr<ID3D11Texture2D>		m_videoTexture;
		D3D11_TEXTURE2D_DESC						renderTextureDesc;
		Windows::Graphics::Display::Core::HdmiDisplayMode^ m_lastDisplayMode;
		Windows::Graphics::Display::Core::HdmiDisplayMode^ m_currentDisplayMode;
		ID3D11ShaderResourceView* m_VideoTextureResourceViews[2];

		// System resources for cube geometry.
		ModelViewProjectionConstantBuffer	m_constantBufferData;

		// Variables used with the rendering loop.
		bool	m_loadingComplete;
		bool	m_LastFullRange;
		int		m_LastColorSpace;
		MoonlightClient *client;
		StreamConfiguration^ configuration;
	};
}

