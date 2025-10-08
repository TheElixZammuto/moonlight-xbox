#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include <gamingdeviceinformation.h>
#include <windows.ui.xaml.media.dxinterop.h>
#include <winrt/Windows.UI.Core.h>
#include <Pages/StreamPage.xaml.h>
#include <Streaming/FFmpegDecoder.h>
#include <Plot/ImGuiPlots.h>

using namespace moonlight_xbox_dx;
using namespace D2D1;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::Graphics::Display::Core;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;

// Constants used to calculate screen rotations.
namespace ScreenRotation
{
	// 0-degree Z-rotation
	static const XMFLOAT4X4 Rotation0(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 90-degree Z-rotation
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 180-degree Z-rotation
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 270-degree Z-rotation
	static const XMFLOAT4X4 Rotation270(
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
};

// Constructor for DeviceResources.
DX::DeviceResources::DeviceResources() :
	m_backBufferFormat(DXGI_FORMAT_R10G10B10A2_UNORM), // 10-bit for HDR
	m_screenViewport(),
	m_d3dFeatureLevel(D3D_FEATURE_LEVEL_11_1),
	m_d3dRenderTargetSize(),
	m_dxgiFactoryFlags(0),
	m_enableVsync(false),
	m_swapchainVsync(false),
	m_outputSize(),
	m_logicalSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_compositionScaleX(1.0f),
	m_compositionScaleY(1.0f),
	m_deviceNotify(nullptr),
	m_stats(nullptr),
	m_imguiRunning(false),
	m_showImGui(false),
	m_frameLatencyWaitable()
{
	m_refreshRate = GetUWPRefreshRate();
	GetUWPPixelDimensions(&m_pixelWidth, &m_pixelHeight);

	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

// Configures resources that don't depend on the Direct3D device.
void DX::DeviceResources::CreateDeviceIndependentResources()
{
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DX::DeviceResources::CreateDeviceResources()
{
	// This flag adds support for surfaces with a different color channel ordering
	// than the API default. It is required for compatibility with Direct2D.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (DX::SdkLayersAvailable())
	{
		// If the project is in a debug build, enable debugging via SDK Layers with this flag.
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

#if defined(_DEBUG)
	{
		ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
		{
			m_dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

			dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
			dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

			DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
			{
				80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
			};
			DXGI_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
			filter.DenyList.pIDList = hide;
			dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
		}
	}
#endif

	DX::ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(m_dxgiFactory.ReleaseAndGetAddressOf())));

	// Determines whether tearing support is available for fullscreen borderless windows.
	if (!m_enableVsync) {
		BOOL allowTearing = FALSE;

		ComPtr<IDXGIFactory5> factory5;
		HRESULT hr = m_dxgiFactory.As(&factory5);
		if (SUCCEEDED(hr)) {
			hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
		}

		Utils::Logf("CheckFeatureSupport(ALLOW_TEARING): %d\n", allowTearing ? 1 : 0);

		if (FAILED(hr) || !allowTearing) {
			//m_displayStatus &= ~VRR_SUPPORTED;
			Utils::Log("Warning: Variable Refresh Rate display was not detected, vsync will be forced.\n");
			m_enableVsync = true;
		}
		else {
			Utils::Log("Warning: Variable Refresh Rate display detected, but VRR is not yet working. You will experience tearing.\n");
			//m_displayStatus |= VRR_SUPPORTED;
		}
	}

	// This array defines the set of DirectX hardware feature levels this app will support.
	// Note the ordering should be preserved.
	// Don't forget to declare your application's minimum required feature level in its
	// description.  All applications are assumed to support 9.1 unless otherwise stated.
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	// Create the Direct3D 11 API device object and a corresponding context.
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	HRESULT hr = D3D11CreateDevice(
		nullptr,					// Specify nullptr to use the default adapter.
		D3D_DRIVER_TYPE_HARDWARE,	// Create a device using the hardware graphics driver.
		0,							// Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
		creationFlags,				// Set debug and Direct2D compatibility flags.
		featureLevels,				// List of feature levels this app can support.
		ARRAYSIZE(featureLevels),	// Size of the list above.
		D3D11_SDK_VERSION,			// Always set this to D3D11_SDK_VERSION for Microsoft Store apps.
		&device,					// Returns the Direct3D device created.
		&m_d3dFeatureLevel,			// Returns feature level of device created.
		&context					// Returns the device immediate context.
		);

	if (FAILED(hr))
	{
		DX::ThrowIfFailed(hr);

		// If the initialization fails, fall back to the WARP device.
		// For more information on WARP, see:
		// https://go.microsoft.com/fwlink/?LinkId=286690
		DX::ThrowIfFailed(
			D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
				0,
				creationFlags,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				&device,
				&m_d3dFeatureLevel,
				&context
				)
			);
	}

	// Store pointers to the Direct3D 11.3 API device and immediate context.
	DX::ThrowIfFailed(
		device.As(&m_d3dDevice)
		);

	DX::ThrowIfFailed(
		context.As(&m_d3dContext)
		);
}

// These resources need to be recreated every time the window size is changed.
void DX::DeviceResources::CreateWindowSizeDependentResources()
{
	// Clear the previous window size specific context.
	ID3D11RenderTargetView* nullViews[] = {nullptr};
	m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
	m_d3dRenderTargetView = nullptr;
	m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

	UpdateRenderTargetSize();

	//Get the correct screen resolution and adapt the swapchain to 16:9 aspect ratio
	float normalizedWidth = (float)uwp_get_width();
	float normalizedHeight = (float)uwp_get_height();
	m_d3dRenderTargetSize.Width = normalizedWidth;
	m_d3dRenderTargetSize.Height = normalizedHeight;

	if (m_swapChain != nullptr && m_enableVsync == m_swapchainVsync)
	{
		// If the swap chain already exists, resize it.
		int flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		if (!m_enableVsync) flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		HRESULT hr = m_swapChain->ResizeBuffers(
			0, // Don't change the number of buffers
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			m_backBufferFormat,
			flags
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// If the device was removed for any reason, a new device and swap chain will need to be created.
			HandleDeviceLost();

			// Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method
			// and correctly set up the new device.
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// Otherwise, create a new one using the same adapter as the existing Direct3D device.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// Match the size of the window.
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = m_backBufferFormat;
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;								// Don't use multi-sampling.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		//Check moonlight-stream/moonlight-qt/app/streaming/video/ffmpeg-renderers/d3d11va.cpp for rationale
		swapChainDesc.BufferCount = 3;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		if (!m_enableVsync) swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// This sequence obtains the DXGI factory that was used to create the Direct3D device above.
		ComPtr<IDXGIDevice3> dxgiDevice;
		DX::ThrowIfFailed(
			m_d3dDevice.As(&dxgiDevice)
			);

		ComPtr<IDXGIAdapter> dxgiAdapter;
		DX::ThrowIfFailed(
			dxgiDevice->GetAdapter(&dxgiAdapter)
			);

		DX::ThrowIfFailed(
		    dxgiAdapter->EnumOutputs(0, &m_dxgiOutput));

		ComPtr<IDXGIFactory4> dxgiFactory;
		DX::ThrowIfFailed(
			dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
			);

		// When using XAML interop, the swap chain must be created for composition.
		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			dxgiFactory->CreateSwapChainForComposition(
				m_d3dDevice.Get(),
				&swapChainDesc,
				nullptr,
				&swapChain
			)
		);

		Utils::Logf("Vsync: %s\n", m_enableVsync ? "enabled" : "disabled");

		DX::ThrowIfFailed(
			swapChain.As(&m_swapChain)
		);

		m_swapChain->SetMaximumFrameLatency(1);
		m_frameLatencyWaitable = m_swapChain->GetFrameLatencyWaitableObject();

		// Associate swap chain with SwapChainPanel
		// UI changes will need to be dispatched back to the UI thread
		m_swapChainPanel->Dispatcher->RunAsync(CoreDispatcherPriority::High, ref new DispatchedHandler([=]()
		{
			// Get backing native interface for SwapChainPanel
			ComPtr<ISwapChainPanelNative> panelNative;
			DX::ThrowIfFailed(
				reinterpret_cast<IUnknown*>(m_swapChainPanel)->QueryInterface(IID_PPV_ARGS(&panelNative))
				);

			DX::ThrowIfFailed(
				panelNative->SetSwapChain(m_swapChain.Get())
				);
		}, CallbackContext::Any));
	}

	// note the current state of the swapchain wrt ALLOW_TEARING. If this is changed, we have to recreate
	// the swapchain.
	m_swapchainVsync = m_enableVsync;

	// Setup inverse scale on the swap chain
	DXGI_MATRIX_3X2_F inverseScale = { 0 };
	inverseScale._11 = 1.0f / m_effectiveCompositionScaleX;
	inverseScale._22 = 1.0f / m_effectiveCompositionScaleY;
	ComPtr<IDXGISwapChain4> spSwapChain2;
	DX::ThrowIfFailed(
		m_swapChain.As<IDXGISwapChain4>(&spSwapChain2)
		);

	DX::ThrowIfFailed(
		spSwapChain2->SetMatrixTransform(&inverseScale)
		);

	// Create a render target view of the swap chain back buffer.
	ComPtr<ID3D11Texture2D1> backBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
		);

	DX::ThrowIfFailed(
		m_d3dDevice->CreateRenderTargetView1(
			backBuffer.Get(),
			nullptr,
			&m_d3dRenderTargetView
			)
		);

	// Set the 3D rendering viewport to target the entire window.
	m_screenViewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		m_d3dRenderTargetSize.Width,
		m_d3dRenderTargetSize.Height
		);

