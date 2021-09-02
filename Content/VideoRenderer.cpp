#include "pch.h"
#include "VideoRenderer.h"
#include "MoonlightClient.h"
#include "..\Common\DirectXHelper.h"
#include <FFMpegDecoder.h>
#include <Utils.hpp>

extern "C" {
#include "libgamestream/client.h"
#include <Limelight.h>
#include <libavcodec/avcodec.h>

}

using namespace moonlight_xbox_dx;
using namespace DirectX;
using namespace Windows::Foundation;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
VideoRenderer::VideoRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_degreesPerSecond(45),
	m_indexCount(0),
	m_tracking(false),
	m_deviceResources(deviceResources)
{
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
bool renderedOneFrame = false;
// Renders one frame using the vertex and pixel shaders.
void VideoRenderer::Render()
{
		// Loading is asynchronous. Only draw geometry after it's loaded.
		if (!m_loadingComplete || renderTexture == NULL)
		{
			return;
		}
		//Create a rendering texture
		client->pacer->PrepareFrameForRendering();
		auto texture = client->pacer->GetCurrentRenderingFrame();
		if (texture == NULL || texture == nullptr) {
			return;
		}
		m_deviceResources->GetD3DDeviceContext()->CopyResource(renderTexture.Get(), texture.Get());
		client->pacer->ReleaseFrame();
		ID3D11ShaderResourceView* m_luminance_shader_resource_view;
		ID3D11ShaderResourceView* m_chrominance_shader_resource_view;
		D3D11_SHADER_RESOURCE_VIEW_DESC luminance_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(renderTexture.Get(), D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8_UNORM);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateShaderResourceView(renderTexture.Get(), &luminance_desc, &m_luminance_shader_resource_view),"Luminance SRV Creation");
		D3D11_SHADER_RESOURCE_VIEW_DESC chrominance_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(renderTexture.Get(), D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8_UNORM);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateShaderResourceView(renderTexture.Get(), &chrominance_desc, &m_chrominance_shader_resource_view),"Chrominance SRV Creation");
		m_deviceResources->GetD3DDeviceContext()->PSSetShaderResources(0, 1, &m_luminance_shader_resource_view);
		m_deviceResources->GetD3DDeviceContext()->PSSetShaderResources(1, 1, &m_chrominance_shader_resource_view);

		auto context = m_deviceResources->GetD3DDeviceContext();
		UINT stride = sizeof(VertexPositionColor);
		UINT offset = 0;
		context->IASetVertexBuffers(
			0,
			1,
			m_vertexBuffer.GetAddressOf(),
			&stride,
			&offset
		);
		context->IASetIndexBuffer(
			m_indexBuffer.Get(),
			DXGI_FORMAT_R16_UINT,
			0
		);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(m_inputLayout.Get());
		// Attach our vertex shader.
		context->VSSetShader(m_vertexShader.Get(),nullptr,0);
		context->PSSetSamplers(0, 1, samplerState.GetAddressOf());
		context->PSSetShader(m_pixelShader.Get(),nullptr,0);
		context->DrawIndexed(m_indexCount,0,0);
		//m_luminance_shader_resource_view->Release();
		//m_chrominance_shader_resource_view->Release();
		if (!renderedOneFrame) {
			Utils::Log("Rendered First Frame!\n");
			renderedOneFrame = true;
		}
}

