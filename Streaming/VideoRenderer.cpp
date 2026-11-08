#include "pch.h"
#include "VideoRenderer.h"
#include "Pacer.h"
#include <State\MoonlightClient.h>
#include "PyroWave\FramePool.h"
#include "..\Common\DirectXHelper.h"
#include <Utils.hpp>
#include "..\Common\ModalDialog.xaml.h"
#include "..\Plot\ImGuiPlots.h"

#include <d3d11shader.h>
#include <d3dcompiler.h>

extern "C" {
#include "libgamestream/errors.h"
#include "libgamestream/client.h"
#include <Limelight.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
}

using Microsoft::WRL::ComPtr;
using namespace moonlight_xbox_dx;
using namespace DirectX;
using namespace Windows::Foundation;

typedef struct _VERTEX
{
	float x, y;
	float tu, tv;
} VERTEX, * PVERTEX;

#define CSC_MATRIX_RAW_ELEMENT_COUNT 9
#define CSC_MATRIX_PACKED_ELEMENT_COUNT 12
#define OFFSETS_ELEMENT_COUNT 3

typedef struct _CSC_CONST_BUF
{
	// CscMatrix value from above but packed and scaled
	float cscMatrix[CSC_MATRIX_PACKED_ELEMENT_COUNT];

	// YUV offset values
	float offsets[OFFSETS_ELEMENT_COUNT];

	// Padding float to end 16-byte boundary
	float padding;

	// Chroma offset values
	float chromaOffset[2];

	// Max UV coordinates to avoid sampling alignment padding
	float chromaUVMax[2];
} CSC_CONST_BUF, * PCSC_CONST_BUF;
static_assert(sizeof(CSC_CONST_BUF) % 16 == 0, "Constant buffer sizes must be a multiple of 16");

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
VideoRenderer::VideoRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, MoonlightClient* mclient, StreamConfiguration^ sConfig) :
	m_LastColorTrc(AVCOL_TRC_UNSPECIFIED),
	m_loadingComplete(false),
	m_loadingSuccessful(false),
	m_deviceResources(deviceResources),
	client(mclient),
	configuration(sConfig)
{
	ZeroMemory(&m_lastHdr10, sizeof(DXGI_HDR_METADATA_HDR10));

	m_DecoderParams.width = configuration->width;
	m_DecoderParams.height = configuration->height;
	m_DecoderParams.frameRate = configuration->FPS;

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Initializes view parameters when the window size changes.
void VideoRenderer::CreateWindowSizeDependentResources()
{
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void VideoRenderer::Update(DX::StepTimer const& timer)
{

}

static inline std::vector<DXGI_FORMAT> getPlaneSRVFormats(DXGI_FORMAT fmt)
{
	if (fmt == DXGI_FORMAT_P010) {
		return { DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16G16_UNORM };
	}
	else {
		return { DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8G8_UNORM };
	}
}

bool renderedOneFrame = false;
// Renders one frame using the vertex and pixel shaders.
bool VideoRenderer::Render(AVFrame *frame) {
	ZoneScopedN("VideoRender");
	TracyD3D11Zone(g_tracyCtx11, "VideoRender");
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete.load(std::memory_order_acquire) && !m_loadingSuccessful.load(std::memory_order_acquire)) {
		return true;
	}
	if (Utils::IsWindowsPhone()) {
		return RenderW10M(frame);
	}
	return RenderDesktop(frame);
}

// W10M (FL 9_3): sample the flat NV12 copy the decode thread produced, with
// level_9_3 shaders. Self-contained: no PyroWave, no HDR/color_trc handling.
bool VideoRenderer::RenderW10M(AVFrame *frame) {
	auto *ctx = m_deviceResources->GetD3DDeviceContext();

	// Clear the back buffer
	ID3D11RenderTargetView* renderTarget[] = { m_deviceResources->GetBackBufferRenderTargetView() };
	ctx->ClearRenderTargetView(renderTarget[0], Colors::Black);

	// Bind the back buffer. This needs to be done each time,
	// because the render target view will be unbound by Present().
	ctx->OMSetRenderTargets(1, renderTarget, nullptr);

	ID3D11Texture2D *frameTexture = (ID3D11Texture2D *)(frame->data[0]);
	if (!frameTexture) {
		// This sometimes happens when reconnecting
		return false;
	}
	D3D11_TEXTURE2D_DESC texDesc;
	frameTexture->GetDesc(&texDesc);

	// Setup shader
	ctx->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->IASetInputLayout(m_inputLayout.Get());
	ctx->VSSetShader(m_vertexShader.Get(), nullptr, 0);
	ctx->PSSetShader(m_pixelShaderYUV420_93.Get(), nullptr, 0);

	if (hasFrameFormatChanged(frame)) {
		// Use the frame's actual dimensions, not m_DecoderParams: the stream
		// resolution (1280x720 on W10M) can differ from the UI configuration.
		float uMax = texDesc.Width > 0 ? (float)frame->width / texDesc.Width : 1.0f;
		float vMax = texDesc.Height > 0 ? (float)frame->height / texDesc.Height : 1.0f;
		setupVertexBuffer(texDesc, uMax, vMax);
		bindColorConversion(frame, texDesc);
	}
	else {
		// Track display size changes (e.g. the forced landscape rotation on
		// W10M) so the letterboxing follows the swap chain.
		auto logicalSize = m_deviceResources->GetLogicalSize();
		if ((int)logicalSize.Width != m_DisplayWidth || (int)logicalSize.Height != m_DisplayHeight) {
			m_DisplayWidth = (int)logicalSize.Width;
			m_DisplayHeight = (int)logicalSize.Height;
			float uMax = texDesc.Width > 0 ? (float)frame->width / texDesc.Width : 1.0f;
			float vMax = texDesc.Height > 0 ? (float)frame->height / texDesc.Height : 1.0f;
			setupVertexBuffer(texDesc, uMax, vMax);
		}
	}

	UINT stride = sizeof(VERTEX);
	UINT offset = 0;
	ctx->IASetVertexBuffers(0, 1, m_VideoVertexBuffer.GetAddressOf(), &stride, &offset);
	ctx->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	const std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2>* frameSrvPair =
		getFlatSrvs(frameTexture);
	if (!frameSrvPair) {
		// SRV creation failed; nothing we can render this frame
		return false;
	}
	ID3D11ShaderResourceView *frameSrvs[] = { (*frameSrvPair)[0].Get(), (*frameSrvPair)[1].Get() };
	ctx->PSSetShaderResources(0, 2, frameSrvs);
	ctx->PSSetConstantBuffers(0, 1, m_cscConstantBuffer.GetAddressOf());
	ctx->DrawIndexed(6, 0, 0);

	// Unbind SRVs for this frame
	ID3D11ShaderResourceView* nullSrvs[3] = {};
	ctx->PSSetShaderResources(0, 3, nullSrvs);
	return true;
}

// Desktop (Xbox/PC) pipeline: direct sampling of decoder array textures,
// PyroWave support and HDR color handling.
bool VideoRenderer::RenderDesktop(AVFrame *frame) {
	auto *ctx = m_deviceResources->GetD3DDeviceContext();

	// Clear the back buffer
	ID3D11RenderTargetView* renderTarget[] = { m_deviceResources->GetBackBufferRenderTargetView() };
	ctx->ClearRenderTargetView(renderTarget[0], Colors::Black);

	// Bind the back buffer. This needs to be done each time,
	// because the render target view will be unbound by Present().
	ctx->OMSetRenderTargets(1, renderTarget, nullptr);

	// AV_PIX_FMT_YUV444P16 / YUV420P16 are the PyroWaveFramePool sentinels:
	// data[0..2] are our own R16_UNORM plane textures, data[3] the PyroWaveFrameSet.
	const bool isPyroWave = frame->format == AV_PIX_FMT_YUV444P16 ||
	                        frame->format == AV_PIX_FMT_YUV420P16;

	ID3D11Texture2D *ffmpegTexture = (ID3D11Texture2D *)(frame->data[0]);
	if (!ffmpegTexture) {
		// This sometimes happens when reconnecting
		return false;
	}
	D3D11_TEXTURE2D_DESC ffmpegDesc;
	ffmpegTexture->GetDesc(&ffmpegDesc);

	// Setup shader
	ctx->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->IASetInputLayout(m_inputLayout.Get());
	ctx->VSSetShader(m_vertexShader.Get(), nullptr, 0);
	ctx->PSSetShader(isPyroWave ? m_pixelShaderPyroWave.Get() : m_pixelShaderYUV420Array.Get(), nullptr, 0);

	bool hasChanged = hasFrameFormatChanged(frame);
	if (hasChanged) {
		// PyroWave planes are exactly frame-sized (no decoder padding), so
		// the full texture is sampled; ffmpeg textures can be padded.
		float uMax = isPyroWave ? 1.0f : (ffmpegDesc.Width > 0 ? (float)m_DecoderParams.width / ffmpegDesc.Width : 1.0f);
		float vMax = isPyroWave ? 1.0f : (ffmpegDesc.Height > 0 ? (float)m_DecoderParams.height / ffmpegDesc.Height : 1.0f);
		setupVertexBuffer(ffmpegDesc, uMax, vMax);
		bindColorConversion(frame, ffmpegDesc);
	}

	UINT stride = sizeof(VERTEX);
	UINT offset = 0;
	ctx->IASetVertexBuffers(0, 1, m_VideoVertexBuffer.GetAddressOf(), &stride, &offset);
	ctx->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Bind SRVs for this frame and draw
	if (isPyroWave) {
		// PyroWave planes are sampled in place: the decoder queues a GPU wait
		// for its decode on the immediate context, so the decode is ordered
		// before this draw, and the pool recycles sets only via av_frame_free.
		auto *set = reinterpret_cast<PyroWaveFrameSet *>(frame->data[3]);
		ID3D11ShaderResourceView *frameSrvs[] = { set->srv[0].Get(), set->srv[1].Get(), set->srv[2].Get() };
		ctx->PSSetShaderResources(0, 3, frameSrvs);
	} else {
		// Sample the decoder's array texture straight into the YUV->RGB shader.
		// frame->data[1] is the slice of the decoder's array texture holding this frame.
		UINT slice = (UINT)(intptr_t)frame->data[1];
		const std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2>* frameSrvPair =
			getDirectSampleSrvs(ffmpegTexture, slice, ffmpegDesc);
		if (!frameSrvPair) {
			// SRV creation failed; nothing we can render this frame
			return false;
		}

		// Bind SRVs for this frame
		ID3D11ShaderResourceView* frameSrvs[] = { (*frameSrvPair)[0].Get(), (*frameSrvPair)[1].Get() };
		ctx->PSSetShaderResources(0, 2, frameSrvs);
	}
	ctx->PSSetConstantBuffers(0, 1, m_cscConstantBuffer.GetAddressOf());

	// Draw the video
	ctx->DrawIndexed(6, 0, 0);

	// Unbind SRVs for this frame
	ID3D11ShaderResourceView* nullSrvs[3] = {};
	ctx->PSSetShaderResources(0, 3, nullSrvs);

	if (frame->color_trc != m_LastColorTrc || m_NeedsRefresh) {
		DXGI_COLOR_SPACE_TYPE colorspace = {};

		if (frame->color_trc == AVCOL_TRC_SMPTE2084) {
			// Switch to Rec 2020 PQ (SMPTE ST 2084) colorspace for HDR10 rendering
			colorspace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
		} else {
			// Restore default sRGB colorspace
			colorspace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
		}

		UINT colorSpaceSupport = 0;
		// SetColorSpace1/CheckColorSpaceSupport are IDXGISwapChain4-only; QI locally
		// and skip HDR silently on DXGI 1.4-era systems (e.g. W10M).
		Microsoft::WRL::ComPtr<IDXGISwapChain4> sc4;
		if (colorspace && SUCCEEDED(m_deviceResources->GetSwapChain()->QueryInterface(IID_PPV_ARGS(&sc4)))
			&& SUCCEEDED(sc4->CheckColorSpaceSupport(colorspace, &colorSpaceSupport)) && (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
			DX::ThrowIfFailed(sc4->SetColorSpace1(colorspace));
			Utils::Logf("Colorspace changed to %s\n",
			            colorspace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
			                ? "DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020"
			                : "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709");
		}

		m_LastColorTrc = frame->color_trc;

		if (m_NeedsRefresh) {
			// Ensure HDR is set correctly after an HDMI change event
			m_NeedsRefresh = false;
			SetHDR(frame->color_trc == AVCOL_TRC_SMPTE2084);
		}
	}

	return true;
}

void VideoRenderer::CreateDeviceDependentResources()
{
	Utils::Log("Started with creation of DXView\n");

	if (Utils::IsWindowsPhone()) {
		CreateDeviceDependentResourcesW10M();
		return;
	}
	CreateDeviceDependentResourcesDesktop();
}

// W10M (FL 9_3) path. Fully self-contained: no HDMI/HDR APIs, no PyroWave,
// no VideoProcessor; its own level_9_3 shader bytecode and exactly one
// StartStreaming dispatch. Nothing in here may be shared with the desktop
// path - fall-through sharing caused double-init crashes.
void VideoRenderer::CreateDeviceDependentResourcesW10M()
{
	Utils::Log("VideoRenderer: W10M 9_3 shader sampling path\n");

	auto w = CoreWindow::GetForCurrentThread();
	m_DisplayWidth = (int)w->Bounds.Width;
	m_DisplayHeight = (int)w->Bounds.Height;

	// Vertex shader (level_9_3 bytecode, built with fxc)
	{
		auto vertexShaderBytecode = DX::ReadData(L"Assets\\Shader\\d3d11_vertex_93.fxc");
		DX::ThrowIfFailed(
		    m_deviceResources->GetD3DDevice()->CreateVertexShader(
		        vertexShaderBytecode.data(),
		        vertexShaderBytecode.size(),
		        nullptr,
				&m_vertexShader
			)
			, "Vertex Shader Creation");

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
		    {
				{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		    };

		DX::ThrowIfFailed(
		    m_deviceResources->GetD3DDevice()->CreateInputLayout(
		        vertexDesc,
		        ARRAYSIZE(vertexDesc),
		        vertexShaderBytecode.data(),
		        vertexShaderBytecode.size(),
				&m_inputLayout
			)
			, "Input Layout Creation");
	}

	// Pixel shader (level_9_3 bytecode, samples the flat NV12 planes as Texture2D)
	{
		auto pixelShaderBytecode = DX::ReadData(L"Assets\\Shader\\d3d11_yuv420_pixel_93.fxc");
		DX::ThrowIfFailed(
		    m_deviceResources->GetD3DDevice()->CreatePixelShader(
		        pixelShaderBytecode.data(),
		        pixelShaderBytecode.size(),
		        nullptr,
				&m_pixelShaderYUV420_93
			)
			, "Pixel Shader Creation (9_3)");
	}

	// NOTE: intentionally NO HDMI/HDR setup, no PyroWave shader, no
	// VideoDevice/VideoProcessor here - those are desktop-only device-state
	// interfaces and must never run on the phone path.

	// We use a common sampler for all pixel shaders
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateSamplerState(&samplerDesc,  &m_samplerState));
	}

	// We use a common index buffer for all geometry
	{
		const int indexes[] = { 0, 1, 2, 3, 2, 1 };
		D3D11_BUFFER_DESC indexBufferDesc = {};
		indexBufferDesc.ByteWidth = sizeof(indexes);
		indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.CPUAccessFlags = 0;
		indexBufferDesc.MiscFlags = 0;
		indexBufferDesc.StructureByteStride = sizeof(int);

		D3D11_SUBRESOURCE_DATA indexBufferData = {};
		indexBufferData.pSysMem = indexes;
		indexBufferData.SysMemPitch = sizeof(int);

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_indexBuffer), "Index Buffer creation");
	}

	DISPATCH_THREADPOOL(([this, devRes = m_deviceResources, cfg = configuration] {
		int status = this->client->StartStreaming(devRes, cfg);

		if (status != 0) {
			Utils::Logf("StartStreaming failed with status %d\n", status);
			m_loadingSuccessful.store(false, std::memory_order_release);
			m_loadingComplete.store(true, std::memory_order_release);
			return;
		}

        m_loadingSuccessful.store(true, std::memory_order_release);
        m_loadingComplete.store(true, std::memory_order_release);
        Utils::Log("Loading Complete!\n");
    }));
}

