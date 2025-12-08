#include "pch.h"
#include "VideoRenderer.h"
#include <State\MoonlightClient.h>
#include "..\Common\DirectXHelper.h"
#include <Streaming\FFMpegDecoder.h>
#include <Utils.hpp>
#include "..\Common\ModalDialog.xaml.h"

#include <d3d11shader.h>
#include <d3dcompiler.h>

extern "C" {
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

static inline std::vector<DXGI_FORMAT> getVideoTextureSRVFormats(DXGI_FORMAT fmt)
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
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete.load(std::memory_order_acquire)) {
		return true;
	}

	auto *ctx = m_deviceResources->GetD3DDeviceContext();
	auto *dev = m_deviceResources->GetD3DDevice();

	ID3D11Texture2D *ffmpegTexture = (ID3D11Texture2D *)(frame->data[0]);
	D3D11_TEXTURE2D_DESC ffmpegDesc;
	ffmpegTexture->GetDesc(&ffmpegDesc);

	bool hasChanged = hasFrameFormatChanged(frame);
	if (hasChanged) {
		// Create our internal texture to copy and render
		setupVideoTexture(ffmpegDesc);
	}

	// Copy this frame into our video texture
	ctx->CopySubresourceRegion1(m_VideoTexture.Get(), 0, 0, 0, 0,
	                            (ID3D11Resource*)frame->data[0], (int)(intptr_t)frame->data[1],
	                            nullptr, D3D11_COPY_DISCARD);

	// SRV 0 is always mapped to the video texture
	UINT srvIndex = 0;

	// Setup shader
	ctx->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->IASetInputLayout(m_inputLayout.Get());
	ctx->VSSetShader(m_vertexShader.Get(), nullptr, 0);
	ctx->PSSetShader(m_pixelShaderYUV420.Get(), nullptr, 0);

	if (hasChanged) {
		setupVertexBuffer(ffmpegDesc);
		bindColorConversion(frame, ffmpegDesc);
	}

	UINT stride = sizeof(VERTEX);
	UINT offset = 0;
	ctx->IASetVertexBuffers(0, 1, m_VideoVertexBuffer.GetAddressOf(), &stride, &offset);
	ctx->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Bind SRVs for this frame
	ID3D11ShaderResourceView* frameSrvs[] = { m_VideoTextureResourceViews[srvIndex][0].Get(), m_VideoTextureResourceViews[srvIndex][1].Get() };
	ctx->PSSetShaderResources(0, 2, frameSrvs);
	ctx->PSSetConstantBuffers(0, 1, m_cscConstantBuffer.GetAddressOf());

	// Draw the video
	ctx->DrawIndexed(6, 0, 0);

	// Unbind SRVs for this frame
	ID3D11ShaderResourceView* nullSrvs[2] = {};
	ctx->PSSetShaderResources(0, 2, nullSrvs);

	if (frame->color_trc != m_LastColorTrc) {
		DXGI_COLOR_SPACE_TYPE colorspace = {};

		if (frame->color_trc == AVCOL_TRC_SMPTE2084) {
			// Switch to Rec 2020 PQ (SMPTE ST 2084) colorspace for HDR10 rendering
			colorspace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
		} else {
			// Restore default sRGB colorspace
			colorspace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
		}

		UINT colorSpaceSupport = 0;
		if (colorspace && SUCCEEDED(m_deviceResources->GetSwapChain()->CheckColorSpaceSupport(colorspace, &colorSpaceSupport)) && (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
			DX::ThrowIfFailed(m_deviceResources->GetSwapChain()->SetColorSpace1(colorspace));
			Utils::Logf("Colorspace changed to %s\n",
			            colorspace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
			                ? "DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020"
			                : "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709");
		}

		m_LastColorTrc = frame->color_trc;
	}

	return true;
}