void VideoRenderer::CreateDeviceDependentResources()
{
	Utils::Log("Started with creation of DXView\n");
	// Load shaders asynchronously.
	auto loadVSTask = DX::ReadDataAsync(L"SampleVertexShader.cso");
	auto loadPSTask = DX::ReadDataAsync(L"SamplePixelShader.cso");

	// After the vertex shader file is loaded, create the shader and input layout.
	auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateVertexShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_vertexShader
				)
			,"Vertex Shader Creation");

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc [] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateInputLayout(
				vertexDesc,
				ARRAYSIZE(vertexDesc),
				&fileData[0],
				fileData.size(),
				&m_inputLayout
				)
			,"Input Layout Creation");
	});

	// After the pixel shader file is loaded, create the shader and constant buffer.
	auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreatePixelShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_pixelShader
				)
			, "Pixel Shader Creation");
	});

	// Once both shaders are loaded, create the mesh.
	auto createCubeTask = (createPSTask && createVSTask).then([this] () {
		
		/*
		 12
		 03
		*/
		// Load mesh vertices. Each vertex has a position and a color.
		static const VertexPositionColor cubeVertices[] = 
		{
			{XMFLOAT3(-1.0f,-1.0f,0.0f), XMFLOAT2(0.0f, 1.0f)},
			{XMFLOAT3(-1.0f,1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(1.0f,1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f)},
			{XMFLOAT3(1.0f,-1.0f,0.0f), XMFLOAT2(1.0f, 1.0f)},
			
		};

		D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
		vertexBufferData.pSysMem = cubeVertices;
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(cubeVertices), D3D11_BIND_VERTEX_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&vertexBufferDesc,
				&vertexBufferData,
				&m_vertexBuffer
				)
			,"Vertex Buffer Creation");

		// Load mesh indices. Each trio of indices represents
		// a triangle to be rendered on the screen.
		// For example: 0,2,1 means that the vertices with indexes
		// 0, 2 and 1 from the vertex buffer compose the 
		// first triangle of this mesh.
		static const unsigned short cubeIndices [] =
		{
			0,2,1,
			3,2,0
		};

		m_indexCount = ARRAYSIZE(cubeIndices);

		D3D11_SUBRESOURCE_DATA indexBufferData = {0};
		indexBufferData.pSysMem = cubeIndices;
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC indexBufferDesc(sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&indexBufferDesc,
				&indexBufferData,
				&m_indexBuffer
				)
			,"Index Buffer Creation");
	});

	D3D11_RASTERIZER_DESC rasterizerState;
	ZeroMemory(&rasterizerState, sizeof(D3D11_RASTERIZER_DESC));

	rasterizerState.AntialiasedLineEnable = false;
	rasterizerState.CullMode = D3D11_CULL_NONE; // D3D11_CULL_FRONT or D3D11_CULL_NONE D3D11_CULL_BACK
	rasterizerState.FillMode = D3D11_FILL_SOLID; // D3D11_FILL_SOLID  D3D11_FILL_WIREFRAME
	rasterizerState.DepthBias = 0;
	rasterizerState.DepthBiasClamp = 0.0f;
	rasterizerState.DepthClipEnable = true;
	rasterizerState.FrontCounterClockwise = false;
	rasterizerState.MultisampleEnable = false;
	rasterizerState.ScissorEnable = false;
	rasterizerState.SlopeScaledDepthBias = 0.0f;
	ID3D11RasterizerState* m_pRasterState;
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateRasterizerState(&rasterizerState, &m_pRasterState),"Rasterizer Creation");
	m_deviceResources->GetD3DDeviceContext()->RSSetState(m_pRasterState);

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
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateSamplerState(&samplerDesc, samplerState.GetAddressOf()),"Sampler Creation");
	DX::ThrowIfFailed(E_ACCESSDENIED,"Test Logging");
	D3D11_TEXTURE2D_DESC renderTextureDesc = { 0 };
	int width = 1280;
	int height = 720;
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
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateTexture2D(&renderTextureDesc, NULL, renderTexture.GetAddressOf()),"Render Texture Creation");
	Microsoft::WRL::ComPtr<IDXGIResource1> dxgiResource;
	createCubeTask.then([this,width,height] () {
		client = MoonlightClient::GetInstance();
		client->pacer->renderingDevice = m_deviceResources->GetD3DDevice();
		int status = client->Init(m_deviceResources, width,height);
		if (status != 0) {
			Windows::UI::Xaml::Controls::ContentDialog^ dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
			dialog->Content = Utils::StringPrintf("Got status %d from Moonlight init", status);
			dialog->CloseButtonText = L"OK";
			dialog->ShowAsync();
		}
		m_loadingComplete = true;
		Utils::Log("Loading Complete!\n");
	});
}

void VideoRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_constantBuffer.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
}