	m_d3dContext->RSSetViewports(1, &m_screenViewport);
}

// Determine the dimensions of the render target and whether it will be scaled down.
void DX::DeviceResources::UpdateRenderTargetSize()
{
	auto state = GetApplicationState();
	m_effectiveDpi = m_dpi;
	float compositionScaleMultiplier = 1.0f;

	Windows::Graphics::Display::Core::HdmiDisplayInformation^ hdi = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
	if (hdi) {
		auto mode = hdi->GetCurrentDisplayMode();
		if (mode->ResolutionWidthInRawPixels > 2560) compositionScaleMultiplier = 2.0f;
		else if (mode->ResolutionWidthInRawPixels > 1920) compositionScaleMultiplier = 1.33333333f;
	}

	m_effectiveCompositionScaleX = m_compositionScaleX * compositionScaleMultiplier;
	m_effectiveCompositionScaleY = m_compositionScaleY * compositionScaleMultiplier;
}

// This method is called when the XAML control is created (or re-created).
void DX::DeviceResources::SetSwapChainPanel(SwapChainPanel^ panel)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_swapChainPanel = panel;
	m_logicalSize = Windows::Foundation::Size(static_cast<float>(panel->ActualWidth), static_cast<float>(panel->ActualHeight));

	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;
	m_compositionScaleX = panel->CompositionScaleX;
	m_compositionScaleY = panel->CompositionScaleY;

	CreateWindowSizeDependentResources();

	ComPtr<ISwapChainPanelNative> panelNative;
	DX::ThrowIfFailed(
		reinterpret_cast<IUnknown*>(m_swapChainPanel)->QueryInterface(IID_PPV_ARGS(&panelNative))
		);

	// Setup Dear ImGui context
	float dpi = currentDisplayInformation->LogicalDpi / 96.0f;
	ImGui_Init(panelNative, dpi);
}

