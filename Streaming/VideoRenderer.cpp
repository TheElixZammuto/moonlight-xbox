#include "pch.h"
#include "VideoRenderer.h"
#include <State\MoonlightClient.h>
#include "..\Common\DirectXHelper.h"
#include <Streaming\FFMpegDecoder.h>
#include <Utils.hpp>
#include "..\Common\ModalDialog.xaml.h"

extern "C" {
#include "libgamestream/client.h"
#include <Limelight.h>
#include <libavcodec/avcodec.h>

}

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

static const float k_CscMatrix_Bt601Lim[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
	1.1644f, 1.1644f, 1.1644f,
	0.0f, -0.3917f, 2.0172f,
	1.5960f, -0.8129f, 0.0f,
};
static const float k_CscMatrix_Bt601Full[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
	1.0f, 1.0f, 1.0f,
	0.0f, -0.3441f, 1.7720f,
	1.4020f, -0.7141f, 0.0f,
};
static const float k_CscMatrix_Bt709Lim[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
	1.1644f, 1.1644f, 1.1644f,
	0.0f, -0.2132f, 2.1124f,
	1.7927f, -0.5329f, 0.0f,
};
static const float k_CscMatrix_Bt709Full[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
	1.0f, 1.0f, 1.0f,
	0.0f, -0.1873f, 1.8556f,
	1.5748f, -0.4681f, 0.0f,
};
static const float k_CscMatrix_Bt2020Lim[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
	1.1644f, 1.1644f, 1.1644f,
	0.0f, -0.1874f, 2.1418f,
	1.6781f, -0.6505f, 0.0f,
};
static const float k_CscMatrix_Bt2020Full[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
	1.0f, 1.0f, 1.0f,
	0.0f, -0.1646f, 1.8814f,
	1.4746f, -0.5714f, 0.0f,
};

#define OFFSETS_ELEMENT_COUNT 3

// 8-bit offsets
static const float k_Offsets_Lim[OFFSETS_ELEMENT_COUNT] = { 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f };
static const float k_Offsets_Full[OFFSETS_ELEMENT_COUNT] = { 0.0f, 128.0f / 255.0f, 128.0f / 255.0f };
// 10-bit offsets
static const float k_Offsets_10bit_Lim[OFFSETS_ELEMENT_COUNT] = { 64.0f / 1023.0f, 512.0f / 1023.0f, 512.0f / 1023.0f };
static const float k_Offsets_10bit_Full[OFFSETS_ELEMENT_COUNT] = { 0.0f, 512.0f / 1023.0f, 512.0f / 1023.0f };