// Desktop path (Xbox/PC): full-featured pipeline with SM5 shaders, HDMI/HDR
// and the PyroWave codec. Own StartStreaming dispatch.
void VideoRenderer::CreateDeviceDependentResourcesDesktop()
{
	// Vertex shader
	{
		auto vertexShaderBytecode = DX::ReadData(L"Assets\\Shader\\d3d11_vertex.fxc");
		DX::ThrowIfFailed(
		    m_deviceResources->GetD3DDevice()->CreateVertexShader(
		        vertexShaderBytecode.data(),
		        vertexShaderBytecode.size(),
		        nullptr,
				&m_vertexShader
			)
			, "Vertex Shader Creation");

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
		    {
				{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		    };

		DX::ThrowIfFailed(
		    m_deviceResources->GetD3DDevice()->CreateInputLayout(
		        vertexDesc,
		        ARRAYSIZE(vertexDesc),
		        vertexShaderBytecode.data(),
		        vertexShaderBytecode.size(),
				&m_inputLayout
			)
			, "Input Layout Creation");
	}

	// Texture2DArray pixel shader, samples the decoder's output surfaces directly
	{
		auto pixelShaderBytecode = DX::ReadData(L"Assets\\Shader\\d3d11_yuv420_pixel_array.fxc");
		DX::ThrowIfFailed(
		    m_deviceResources->GetD3DDevice()->CreatePixelShader(
		        pixelShaderBytecode.data(),
		        pixelShaderBytecode.size(),
		        nullptr,
				&m_pixelShaderYUV420Array
			)
			, "Pixel Shader Creation");
	}

	// Three-plane pixel shader (PyroWave path, 4:4:4 and 4:2:0)
	{
		auto pixelShaderBytecode = DX::ReadData(L"Assets\\Shader\\d3d11_pyrowave_pixel.fxc");
		DX::ThrowIfFailed(
		    m_deviceResources->GetD3DDevice()->CreatePixelShader(
		        pixelShaderBytecode.data(),
		        pixelShaderBytecode.size(),
		        nullptr,
		        &m_pixelShaderPyroWave
			)
			, "PyroWave Pixel Shader Creation");
	}

	Windows::Graphics::Display::Core::HdmiDisplayInformation^ hdi = nullptr;
	try {
		hdi = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
	}
	catch (...) {
		Utils::Log("HdmiDisplayInformation unavailable\n");
	}
	auto w = CoreWindow::GetForCurrentThread();
	m_DisplayWidth = (int)w->Bounds.Width;
	m_DisplayHeight = (int)w->Bounds.Height;
	// HDR Setup
	if (hdi) {
		m_lastDisplayMode = hdi->GetCurrentDisplayMode();
		m_currentDisplayMode = m_lastDisplayMode;
		// Scale video to the window size while preserving aspect ratio
		m_DisplayWidth = std::max(m_currentDisplayMode->ResolutionWidthInRawPixels, (UINT)1920);
		m_DisplayHeight = std::max(m_currentDisplayMode->ResolutionHeightInRawPixels, (UINT)1080);
	}

	// We use a common sampler for all pixel shaders
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateSamplerState(&samplerDesc,  &m_samplerState));
	}

	// We use a common index buffer for all geometry
	{
		const int indexes[] = { 0, 1, 2, 3, 2, 1 };
		D3D11_BUFFER_DESC indexBufferDesc = {};
		indexBufferDesc.ByteWidth = sizeof(indexes);
		indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.CPUAccessFlags = 0;
		indexBufferDesc.MiscFlags = 0;
		indexBufferDesc.StructureByteStride = sizeof(int);

		D3D11_SUBRESOURCE_DATA indexBufferData = {};
		indexBufferData.pSysMem = indexes;
		indexBufferData.SysMemPitch = sizeof(int);

		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_indexBuffer), "Index Buffer creation");
	}

	DISPATCH_THREADPOOL(([this, devRes = m_deviceResources, cfg = configuration] {
		int status = this->client->StartStreaming(devRes, cfg);

		if (status != 0) {
			Utils::Logf("StartStreaming failed with status %d\n", status);
			m_loadingSuccessful.store(false, std::memory_order_release);
			m_loadingComplete.store(true, std::memory_order_release);
			return;
		}

		m_loadingSuccessful.store(true, std::memory_order_release);
		m_loadingComplete.store(true, std::memory_order_release);
		Utils::Log("Loading Complete!\n");
	}));
}

