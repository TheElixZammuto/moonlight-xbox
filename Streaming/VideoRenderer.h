#pragma once

#include "ShaderStructures.h"
#include "Common\StepTimer.h"
#include "State\MoonlightClient.h"
#include "State\StreamConfiguration.h"
#include <atomic>
#include <array>
#include <unordered_map>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>  // AVFrame
#include <libavutil/pixfmt.h> // AVColorTransferCharacteristic
}

namespace moonlight_xbox_dx
{

	typedef struct _Rect {
		int x, y, w, h;
	} IRECT;

	typedef struct _Frect {
		float x, y, w, h;
	} FRECT;

	typedef struct _DECODER_PARAMETERS {
		int videoFormat;
		int width;
		int height;
		int frameRate;
	} DECODER_PARAMETERS, *PDECODER_PARAMETERS;

	class VideoRenderer
	{
	public:
		bool IsLoadingComplete() const { return m_loadingComplete.load(std::memory_order_acquire); }
		bool IsLoadingSuccessful() const { return m_loadingSuccessful.load(std::memory_order_acquire); }
		VideoRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources,MoonlightClient *client,StreamConfiguration ^sConfig);
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::StepTimer const& timer);
		void scaleSourceToDestinationSurface(IRECT* src, IRECT* dst);
		void screenSpaceToNormalizedDeviceCoords(IRECT* src, FRECT* dst, int viewportWidth, int viewportHeight);
		bool Render(AVFrame* frame);
		void bindColorConversion(AVFrame* frame, D3D11_TEXTURE2D_DESC frameDesc);
		void RequestRefreshColorspace();
		void SetHDR(bool enabled);
		void Stop();

	private:
		const std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2>*
			getDirectSampleSrvs(ID3D11Texture2D* texture, UINT slice, const D3D11_TEXTURE2D_DESC& desc);
		void setupVertexBuffer(D3D11_TEXTURE2D_DESC frameDesc, float uMax, float vMax);
		void getFramePremultipliedCscConstants(const AVFrame* frame, std::array<float, 9> &cscMatrix, std::array<float, 3> &offsets);
		void getFrameChromaCositingOffsets(const AVFrame* frame, std::array<float, 2> &chromaOffsets);
		bool hasFrameFormatChanged(const AVFrame* frame);

		// ---- W10M FL 9_3 path: hardware decode + 9_3 shader sampling ----
		// Fully self-contained: no HDMI/HDR/PyroWave/VideoProcessor, own shader
		// bytecode, own dispatch. Shared fall-through code caused double-init
		// crashes, so nothing in here may be shared with the desktop path.
		void CreateDeviceDependentResourcesW10M();
		void CreateDeviceDependentResourcesDesktop();
		bool RenderW10M(AVFrame* frame);
		bool RenderDesktop(AVFrame* frame);
		bool RenderVideoProcessor(AVFrame* frame);
		const std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2>* getFlatSrvs(ID3D11Texture2D* texture);
		Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> getInputView(ID3D11Texture2D* texture, UINT slice);

		Microsoft::WRL::ComPtr<ID3D11VideoDevice>				m_videoDevice;
		Microsoft::WRL::ComPtr<ID3D11VideoContext>				m_videoContext;
		Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>	m_vpEnumerator;
		Microsoft::WRL::ComPtr<ID3D11VideoProcessor>			m_videoProcessor;
		Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView>	m_vpOutputView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D>					m_vpMiddleTex;
		Microsoft::WRL::ComPtr<ID3D11Texture2D>					m_vpBackBufferTex;
		Microsoft::WRL::ComPtr<ID3D11Texture2D>					m_vpFlatNV12;
		UINT m_vpOutWidth = 0;
		UINT m_vpOutHeight = 0;
		// VP input views over the decoder's NV12 array textures, keyed by the
		// texture pointer and indexed by array slice (mirrors m_DirectSampleSrvs).
		std::unordered_map<ID3D11Texture2D*,
			std::vector<Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView>>> m_VPInputViews;

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// Direct3D resources for cube geometry.
		Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_VideoVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_vertexShader;
		// Texture2DArray YUV->RGB shader, samples the decoder surfaces directly
		Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShaderYUV420Array;
		// level_9_3 variant (plain Texture2D planes) for the W10M path
		Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShaderYUV420_93;
		// Flat-texture SRV pairs (TEXTURE2D view, R8 + R8G8) for the W10M path
		std::unordered_map<ID3D11Texture2D*,
			std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2>> m_FlatSrvs;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShaderPyroWave; // 3-plane path, 4:4:4 + 4:2:0
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_cscConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11SamplerState>  m_samplerState;
		Windows::Graphics::Display::Core::HdmiDisplayMode^ m_lastDisplayMode;
		Windows::Graphics::Display::Core::HdmiDisplayMode^ m_currentDisplayMode;

		// Variables used with the rendering loop.
		DXGI_HDR_METADATA_HDR10 m_lastHdr10;
		std::atomic<bool> m_loadingComplete;
	    std::atomic<bool> m_loadingSuccessful;
		MoonlightClient *client;
		StreamConfiguration^ configuration;
		bool m_NeedsRefresh = false;

		DECODER_PARAMETERS m_DecoderParams{};
		UINT m_TextureWidth;
		UINT m_TextureHeight;
		int m_DisplayWidth;
		int m_DisplayHeight;

		// Properties watched by hasFrameFormatChanged()
		int m_LastFrameWidth = 0;
		int m_LastFrameHeight = 0;
		AVPixelFormat m_LastFramePixelFormat = AV_PIX_FMT_NONE;
		AVColorRange m_LastColorRange = AVCOL_RANGE_UNSPECIFIED;
		AVColorPrimaries m_LastColorPrimaries = AVCOL_PRI_UNSPECIFIED;
		AVColorTransferCharacteristic m_LastColorTrc = AVCOL_TRC_UNSPECIFIED;
		AVColorSpace m_LastColorSpace = AVCOL_SPC_UNSPECIFIED;
		AVChromaLocation m_LastChromaLocation = AVCHROMA_LOC_UNSPECIFIED;

		// Cache of SRVs over the ffmpeg decoder's array texture(s). Keyed by the
		// underlying ID3D11Texture2D*; the inner vector is indexed by array slice,
		// each holding the (luma, chroma) pair.
		std::unordered_map<ID3D11Texture2D*,
			std::vector<std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2>>> m_DirectSampleSrvs;
	};
}