// This method is called in the event handler for the SizeChanged event.
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

// This method is called in the event handler for the DpiChanged event.
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;
		CreateWindowSizeDependentResources();
	}
}

// This method is called in the event handler for the OrientationChanged event.
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// This method is called in the event handler for the CompositionScaleChanged event.
void DX::DeviceResources::SetCompositionScale(float compositionScaleX, float compositionScaleY)
{
	if (m_compositionScaleX != compositionScaleX ||
		m_compositionScaleY != compositionScaleY)
	{
		m_compositionScaleX = compositionScaleX;
		m_compositionScaleY = compositionScaleY;
		CreateWindowSizeDependentResources();
	}
}

void DX::DeviceResources::SetVsync(bool enableVsync)
{
	m_enableVsync = enableVsync;
}

// This method is called in the event handler for the DisplayContentsInvalidated event.
void DX::DeviceResources::ValidateDevice()
{
	// The D3D Device is no longer valid if the default adapter changed since the device
	// was created or if the device has been removed.

	// First, get the information for the default adapter from when the device was created.

	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

	ComPtr<IDXGIAdapter> deviceAdapter;
	DX::ThrowIfFailed(dxgiDevice->GetAdapter(&deviceAdapter));

	ComPtr<IDXGIFactory2> deviceFactory;
	DX::ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&deviceFactory)));

	ComPtr<IDXGIAdapter1> previousDefaultAdapter;
	DX::ThrowIfFailed(deviceFactory->EnumAdapters1(0, &previousDefaultAdapter));

	DXGI_ADAPTER_DESC1 previousDesc;
	DX::ThrowIfFailed(previousDefaultAdapter->GetDesc1(&previousDesc));

	// Next, get the information for the current default adapter.

	ComPtr<IDXGIFactory4> currentFactory;
	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC1 currentDesc;
	DX::ThrowIfFailed(currentDefaultAdapter->GetDesc1(&currentDesc));

	// If the adapter LUIDs don't match, or if the device reports that it has been removed,
	// a new D3D device must be created.

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		// Release references to resources related to the old device.
		dxgiDevice = nullptr;
		deviceAdapter = nullptr;
		deviceFactory = nullptr;
		previousDefaultAdapter = nullptr;

		// Create a new device and swap chain.
		HandleDeviceLost();
	}
}