void VideoRenderer::ReleaseDeviceDependentResources()
{
	if (!Utils::IsWindowsPhone()) {
		Windows::Graphics::Display::Core::HdmiDisplayInformation^ hdi = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
	}
	m_loadingComplete.store(false, std::memory_order_release);
	m_loadingSuccessful.store(false, std::memory_order_release);
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShaderYUV420Array.Reset();
	m_pixelShaderYUV420_93.Reset();
	m_FlatSrvs.clear();
	m_VPInputViews.clear();
	m_pixelShaderPyroWave.Reset();
	m_cscConstantBuffer.Reset();
	m_VideoVertexBuffer.Reset();
	m_samplerState.Reset();
	m_indexBuffer.Reset();
	m_videoDevice.Reset();
	m_videoContext.Reset();
	m_videoProcessor.Reset();
	m_vpEnumerator.Reset();
	m_vpOutputView.Reset();
	m_vpMiddleTex.Reset();
	m_vpBackBufferTex.Reset();
	m_vpFlatNV12.Reset();
	m_vpOutWidth = 0;
	m_vpOutHeight = 0;

	// Drop SRVs over decoder surfaces; the pool is owned by ffmpeg and is going away.
	m_DirectSampleSrvs.clear();
	m_VPInputViews.clear();
}