typedef struct _CSC_CONST_BUF
{
	// CscMatrix value from above but packed appropriately
	float cscMatrix[CSC_MATRIX_PACKED_ELEMENT_COUNT];

	// YUV offset values from above
	float offsets[OFFSETS_ELEMENT_COUNT];

	// Padding float to be a multiple of 16 bytes
	float padding;
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

void VideoRenderer::ensureYuvTargets(ID3D11Device* dev, DXGI_FORMAT fmt, UINT w, UINT h) {
	if (m_yuvTexture && m_yuvFormat == fmt && m_yuvWidth == w && m_yuvHeight == h)
		return;

	m_yuvTexture.Reset();
	m_srvY.Reset();
	m_srvUV.Reset();

	D3D11_TEXTURE2D_DESC d = {};
	d.Width = w;
	d.Height = h;
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.Format = fmt;
	d.SampleDesc.Count = 1;
	d.Usage = D3D11_USAGE_DEFAULT;
	d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	DX::ThrowIfFailed(dev->CreateTexture2D(&d, nullptr, &m_yuvTexture));

	D3D11_SHADER_RESOURCE_VIEW_DESC yDesc = {};
	yDesc.Format = (fmt == DXGI_FORMAT_P010) ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
	yDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	yDesc.Texture2D.MostDetailedMip = 0;
	yDesc.Texture2D.MipLevels = 1;
	DX::ThrowIfFailed(dev->CreateShaderResourceView(m_yuvTexture.Get(), &yDesc, &m_srvY));

	D3D11_SHADER_RESOURCE_VIEW_DESC uvDesc = {};
	uvDesc.Format = (fmt == DXGI_FORMAT_P010) ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
	uvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	uvDesc.Texture2D.MostDetailedMip = 0;
	uvDesc.Texture2D.MipLevels = 1;
	DX::ThrowIfFailed(dev->CreateShaderResourceView(m_yuvTexture.Get(), &uvDesc, &m_srvUV));

	m_yuvFormat = fmt;
	m_yuvWidth = w;
	m_yuvHeight = h;
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
	int index = (int)(frame->data[1]);

	ensureYuvTargets(dev, ffmpegDesc.Format, frame->width, frame->height);

	ID3D11ShaderResourceView *nullSrvs[2] = {nullptr, nullptr};
	ctx->PSSetShaderResources(0, 2, nullSrvs);

	D3D11_BOX box{};
	box.right = frame->width;
	box.bottom = frame->height;
	box.back = 1;
	ctx->CopySubresourceRegion(m_yuvTexture.Get(), 0, 0, 0, 0, ffmpegTexture, index, &box);

	ID3D11ShaderResourceView *srvs[2] = {m_srvY.Get(), m_srvUV.Get()};
	ctx->PSSetShaderResources(0, 2, srvs);

	UINT stride = sizeof(VERTEX);
	UINT offset = 0;
	ctx->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	ctx->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->IASetInputLayout(m_inputLayout.Get());
	// Attach our vertex shader.
	ctx->VSSetShader(m_vertexShader.Get(), nullptr, 0);

	bindColorConversion(frame);

	ctx->DrawIndexed(6, 0, 0);

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
	// Load shaders asynchronously.
	// After the vertex shader file is loaded, create the shader and input layout.
	auto createVSTask = DX::ReadDataAsync(L"Assets\\Shader\\d3d11_vertex.fxc").then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateVertexShader(
				&fileData[0],
				fileData.size(),
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
				&fileData[0],
				fileData.size(),
				&m_inputLayout
			)
			, "Input Layout Creation");
		});

	// After the pixel shader file is loaded, create the shader and constant buffer.
	auto createPSTaskGen = DX::ReadDataAsync(L"Assets\\Shader\\d3d11_genyuv_pixel.fxc").then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreatePixelShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_pixelShaderGeneric
			)
			, "Pixel Shader Creation");
		});

	auto createPSTaskBT601 = DX::ReadDataAsync(L"Assets\\Shader\\d3d11_bt601lim_pixel.fxc").then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreatePixelShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_pixelShaderBT601
			)
			, "Pixel Shader Creation");
		});

	auto createPSTaskBT2020 = DX::ReadDataAsync(L"Assets\\Shader\\d3d11_bt2020lim_pixel.fxc").then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreatePixelShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_pixelShaderBT2020
			)
			, "Pixel Shader Creation");
		});

	// Once both shaders are loaded, create the mesh.
	auto createCubeTask = (createVSTask && createPSTaskGen && createPSTaskBT601 && createPSTaskBT2020).then([this]() {
		Windows::Graphics::Display::Core::HdmiDisplayInformation^ hdi = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
		auto w = CoreWindow::GetForCurrentThread();
		int m_DisplayWidth = (int)w->Bounds.Width;
		int m_DisplayHeight = (int)w->Bounds.Height;
		// HDR Setup
		if (hdi) {
			m_lastDisplayMode = hdi->GetCurrentDisplayMode();
			m_currentDisplayMode = m_lastDisplayMode;
			// Scale video to the window size while preserving aspect ratio
			m_DisplayWidth = std::max(m_currentDisplayMode->ResolutionWidthInRawPixels, (UINT)1920);
			m_DisplayHeight = std::max(m_currentDisplayMode->ResolutionHeightInRawPixels, (UINT)1080);
		}
		IRECT src, dst;
		src.x = src.y = 0;
		src.w = this->configuration->width;
		src.h = this->configuration->height;
		dst.x = dst.y = 0;
		dst.w = m_DisplayWidth;
		dst.h = m_DisplayHeight;
		scaleSourceToDestinationSurface(&src, &dst);

		// Convert screen space to normalized device coordinates
		FRECT renderRect;
		screenSpaceToNormalizedDeviceCoords(&dst, &renderRect, m_DisplayWidth, m_DisplayHeight);
		VERTEX verts[] =
		{
			{renderRect.x, renderRect.y, 0, 1.0f},
			{renderRect.x, renderRect.y + renderRect.h, 0, 0},
			{renderRect.x + renderRect.w, renderRect.y, 1.0f, 1.0f},
			{renderRect.x + renderRect.w, renderRect.y + renderRect.h, 1.0f, 0},
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
				&m_vertexBuffer
			)
			, "Vertex Buffer Creation");

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
		});

	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = -FLT_MAX;
	samplerDesc.MaxLOD = FLT_MAX;
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateSamplerState(&samplerDesc, &samplerState), "Sampler Creation");
	m_deviceResources->GetD3DDeviceContext()->PSSetSamplers(0, 1, samplerState.GetAddressOf());
	renderTextureDesc = { 0 };
	int width = configuration->width;
	int height = configuration->height;
	renderTextureDesc.Width = width;
	renderTextureDesc.Height = height;
	renderTextureDesc.ArraySize = 1;
	renderTextureDesc.Format = DXGI_FORMAT_NV12;
	renderTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	renderTextureDesc.MipLevels = 1;
	renderTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	renderTextureDesc.SampleDesc.Quality = 0;
	renderTextureDesc.SampleDesc.Count = 1;
	renderTextureDesc.CPUAccessFlags = 0;
	renderTextureDesc.MiscFlags = 0;
	Microsoft::WRL::ComPtr<IDXGIResource1> dxgiResource;
	createCubeTask.then([this, width, height]() {
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
			concurrency::create_task(::moonlight_xbox_dx::ModalDialog::ShowOnceAsync(dialog));
			return;
		}
		m_loadingComplete.store(true, std::memory_order_release);
		Utils::Log("Loading Complete!\n");
		});
}