// Recreate all device resources and set them back to the current state.
void DX::DeviceResources::HandleDeviceLost()
{
	ImGui_Deinit();

	m_swapChain = nullptr;

	Utils::Log("HandleDeviceLost()\n");

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceLost();
	}

	CreateDeviceResources();
	CreateWindowSizeDependentResources();

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceRestored();
	}
}

// Register our DeviceNotify to be informed on device lost and creation.
void DX::DeviceResources::RegisterDeviceNotify(DX::IDeviceNotify* deviceNotify)
{
	m_deviceNotify = deviceNotify;
}

// Call this method when the app suspends. It provides a hint to the driver that the app
// is entering an idle state and that temporary buffers can be reclaimed for use by other apps.
void DX::DeviceResources::Trim()
{
	ComPtr<IDXGIDevice3> dxgiDevice;
	m_d3dDevice.As(&dxgiDevice);

	dxgiDevice->Trim();
}

// Present the contents of the swap chain to the screen.
void DX::DeviceResources::Present()
{
	HRESULT hr = E_FAIL;
	DXGI_PRESENT_PARAMETERS parameters = { 0 };

	if (m_enableVsync) {
		// Composition swapchain
		hr = m_swapChain->Present1(0, 0, &parameters);
	}
	else {
		// Recommended to always use tearing if supported when using a sync interval of 0.
		// This call requires VRR to be set to Always On on Xbox
		hr = m_swapChain->Present1(0, DXGI_PRESENT_ALLOW_TEARING, &parameters);
		if (hr == DXGI_ERROR_INVALID_CALL) {
			Utils::Logf("Present1() vsync: disabled, DXGI_ERROR_INVALID_CALL\n");
		}
	}

	// Discard the contents of the render target.
	// This is a valid operation only when the existing contents will be entirely
	// overwritten. If dirty or scroll rects are used, this call should be modified.
	m_d3dContext->DiscardView1(m_d3dRenderTargetView.Get(), nullptr, 0);

	// If the device was removed either by a disconnection or a driver upgrade, we
	// must recreate all device resources.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
#ifdef _DEBUG
		char buff[64] = {};
		sprintf_s(buff, "Device Lost on Present: Reason code 0x%08X\n",
			static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? m_d3dDevice->GetDeviceRemovedReason() : hr));
		OutputDebugStringA(buff);
#endif
		HandleDeviceLost();
	}
	else if (hr == DXGI_ERROR_INVALID_CALL && m_enableVsync != m_swapchainVsync) {
		// Swap chain vsync mismatch, try to reset
		Utils::Logf("Present() failed due to swapchain vsync mismatch (enableVsync=%d != swapchainVsync=%d)\n",
			m_enableVsync, m_swapchainVsync);
		HandleDeviceLost();
	}
	else {
		DX::ThrowIfFailed(hr);
	}
}

//Thank you tunip3 for https://github.com/libretro/RetroArch/pull/13406/files
//Check if we are running on Xbox
bool is_running_on_xbox(void)
{
	Platform::String^ device_family = Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily;
	return (device_family == L"Windows.Xbox");
}