void VideoRenderer::scaleSourceToDestinationSurface(IRECT* src, IRECT* dst)
{
	int dstH = (int)ceil((float)dst->w * src->h / src->w);
	int dstW = (int)ceil((float)dst->h * src->w / src->h);

	if (dstH > dst->h) {
		dst->x += (dst->w - dstW) / 2;
		dst->w = dstW;
	}
	else {
		dst->y += (dst->h - dstH) / 2;
		dst->h = dstH;
	}
}

void VideoRenderer::screenSpaceToNormalizedDeviceCoords(IRECT* src, FRECT* dst, int viewportWidth, int viewportHeight)
{
	dst->x = ((float)src->x / (viewportWidth / 2.0f)) - 1.0f;
	dst->y = ((float)src->y / (viewportHeight / 2.0f)) - 1.0f;
	dst->w = (float)src->w / (viewportWidth / 2.0f);
	dst->h = (float)src->h / (viewportHeight / 2.0f);
}

const std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2>*
VideoRenderer::getDirectSampleSrvs(ID3D11Texture2D* texture, UINT slice, const D3D11_TEXTURE2D_DESC& desc)
{
	auto it = m_DirectSampleSrvs.find(texture);
	if (it == m_DirectSampleSrvs.end()) {
		// Build the (luma, chroma) SRV pair for every slice of this array texture once.
		std::vector<std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2>> slices(desc.ArraySize);
		auto formats = getPlaneSRVFormats(desc.Format);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.ArraySize = 1;

		auto* dev = m_deviceResources->GetD3DDevice();
		for (UINT s = 0; s < desc.ArraySize; s++) {
			srvDesc.Texture2DArray.FirstArraySlice = s;
			for (size_t plane = 0; plane < formats.size() && plane < 2; plane++) {
				srvDesc.Format = formats[plane];
				HRESULT hr = dev->CreateShaderResourceView(texture, &srvDesc, &slices[s][plane]);
				if (FAILED(hr)) {
					Utils::Logf("Direct sampling SRV creation failed (slice %u, plane %zu, 0x%08X)\n",
					            s, plane, (unsigned)hr);
					return nullptr;
				}
			}
		}
		it = m_DirectSampleSrvs.emplace(texture, std::move(slices)).first;
	}

	if (slice >= it->second.size()) {
		// Out of range slice index; should never happen, but stay safe.
		return nullptr;
	}
	return &it->second[slice];
}