void VideoRenderer::CreateDeviceDependentResources()
{
	Utils::Log("Started with creation of DXView\n");

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

	// Pixel shader
	{
		auto pixelShaderBytecode = DX::ReadData(L"Assets\\Shader\\d3d11_yuv420_pixel.fxc");
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreatePixelShader(
				pixelShaderBytecode.data(),
				pixelShaderBytecode.size(),
				nullptr,
				&m_pixelShaderYUV420
			)
			, "Pixel Shader Creation");
	}

	Windows::Graphics::Display::Core::HdmiDisplayInformation^ hdi = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
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

	int width = configuration->width;
	int height = configuration->height;

	int status = client->StartStreaming(m_deviceResources, configuration);
	if (status != 0) {
		Windows::UI::Xaml::Controls::ContentDialog^ dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
		Utils::logMutex.lock();
		std::wstring m_text = L"";
		std::vector<std::wstring> lines = Utils::GetLogLines();
		for (int i = 0; i < lines.size(); i++) {
			//Get only the last 24 lines
			if (lines.size() - i < 24) {
				m_text += lines[i];
			}
		}
		Utils::logMutex.unlock();
		Utils::showLogs = true;
		auto sv = ref new Windows::UI::Xaml::Controls::ScrollViewer();
		sv->VerticalScrollMode = Windows::UI::Xaml::Controls::ScrollMode::Enabled;
		sv->VerticalScrollBarVisibility = Windows::UI::Xaml::Controls::ScrollBarVisibility::Visible;
		auto tb = ref new Windows::UI::Xaml::Controls::TextBlock();
		tb->Text = ref new Platform::String(m_text.c_str());
		sv->Content = tb;
		dialog->Content = sv;
		dialog->CloseButtonText = L"OK";
		dialog->ShowAsync();
		return;
	}
	m_loadingComplete.store(true, std::memory_order_release);
	Utils::Log("Loading Complete!\n");
}

void VideoRenderer::ReleaseDeviceDependentResources()
{
	Windows::Graphics::Display::Core::HdmiDisplayInformation^ hdi = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
	m_loadingComplete.store(false, std::memory_order_release);
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShaderYUV420.Reset();
	m_cscConstantBuffer.Reset();
	m_VideoVertexBuffer.Reset();
	m_samplerState.Reset();
	m_indexBuffer.Reset();
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

bool VideoRenderer::setupVideoTexture(D3D11_TEXTURE2D_DESC frameDesc)
{
	m_TextureWidth = frameDesc.Width;
	m_TextureHeight = frameDesc.Height;
	m_TextureFormat = frameDesc.Format;
	assert(m_TextureWidth > 0 && m_TextureHeight > 0);

	D3D11_TEXTURE2D_DESC texDesc = {};

	texDesc.Width = m_TextureWidth;
	texDesc.Height = m_TextureHeight;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = m_TextureFormat;
	texDesc.SampleDesc.Quality = 0;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	m_VideoTexture.Reset();
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateTexture2D(&texDesc, nullptr, &m_VideoTexture));

	// Create SRVs for the texture
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	size_t srvIndex = 0;
	for (DXGI_FORMAT srvFormat : getVideoTextureSRVFormats(frameDesc.Format)) {
		assert(srvIndex < m_VideoTextureResourceViews[0].size());

		m_VideoTextureResourceViews[0][srvIndex].Reset();

		srvDesc.Format = srvFormat;
		DX::ThrowIfFailed(
		    m_deviceResources->GetD3DDevice()->CreateShaderResourceView(m_VideoTexture.Get(), &srvDesc, &m_VideoTextureResourceViews[0][srvIndex]));

		srvIndex++;
	}

	return true;
}

// Create our fixed vertex buffer for video rendering
void VideoRenderer::setupVertexBuffer(D3D11_TEXTURE2D_DESC frameDesc)
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

	float uMax = m_TextureWidth > 0 ? (float)m_DecoderParams.width / m_TextureWidth : 1.0f;
	float vMax = m_TextureHeight > 0 ? (float)m_DecoderParams.height / m_TextureHeight : 1.0f;

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
	switch (getFrameColorspace(frame)) {
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

	// Limit chroma texcoords to avoid sampling from alignment texels
	constBuf.chromaUVMax[0] = m_DecoderParams.width != (int)m_TextureWidth ?
	                              ((float)(m_DecoderParams.width - 1) / m_TextureWidth) : 1.0f;
	constBuf.chromaUVMax[1] = m_DecoderParams.height != (int)m_TextureHeight ?
	                              ((float)(m_DecoderParams.height - 1) / m_TextureHeight) : 1.0f;

	D3D11_SUBRESOURCE_DATA constData = {};
	constData.pSysMem = &constBuf;

	Utils::Logf("Setup pixel shader params: chromaOffset[0] %f, chromaOffset[1] %f, chromaUVMax[0] %f, chromaUVMax[1] %f\n",
				constBuf.chromaOffset[0], constBuf.chromaOffset[1],
				constBuf.chromaUVMax[0], constBuf.chromaUVMax[1]);

	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&constDesc, &constData, &m_cscConstantBuffer));
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