void VideoRenderer::ReleaseDeviceDependentResources()
{
	Windows::Graphics::Display::Core::HdmiDisplayInformation^ hdi = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
	m_loadingComplete.store(false, std::memory_order_release);
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShaderGeneric.Reset();
	m_pixelShaderBT601.Reset();
	m_pixelShaderBT2020.Reset();
	m_constantBuffer.Reset();
	m_vertexBuffer.Reset();
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

void VideoRenderer::bindColorConversion(AVFrame* frame)
{
	bool fullRange = frame->color_range == AVCOL_RANGE_JPEG;
	int colorspace = COLORSPACE_REC_601;
	switch (frame->colorspace) {
	case AVCOL_SPC_SMPTE170M:
	case AVCOL_SPC_BT470BG:
		colorspace = COLORSPACE_REC_601;
		break;
	case AVCOL_SPC_BT709:
		colorspace = COLORSPACE_REC_709;
		break;
	case AVCOL_SPC_BT2020_NCL:
	case AVCOL_SPC_BT2020_CL:
		colorspace = COLORSPACE_REC_2020;
		break;
	}

	// We have purpose-built shaders for the common Rec 601 (SDR) and Rec 2020 (HDR) cases
	if (!fullRange && colorspace == COLORSPACE_REC_601) {
		m_deviceResources->GetD3DDeviceContext()->PSSetShader(m_pixelShaderBT601.Get(), nullptr, 0);
	}
	else if (!fullRange && colorspace == COLORSPACE_REC_2020) {
		m_deviceResources->GetD3DDeviceContext()->PSSetShader(m_pixelShaderBT2020.Get(), nullptr, 0);
	}
	else {
		// We'll need to use the generic shader for this colorspace and color range combo
		m_deviceResources->GetD3DDeviceContext()->PSSetShader(m_pixelShaderGeneric.Get(), nullptr, 0);

		// If nothing has changed since last frame, we're done
		if (colorspace == m_LastColorSpace && fullRange == m_LastFullRange) {
			return;
		}

		Utils::Logf("Falling back to generic video pixel shader for %d (%s range)\n", colorspace, fullRange ? "full" : "limited");

		D3D11_BUFFER_DESC constDesc = {};
		constDesc.ByteWidth = sizeof(CSC_CONST_BUF);
		constDesc.Usage = D3D11_USAGE_IMMUTABLE;
		constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constDesc.CPUAccessFlags = 0;
		constDesc.MiscFlags = 0;

		CSC_CONST_BUF constBuf = {};
		const float* rawCscMatrix;
		switch (colorspace) {
		case COLORSPACE_REC_601:
			rawCscMatrix = fullRange ? k_CscMatrix_Bt601Full : k_CscMatrix_Bt601Lim;
			break;
		case COLORSPACE_REC_709:
			rawCscMatrix = fullRange ? k_CscMatrix_Bt709Full : k_CscMatrix_Bt709Lim;
			break;
		case COLORSPACE_REC_2020:
			rawCscMatrix = fullRange ? k_CscMatrix_Bt2020Full : k_CscMatrix_Bt2020Lim;
			break;
		default:
			return;
		}

		// We need to adjust our raw CSC matrix to be column-major and with float3 vectors
		// padded with a float in between each of them to adhere to HLSL requirements.
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				constBuf.cscMatrix[i * 4 + j] = rawCscMatrix[j * 3 + i];
			}
		}

		// No adjustments are needed to the float[3] array of offsets, so it can just
		// be copied with memcpy().
		if (colorspace == COLORSPACE_REC_2020) {
			// This is important to avoid tinting the image slightly
			memcpy(constBuf.offsets, fullRange ? k_Offsets_10bit_Full : k_Offsets_10bit_Lim, sizeof(constBuf.offsets));
		}
		else {
			memcpy(constBuf.offsets, fullRange ? k_Offsets_Full : k_Offsets_Lim, sizeof(constBuf.offsets));
		}

		D3D11_SUBRESOURCE_DATA constData = {};
		constData.pSysMem = &constBuf;

		ID3D11Buffer* constantBuffer;
		HRESULT hr = m_deviceResources->GetD3DDevice()->CreateBuffer(&constDesc, &constData, &constantBuffer);
		if (SUCCEEDED(hr)) {
			m_deviceResources->GetD3DDeviceContext()->PSSetConstantBuffers(0, 1, &constantBuffer);
			constantBuffer->Release();
		}
		else {
			/*SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				"ID3D11Device::CreateBuffer() failed: %x",
				hr);*/
			return;
		}
	}

	m_LastColorSpace = colorspace;
	m_LastFullRange = fullRange;
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