// W10M: SRV pair for a plain (non-array) NV12 texture - the format itself
// selects the plane (R8_UNORM = luma, R8G8_UNORM = chroma).
const std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2>*
VideoRenderer::getFlatSrvs(ID3D11Texture2D* texture)
{
	auto it = m_FlatSrvs.find(texture);
	if (it == m_FlatSrvs.end()) {
		std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2> srvs;
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		srvDesc.Format = DXGI_FORMAT_R8_UNORM; // NV12 luma plane
		HRESULT hr = m_deviceResources->GetD3DDevice()->CreateShaderResourceView(texture, &srvDesc, &srvs[0]);
		if (FAILED(hr)) {
			Utils::Logf("Flat SRV (Y) creation failed 0x%08X\n", (unsigned)hr);
			return nullptr;
		}
		srvDesc.Format = DXGI_FORMAT_R8G8_UNORM; // NV12 chroma plane
		hr = m_deviceResources->GetD3DDevice()->CreateShaderResourceView(texture, &srvDesc, &srvs[1]);
		if (FAILED(hr)) {
			Utils::Logf("Flat SRV (UV) creation failed 0x%08X\n", (unsigned)hr);
			return nullptr;
		}
		it = m_FlatSrvs.emplace(texture, srvs).first;
	}
	return &it->second;
}

// Create our fixed vertex buffer for video rendering
void VideoRenderer::setupVertexBuffer(D3D11_TEXTURE2D_DESC frameDesc, float uMax, float vMax)
{
	// Scale video to the window size while preserving aspect ratio
	IRECT src, dst;
	src.x = src.y = 0;
	src.w = configuration->width;
	src.h = configuration->height;
	dst.x = dst.y = 0;
	dst.w = m_DisplayWidth;
	dst.h = m_DisplayHeight;
	scaleSourceToDestinationSurface(&src, &dst);

	// Convert screen space to normalized device coordinates
	FRECT renderRect;
	screenSpaceToNormalizedDeviceCoords(&dst, &renderRect, m_DisplayWidth, m_DisplayHeight);

	m_TextureWidth = frameDesc.Width;
	m_TextureHeight = frameDesc.Height;
	assert(m_TextureWidth > 0 && m_TextureHeight > 0);

	Utils::Logf("Setup vertex shader params: uMax %f, vMax %f\n", uMax, vMax);

	VERTEX verts[] =
	{
		{renderRect.x, renderRect.y, 0, vMax},
		{renderRect.x, renderRect.y + renderRect.h, 0, 0},
		{renderRect.x + renderRect.w, renderRect.y, uMax, vMax},
		{renderRect.x + renderRect.w, renderRect.y + renderRect.h, uMax, 0},
	};

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = sizeof(verts);
	vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = 0;
	vbDesc.MiscFlags = 0;
	vbDesc.StructureByteStride = sizeof(VERTEX);

	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = verts;
	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreateBuffer(
			&vbDesc,
			&vbData,
			&m_VideoVertexBuffer
		)
		, "Vertex Buffer Creation");
}


static inline bool isFrameFullRange(const AVFrame* frame) {
	// This handles the case where the color range is unknown,
	// so that we use Limited color range which is the default
	// behavior for Moonlight.
	return frame->color_range == AVCOL_RANGE_JPEG;
}

static inline int getFrameColorspace(const AVFrame* frame) {
	// Prefer the colorspace field on the AVFrame itself
	switch (frame->colorspace) {
	case AVCOL_SPC_SMPTE170M:
	case AVCOL_SPC_BT470BG:
		return COLORSPACE_REC_601;
	case AVCOL_SPC_BT709:
		return COLORSPACE_REC_709;
	case AVCOL_SPC_BT2020_NCL:
	case AVCOL_SPC_BT2020_CL:
		return COLORSPACE_REC_2020;
	default:
		// If the colorspace is not populated, assume the encoder
		// is sending the colorspace that we requested.
		return COLORSPACE_REC_601;
	}
}

static inline AVPixelFormat getFrameSwPixelFormat(const AVFrame* frame) {
	// For hwaccel formats, we want to get the real underlying format
	if (frame->hw_frames_ctx) {
		return ((AVHWFramesContext*)frame->hw_frames_ctx->data)->sw_format;
	}
	else {
		return (AVPixelFormat)frame->format;
	}
}