bool DX::DeviceResources::isXbox()
{
	GAMING_DEVICE_MODEL_INFORMATION info;
	if (FAILED(GetGamingDeviceModelInformation(&info))) {
		return false;
	}

	if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT) {
		return true;
	}
	return false;
}

int DX::DeviceResources::uwp_get_height()
{
	if (is_running_on_xbox())
	{
		const Windows::Graphics::Display::Core::HdmiDisplayInformation^ hdi = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
		if (hdi)
			//720p on HDMI = 1080p on UWP Window - We should handle this
			return std::max((UINT)1080, Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView()->GetCurrentDisplayMode()->ResolutionHeightInRawPixels);
	}
	const LONG32 resolution_scale = static_cast<LONG32>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->ResolutionScale);
	auto surface_scale = static_cast<float>(resolution_scale) / 100.0f;
	return static_cast<LONG32>(CoreWindow::GetForCurrentThread()->Bounds.Height * surface_scale);
}

int DX::DeviceResources::uwp_get_width()
{
	if (is_running_on_xbox())
	{
		const Windows::Graphics::Display::Core::HdmiDisplayInformation^ hdi = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
		if (hdi)
			//720p on HDMI = 1080p on UWP Window - We should handle this
			return std::max((UINT)1920, Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView()->GetCurrentDisplayMode()->ResolutionWidthInRawPixels);
	}
	const LONG32 resolution_scale = static_cast<LONG32>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->ResolutionScale);
	auto surface_scale = static_cast<float>(resolution_scale) / 100.0f;
	return static_cast<LONG32>(CoreWindow::GetForCurrentThread()->Bounds.Width * surface_scale);
}

void DX::DeviceResources::GetUWPPixelDimensions(uint32_t *width, uint32_t *height)
{
	GAMING_DEVICE_MODEL_INFORMATION info = {};
	GetGamingDeviceModelInformation(&info);

	*width = 1920;
	*height = 1080;

	if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT) {
		// Running on an Xbox
		auto mode = HdmiDisplayInformation::GetForCurrentView()->GetCurrentDisplayMode();
		*width  = std::max((uint32_t)1920, mode->ResolutionWidthInRawPixels);
		*height = std::max((uint32_t)1080, mode->ResolutionHeightInRawPixels);
	}
	else {
		// Running in Windows
		const LONG32 resolution_scale = static_cast<LONG32>(DisplayInformation::GetForCurrentView()->ResolutionScale);
		auto surface_scale = static_cast<float>(resolution_scale) / 100.0f;
		*width  = (uint32_t)CoreWindow::GetForCurrentThread()->Bounds.Width * surface_scale;
		*height = (uint32_t)CoreWindow::GetForCurrentThread()->Bounds.Height * surface_scale;
	}
}

double DX::DeviceResources::GetUWPRefreshRate()
{
	GAMING_DEVICE_MODEL_INFORMATION info = {};
	GetGamingDeviceModelInformation(&info);

	double refreshRate = 0.0f;

	if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT) {
		// Running on Xbox
		refreshRate = HdmiDisplayInformation::GetForCurrentView()->GetCurrentDisplayMode()->RefreshRate;
	}
	else {
		// It seems difficult to get the refresh rate in Windows, TODO
		refreshRate = 120.0;
	}

	return refreshRate;
}

void DX::DeviceResources::ImGui_Init(const ComPtr<ISwapChainPanelNative>& panelNative, float dpi)
{
	if (!m_imguiRunning) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", m_pixelHeight == 2160 ? 36.0f : 18.0f);

		io.ConfigFlags |= ImGuiConfigFlags_NoKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
		io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad; // disable gamepad

		// Setup viewport
		io.DisplayFramebufferScale = { dpi, dpi };

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		ImGui_ImplUwp_InitForSwapChainPanel(static_cast<void*>(panelNative.Get()));
		ImGui_ImplDX11_Init(m_d3dDevice.Get(), m_d3dContext.Get());

		m_imguiRunning = true;
	}
}

void DX::DeviceResources::ImGui_Deinit()
{
	if (m_imguiRunning) {
		ImGuiPlots::instance().clearData();

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplUwp_Shutdown();
		ImGui::DestroyContext();

		m_imguiRunning = false;
	}
}