static inline int getFrameBitsPerChannel(const AVFrame* frame) {
	const AVPixFmtDescriptor* formatDesc = av_pix_fmt_desc_get(getFrameSwPixelFormat(frame));
	if (!formatDesc) {
		// This shouldn't be possible but handle it anyway
		assert(formatDesc);
		return 8;
	}

	// This assumes plane 0 is exclusively the Y component
	return formatDesc->comp[0].depth;
}

void VideoRenderer::getFramePremultipliedCscConstants(const AVFrame* frame, std::array<float, 9> &cscMatrix, std::array<float, 3> &offsets) {
	static const std::array<float, 9> k_CscMatrix_Bt601 = {
		1.0f, 1.0f, 1.0f,
		0.0f, -0.3441f, 1.7720f,
		1.4020f, -0.7141f, 0.0f,
	};
	static const std::array<float, 9> k_CscMatrix_Bt709 = {
		1.0f, 1.0f, 1.0f,
		0.0f, -0.1873f, 1.8556f,
		1.5748f, -0.4681f, 0.0f,
	};
	static const std::array<float, 9> k_CscMatrix_Bt2020 = {
		1.0f, 1.0f, 1.0f,
		0.0f, -0.1646f, 1.8814f,
		1.4746f, -0.5714f, 0.0f,
	};

	bool fullRange = isFrameFullRange(frame);
	int bitsPerChannel = getFrameBitsPerChannel(frame);
	int channelRange = (1 << bitsPerChannel);
	double yMin = (fullRange ? 0 : (16 << (bitsPerChannel - 8)));
	double yMax = (fullRange ? (channelRange - 1) : (235 << (bitsPerChannel - 8)));
	double yScale = (channelRange - 1) / (yMax - yMin);
	double uvMin = (fullRange ? 0 : (16 << (bitsPerChannel - 8)));
	double uvMax = (fullRange ? (channelRange - 1) : (240 << (bitsPerChannel - 8)));
	double uvScale = (channelRange - 1) / (uvMax - uvMin);

	// Calculate YUV offsets
	offsets[0] = yMin / (double)(channelRange - 1);
	offsets[1] = (channelRange / 2) / (double)(channelRange - 1);
	offsets[2] = (channelRange / 2) / (double)(channelRange - 1);

	// Start with the standard full range color matrix
	int colorspace = getFrameColorspace(frame);
	switch (colorspace) {
	default:
	case COLORSPACE_REC_601:
		cscMatrix = k_CscMatrix_Bt601;
		break;
	case COLORSPACE_REC_709:
		cscMatrix = k_CscMatrix_Bt709;
		break;
	case COLORSPACE_REC_2020:
		cscMatrix = k_CscMatrix_Bt2020;
		break;
	}

	// Scale the color matrix according to the color range
	for (int i = 0; i < 3; i++) {
		cscMatrix[i] *= yScale;
	}
	for (int i = 3; i < 9; i++) {
		cscMatrix[i] *= uvScale;
	}

	Utils::Logf("Shader config: %s %d-bit %s, (AVColorSpace %d, AVChromaLocation %d)\n",
	            colorspace == COLORSPACE_REC_601   ? "Rec. 601"
	            : colorspace == COLORSPACE_REC_709 ? "Rec. 709"
	                                               : "Rec. 2020",
	            bitsPerChannel,
	            fullRange ? "full range" : "limited/standard range",
				frame->colorspace,
	            frame->chroma_location);
}

void VideoRenderer::getFrameChromaCositingOffsets(const AVFrame* frame, std::array<float, 2> &chromaOffsets) {
	const AVPixFmtDescriptor* formatDesc = av_pix_fmt_desc_get(getFrameSwPixelFormat(frame));
	if (!formatDesc) {
		assert(formatDesc);
		chromaOffsets.fill(0);
		return;
	}

	assert(formatDesc->log2_chroma_w <= 1);
	assert(formatDesc->log2_chroma_h <= 1);

	switch (frame->chroma_location) {
	default:
	case AVCHROMA_LOC_LEFT:
		chromaOffsets[0] = 0.5;
		chromaOffsets[1] = 0;
		break;
	case AVCHROMA_LOC_CENTER:
		chromaOffsets[0] = 0;
		chromaOffsets[1] = 0;
		break;
	case AVCHROMA_LOC_TOPLEFT:
		chromaOffsets[0] = 0.5;
		chromaOffsets[1] = 0.5;
		break;
	case AVCHROMA_LOC_TOP:
		chromaOffsets[0] = 0;
		chromaOffsets[1] = 0.5;
		break;
	case AVCHROMA_LOC_BOTTOMLEFT:
		chromaOffsets[0] = 0.5;
		chromaOffsets[1] = -0.5;
		break;
	case AVCHROMA_LOC_BOTTOM:
		chromaOffsets[0] = 0;
		chromaOffsets[1] = -0.5;
		break;
	}

	// Force the offsets to 0 if chroma is not subsampled in that dimension
	if (formatDesc->log2_chroma_w == 0) {
		chromaOffsets[0] = 0;
	}
	if (formatDesc->log2_chroma_h == 0) {
		chromaOffsets[1] = 0;
	}
}

// Returns if the frame format has changed since the last call to this function
bool VideoRenderer::hasFrameFormatChanged(const AVFrame* frame) {
	AVPixelFormat format = getFrameSwPixelFormat(frame);
	if (frame->width == m_LastFrameWidth &&
		frame->height == m_LastFrameHeight &&
		format == m_LastFramePixelFormat &&
		frame->color_range == m_LastColorRange &&
		frame->color_primaries == m_LastColorPrimaries &&
		frame->colorspace == m_LastColorSpace &&
		frame->chroma_location == m_LastChromaLocation) {
		return false;
	}

	// m_LastColorTrc is checked elsewhere for calling SetColorSpace1()

	m_LastFrameWidth = frame->width;
	m_LastFrameHeight = frame->height;
	m_LastFramePixelFormat = format;
	m_LastColorRange = frame->color_range;
	m_LastColorPrimaries = frame->color_primaries;
	m_LastColorSpace = frame->colorspace;
	m_LastChromaLocation = frame->chroma_location;
	return true;
}

void VideoRenderer::bindColorConversion(AVFrame* frame, D3D11_TEXTURE2D_DESC frameDesc)
{
	D3D11_BUFFER_DESC constDesc = {};
	constDesc.ByteWidth = sizeof(CSC_CONST_BUF);
	constDesc.Usage = D3D11_USAGE_IMMUTABLE;
	constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constDesc.CPUAccessFlags = 0;
	constDesc.MiscFlags = 0;

	CSC_CONST_BUF constBuf = {};
	std::array<float, 9> cscMatrix;
	std::array<float, 3> yuvOffsets;
	getFramePremultipliedCscConstants(frame, cscMatrix, yuvOffsets);

	std::copy(yuvOffsets.cbegin(), yuvOffsets.cend(), constBuf.offsets);

	// We need to adjust our CSC matrix to be column-major and with float3 vectors
	// padded with a float in between each of them to adhere to HLSL requirements.
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			constBuf.cscMatrix[i * 4 + j] = cscMatrix[j * 3 + i];
		}
	}

	m_TextureWidth = frameDesc.Width;
	m_TextureHeight = frameDesc.Height;
	assert(m_TextureWidth > 0 && m_TextureHeight > 0);

	std::array<float, 2> chromaOffset;
	getFrameChromaCositingOffsets(frame, chromaOffset);
	constBuf.chromaOffset[0] = chromaOffset[0] / m_TextureWidth;
	constBuf.chromaOffset[1] = chromaOffset[1] / m_TextureHeight;

	// Limit chroma texcoords to avoid sampling from alignment texels.
	// PyroWave planes (YUV444P16/YUV420P16 sentinels) are exactly
	// frame-sized, so the full texture is always valid.
	// Use the frame's actual dimensions (the stream resolution can differ
	// from the UI configuration, e.g. 1280x720 forced on W10M).
	bool unpaddedPlanes = frame->format == AV_PIX_FMT_YUV444P16 ||
	                      frame->format == AV_PIX_FMT_YUV420P16;
	constBuf.chromaUVMax[0] = (unpaddedPlanes || frame->width == (int)m_TextureWidth) ?
	                              1.0f : ((float)(frame->width - 1) / m_TextureWidth);
	constBuf.chromaUVMax[1] = (unpaddedPlanes || frame->height == (int)m_TextureHeight) ?
	                              1.0f : ((float)(frame->height - 1) / m_TextureHeight);

	D3D11_SUBRESOURCE_DATA constData = {};
	constData.pSysMem = &constBuf;

	Utils::Logf("Setup pixel shader params: chromaOffset[0] %f, chromaOffset[1] %f, chromaUVMax[0] %f, chromaUVMax[1] %f\n",
				constBuf.chromaOffset[0], constBuf.chromaOffset[1],
				constBuf.chromaUVMax[0], constBuf.chromaUVMax[1]);

	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&constDesc, &constData, &m_cscConstantBuffer));
}

void VideoRenderer::RequestRefreshColorspace()
{
	m_NeedsRefresh = true;
}

void VideoRenderer::SetHDR(bool enabled)
{
	if (enabled) {
		SS_HDR_METADATA sunshineHdrMetadata;

		// Sunshine will have HDR metadata but GFE will not
		if (!LiGetHdrMetadata(&sunshineHdrMetadata)) {
			RtlZeroMemory(&sunshineHdrMetadata, sizeof(sunshineHdrMetadata));
		}

		// Ask the display to switch to HDR and set the metadata from Sunshine
		client->SetDisplayHDR(true, sunshineHdrMetadata);
	}
	else {
		// toggle the display to the correct state
		client->SetDisplayHDR(false, SS_HDR_METADATA{});
	}
}

void VideoRenderer::Stop() {
	// nothing to do
}

// ---- W10M FL 9_3 path -----------------------------------------------

// Cached VP input view over one decoded array-texture slice.
Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> VideoRenderer::getInputView(ID3D11Texture2D* texture, UINT slice)
{
	auto& slices = m_VPInputViews[texture];
	if (slice >= slices.size()) {
		slices.resize(slice + 1);
	}
	if (!slices[slice]) {
		D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC ivDesc = {};
		ivDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
		ivDesc.Texture2D.MipSlice = slice; // array slice for Texture2D arrays
		HRESULT hr = m_videoDevice->CreateVideoProcessorInputView(
			texture, m_vpEnumerator.Get(), &ivDesc, slices[slice].ReleaseAndGetAddressOf());
		if (FAILED(hr)) {
			Utils::Logf("CreateVideoProcessorInputView(slice %u) failed 0x%08X\n", slice, (unsigned)hr);
			return nullptr;
		}
	}
	return slices[slice];
}

// Draw one decoded frame: NV12 array slice -> VideoProcessorBlt -> back buffer.
bool VideoRenderer::RenderVideoProcessor(AVFrame *frame)
{
	if (frame->format != AV_PIX_FMT_D3D11) {
		// PyroWave/other CPU formats never reach a W10M stream (H.264-only),
		// but stay defensive.
		return false;
	}
	ID3D11Texture2D *ffmpegTexture = (ID3D11Texture2D *)(frame->data[0]);
	if (!ffmpegTexture || !m_videoProcessor) {
		return false;
	}
	UINT slice = (UINT)(intptr_t)frame->data[1];

	// W10M: the decoder surface is a BIND_DECODER|BIND_SHADER_RESOURCE array
	// texture (see the ffmpeg frames-pool hook). Copy the slice into a plain
	// NV12 texture first so the VPIV never wraps a decoder-bound surface;
	// the Qualcomm 9_3 UMD crashed inside VideoProcessorBlt on the direct
	// path (3 identical dumps at qcdx11um8626+0x5b59c).
	auto *ctx = m_deviceResources->GetD3DDeviceContext();
	if (!m_vpFlatNV12) {
		D3D11_TEXTURE2D_DESC decDesc = {};
		ffmpegTexture->GetDesc(&decDesc);
		decDesc.ArraySize = 1;
		decDesc.MipLevels = 1;
		decDesc.SampleDesc.Count = 1;
		decDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		decDesc.MiscFlags = 0;
		decDesc.CPUAccessFlags = 0;
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateTexture2D(&decDesc, nullptr, &m_vpFlatNV12)
			, "CreateTexture2D(VP flat NV12)");
		Utils::Logf("VP flat NV12 copy target created %ux%u\n", decDesc.Width, decDesc.Height);
	}
	ctx->CopySubresourceRegion(m_vpFlatNV12.Get(), 0, 0, 0, 0, ffmpegTexture, slice, nullptr);
	ffmpegTexture = m_vpFlatNV12.Get();
	slice = 0;

	// Letterbox clear, then the driver blits over the video rect.
	ID3D11RenderTargetView* renderTarget[] = { m_deviceResources->GetBackBufferRenderTargetView() };
	ctx->ClearRenderTargetView(renderTarget[0], Colors::Black);

	auto inView = getInputView(ffmpegTexture, slice);
	if (!inView) {
		return false;
	}

	// Output view sits on an intermediate render target, NOT on the swap chain's
	// FLIP_SEQUENTIAL back buffer: the Qualcomm FL 9_3 UMD crashes inside
	// VideoProcessorBlt when it writes a composition back buffer directly
	// (qcdx11um8626+0x5b59c, reproducible across dumps 8084/1628). Blit to a
	// plain BGRA8 render target instead and CopyResource it to the back buffer.
	DXGI_SWAP_CHAIN_DESC1 scDesc = {};
	m_deviceResources->GetSwapChain()->GetDesc1(&scDesc);
	if (!m_vpOutputView || scDesc.Width != m_vpOutWidth || scDesc.Height != m_vpOutHeight) {
		ComPtr<ID3D11Texture2D> backBuffer;
		DX::ThrowIfFailed(
			m_deviceResources->GetSwapChain()->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
			, "GetBuffer(0)");
		D3D11_TEXTURE2D_DESC bbDesc = {};
		backBuffer->GetDesc(&bbDesc);

		D3D11_TEXTURE2D_DESC midDesc = bbDesc;
		midDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
		midDesc.MiscFlags = 0;
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateTexture2D(&midDesc, nullptr, &m_vpMiddleTex)
			, "CreateTexture2D(VP middle RT)");

		D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ovDesc = {};
		ovDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
		m_vpOutputView.Reset();
		DX::ThrowIfFailed(
			m_videoDevice->CreateVideoProcessorOutputView(
				m_vpMiddleTex.Get(), m_vpEnumerator.Get(), &ovDesc, &m_vpOutputView)
			, "CreateVideoProcessorOutputView");
		m_vpBackBufferTex = backBuffer;
		m_vpOutWidth = scDesc.Width;
		m_vpOutHeight = scDesc.Height;
		Utils::Logf("VP output view on %ux%u middle RT (back buffer %ux%u)\n",
			m_vpOutWidth, m_vpOutHeight, bbDesc.Width, bbDesc.Height);
	}

	// Source: the visible part of the (possibly padded) decoder surface.
	RECT src = { 0, 0, (LONG)m_DecoderParams.width, (LONG)m_DecoderParams.height };
	// Destination: letterboxed into the back buffer, aspect preserved.
	LONG bbW = (LONG)m_vpOutWidth, bbH = (LONG)m_vpOutHeight;
	double s = std::min((double)bbW / m_DecoderParams.width, (double)bbH / m_DecoderParams.height);
	LONG dstW = lround(m_DecoderParams.width * s);
	LONG dstH = lround(m_DecoderParams.height * s);
	RECT dst = { (bbW - dstW) / 2, (bbH - dstH) / 2, (bbW - dstW) / 2 + dstW, (bbH - dstH) / 2 + dstH };
	m_videoContext->VideoProcessorSetStreamSourceRect(m_videoProcessor.Get(), 0, TRUE, &src);
	m_videoContext->VideoProcessorSetStreamDestRect(m_videoProcessor.Get(), 0, TRUE, &dst);

	D3D11_VIDEO_PROCESSOR_STREAM stream = {};
	stream.Enable = TRUE;
	stream.pInputSurface = inView.Get();

	static bool s_vpFirstFrameLogged = false;
	if (!s_vpFirstFrameLogged) {
		Utils::Log("VP: blt begin\n");
	}
	HRESULT hr = m_videoContext->VideoProcessorBlt(m_videoProcessor.Get(), m_vpOutputView.Get(), 0, 1, &stream);
	if (FAILED(hr)) {
		Utils::Logf("VideoProcessorBlt failed 0x%08X\n", (unsigned)hr);
		return false;
	}
	if (!s_vpFirstFrameLogged) {
		Utils::Log("VP: blt ok\n");
	}

	// Blit landed on the middle RT; copy it onto the swap chain back buffer.
	ctx->CopyResource(m_vpBackBufferTex.Get(), m_vpMiddleTex.Get());
	if (!s_vpFirstFrameLogged) {
		Utils::Log("VP: present copy ok\n");
		s_vpFirstFrameLogged = true;
	}
	return true;
}

