#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"

using namespace D2D1;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;

namespace DisplayMetrics
{
	// Per il rendering con display ad alta risoluzione può essere richiesta una notevole quantità GPU e di energia della batteria.
	// La batteria di telefoni ad alta risoluzione, ad esempio, potrebbe durare meno se
	// i giochi provano a eseguire il rendering a 60 fotogrammi al secondo alla massima fedeltà.
	// La decisione di eseguire il rendering alla massima fedeltà in tutte le piattaforme e per tutti i fattori di forma
	// deve essere opportunamente valutata.
	static const bool SupportHighResolutions = false;

	// Soglie predefinite che definiscono un display ad alta risoluzione. Se
	// vengono superate e SupportHighResolutions è impostato su false, le dimensioni verranno ridotte
	// del 50%.
	static const float DpiThreshold = 192.0f;		// 200% del display desktop standard.
	static const float WidthThreshold = 1920.0f;	// Larghezza pari a 1080p.
	static const float HeightThreshold = 1080.0f;	// Altezza pari a 1080p.
};

// Costanti usate per calcolare le rotazioni dello schermo
namespace ScreenRotation
{
	// Rotazione Z di 0 gradi
	static const XMFLOAT4X4 Rotation0(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Rotazione Z di 90 gradi
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Rotazione Z di 180 gradi
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Rotazione Z di 270 gradi
	static const XMFLOAT4X4 Rotation270(
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
};

// Costruttore per DeviceResources.
DX::DeviceResources::DeviceResources() :
	m_screenViewport(),
	m_d3dFeatureLevel(D3D_FEATURE_LEVEL_9_1),
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_logicalSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_deviceNotify(nullptr)
{
	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

// Configura le risorse che non dipendono dal dispositivo Direct3D.
void DX::DeviceResources::CreateDeviceIndependentResources()
{
	// Inizializza le risorse Direct2D.
	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
	// Se il progetto si trova in una build di debug, abilitare il debug Direct2D tramite livelli SDK.
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	// Inizializza la factory Direct2D.
	DX::ThrowIfFailed(
		D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory3),
			&options,
			&m_d2dFactory
			)
		);

	// Inizializza la factory DirectWrite.
	DX::ThrowIfFailed(
		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory3),
			&m_dwriteFactory
			)
		);

	// Inizializza la factory di Windows Imaging Component (WIC).
	DX::ThrowIfFailed(
		CoCreateInstance(
			CLSID_WICImagingFactory2,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_wicFactory)
			)
		);
}

// Configura il dispositivo Direct3D e archivia gli handle nel dispositivo e nel contesto del dispositivo.
void DX::DeviceResources::CreateDeviceResources() 
{
	// Questo flag aggiunge il supporto per superfici con un ordinamento dei canali colore diverso
	// da quello predefinito dell'API. È obbligatorio per la compatibilità con Direct2D.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (DX::SdkLayersAvailable())
	{
		// Se il progetto si trova in una build di debug, questo flag consente di abilitare il debug tramite i livelli SDK.
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

	// Questa matrice consente di definire il set di livelli di funzionalità hardware DirectX che verrà supportato dall'app.
	// Si noti che l'ordinamento deve essere mantenuto.
	// Ricordare di dichiarare il livello di funzionalità minimo richiesto nella descrizione
	// descrizione. Si presuppone che tutte le applicazioni supportino la versione 9.1 a eccezione dei casi in cui è indicato diversamente.
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

	// Crea l'oggetto dispositivo dell'API Direct3D 11 e un contesto corrispondente.
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	HRESULT hr = D3D11CreateDevice(
		nullptr,					// Specifica nullptr per usare l'adattatore predefinito.
		D3D_DRIVER_TYPE_HARDWARE,	// Crea un dispositivo usando il driver di grafica hardware.
		0,							// Deve essere 0 a meno che il driver non sia D3D_DRIVER_TYPE_SOFTWARE.
		creationFlags,				// Imposta i flag di compatibilità Direct2D e di debug.
		featureLevels,				// Elenco di livelli di funzionalità che l'app è in grado di supportare.
		ARRAYSIZE(featureLevels),	// Dimensioni dell'elenco precedente.
		D3D11_SDK_VERSION,			// Impostare sempre questo parametro su D3D11_SDK_VERSION per le app di Microsoft Store.
		&device,					// Restituisce il dispositivo Direct3D creato.
		&m_d3dFeatureLevel,			// Restituisce il livello di funzionalità del dispositivo creato.
		&context					// Restituisce il contesto immediato del dispositivo.
		);

	if (FAILED(hr))
	{
		// Se l'inizializzazione non riesce, eseguire il fallback al dispositivo WARP.
		// Per altre informazioni su WARP, vedere: 
		// https://go.microsoft.com/fwlink/?LinkId=286690
		DX::ThrowIfFailed(
			D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP, // Crea un dispositivo WARP invece di un dispositivo hardware.
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

	// Archivia i puntatori nel dispositivo dell'API Direct3D 11.3 e nel contesto immediato.
	DX::ThrowIfFailed(
		device.As(&m_d3dDevice)
		);

	DX::ThrowIfFailed(
		context.As(&m_d3dContext)
		);

	// Crea l'oggetto dispositivo Direct2D e un contesto corrispondente.
	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(
		m_d3dDevice.As(&dxgiDevice)
		);

	DX::ThrowIfFailed(
		m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice)
		);

	DX::ThrowIfFailed(
		m_d2dDevice->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			&m_d2dContext
			)
		);
}

// Queste risorse devono essere ricreate ogni volta che cambiano le dimensioni della finestra.
void DX::DeviceResources::CreateWindowSizeDependentResources() 
{
	// Cancella il contesto specifico precedente delle dimensioni della finestra.
	ID3D11RenderTargetView* nullViews[] = {nullptr};
	m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
	m_d3dRenderTargetView = nullptr;
	m_d2dContext->SetTarget(nullptr);
	m_d2dTargetBitmap = nullptr;
	m_d3dDepthStencilView = nullptr;
	m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

	UpdateRenderTargetSize();

	// La larghezza e l'altezza della catena di scambio devono basarsi sulla
	// la larghezza e l'altezza della finestra con orientamento nativo. Se l'orientamento della finestra non è nativo,
	// le dimensioni devono essere invertite.
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;

	if (m_swapChain != nullptr)
	{
		// Se la catena di scambio esiste già, la ridimensiona.
		HRESULT hr = m_swapChain->ResizeBuffers(
			2, // Catena di scambio con doppio buffer.
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// Se il dispositivo è stato rimosso per un qualsiasi motivo, è necessario creare un nuovo dispositivo e la catena di scambio.
			HandleDeviceLost();

			// La configurazione è completata. Non continuare l'esecuzione del metodo. Questo metodo verrà immesso nuovamente da HandleDeviceLost 
			// e configurerà correttamente il nuovo dispositivo.
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// In alternativa, ne creerà uno nuovo con lo stesso adattatore del dispositivo Direct3D esistente.
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// Deve corrispondere alle dimensioni della finestra.
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// Questo è il formato più comune di catena di scambio.
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;								// Non usare il campionamento multiplo.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;									// Usa il doppio buffer per ridurre al minimo la latenza.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// Tutte le app di Microsoft Store devono usare questo elemento SwapEffect.
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// Questa sequenza ottiene la factory DXGI che è stata usata per creare il dispositivo Direct3D precedente.
		ComPtr<IDXGIDevice3> dxgiDevice;
		DX::ThrowIfFailed(
			m_d3dDevice.As(&dxgiDevice)
			);

		ComPtr<IDXGIAdapter> dxgiAdapter;
		DX::ThrowIfFailed(
			dxgiDevice->GetAdapter(&dxgiAdapter)
			);

		ComPtr<IDXGIFactory4> dxgiFactory;
		DX::ThrowIfFailed(
			dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
			);

		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			dxgiFactory->CreateSwapChainForCoreWindow(
				m_d3dDevice.Get(),
				reinterpret_cast<IUnknown*>(m_window.Get()),
				&swapChainDesc,
				nullptr,
				&swapChain
				)
			);
		DX::ThrowIfFailed(
			swapChain.As(&m_swapChain)
			);

		// Accertarsi che DXGI non accodi più di un frame alla volta. In questo modo si riduce la latenza e
		// si garantisce che l'applicazione esegua il rendering dopo ogni VSync, riducendo al minimo il consumo di energia.
		DX::ThrowIfFailed(
			dxgiDevice->SetMaximumFrameLatency(1)
			);
	}

	// Imposta l'orientamento corretto per la catena di scambio e genera
	// trasformazioni di matrice 2D e 3D per il rendering nella catena di scambio ruotata.
	// Si noti che gli angoli di rotazione per le trasformazioni 2D e 3D sono diversi tra loro.
	// Ciò è dovuto alla differenza negli spazi di coordinate. Inoltre,
	// la matrice 3D viene specificata in modo esplicito per evitare errori di arrotondamento.

	switch (displayRotation)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
		m_orientationTransform2D = Matrix3x2F::Identity();
		m_orientationTransform3D = ScreenRotation::Rotation0;
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(90.0f) *
			Matrix3x2F::Translation(m_logicalSize.Height, 0.0f);
		m_orientationTransform3D = ScreenRotation::Rotation270;
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(180.0f) *
			Matrix3x2F::Translation(m_logicalSize.Width, m_logicalSize.Height);
		m_orientationTransform3D = ScreenRotation::Rotation180;
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(270.0f) *
			Matrix3x2F::Translation(0.0f, m_logicalSize.Width);
		m_orientationTransform3D = ScreenRotation::Rotation90;
		break;

	default:
		throw ref new FailureException();
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
		);

	// Crea una visualizzazione di destinazione del rendering del buffer nascosto della catena di scambio.
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

	// Crea una visualizzazione del depth stencil per l'uso, se necessario, con il rendering 3D.
	CD3D11_TEXTURE2D_DESC1 depthStencilDesc(
		DXGI_FORMAT_D24_UNORM_S8_UINT, 
		lround(m_d3dRenderTargetSize.Width),
		lround(m_d3dRenderTargetSize.Height),
		1, // Questa visualizzazione depth stencil ha una sola trama.
		1, // Usa un solo livello mipmap.
		D3D11_BIND_DEPTH_STENCIL
		);

	ComPtr<ID3D11Texture2D1> depthStencil;
	DX::ThrowIfFailed(
		m_d3dDevice->CreateTexture2D1(
			&depthStencilDesc,
			nullptr,
			&depthStencil
			)
		);

	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
	DX::ThrowIfFailed(
		m_d3dDevice->CreateDepthStencilView(
			depthStencil.Get(),
			&depthStencilViewDesc,
			&m_d3dDepthStencilView
			)
		);
	
	// Imposta il viewport di rendering 3D in modo da usare come destinazione l'intera finestra.
	m_screenViewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		m_d3dRenderTargetSize.Width,
		m_d3dRenderTargetSize.Height
		);

	m_d3dContext->RSSetViewports(1, &m_screenViewport);

	// Crea una bitmap di destinazione Direct2D associata
	// al buffer nascosto della catena di scambio e la imposta come destinazione corrente.
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = 
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			m_dpi,
			m_dpi
			);

	ComPtr<IDXGISurface2> dxgiBackBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer))
		);

	DX::ThrowIfFailed(
		m_d2dContext->CreateBitmapFromDxgiSurface(
			dxgiBackBuffer.Get(),
			&bitmapProperties,
			&m_d2dTargetBitmap
			)
		);

	m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
	m_d2dContext->SetDpi(m_effectiveDpi, m_effectiveDpi);

	// L'anti-aliasing del testo in gradazioni di grigio è consigliato per tutte le app di Microsoft Store.
	m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

// Determina le dimensioni della destinazione di rendering e indica se verrà ridimensionata.
void DX::DeviceResources::UpdateRenderTargetSize()
{
	m_effectiveDpi = m_dpi;

	// Per ottimizzare la durata della batteria di dispositivi ad alta risoluzione, esegue il rendering in base a una destinazione di rendering ridotta
	// e consente alla GPU di ridimensionare l'output quando viene visualizzato.
	if (!DisplayMetrics::SupportHighResolutions && m_dpi > DisplayMetrics::DpiThreshold)
	{
		float width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_dpi);
		float height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_dpi);

		// Quando il dispositivo è impostato per l'orientamento verticale, l'altezza è maggiore della larghezza. Confrontare la
		// dimensione maggiore con la soglia della larghezza e la dimensione minore
		// con quella dell'altezza.
		if (max(width, height) > DisplayMetrics::WidthThreshold && min(width, height) > DisplayMetrics::HeightThreshold)
		{
			// Per ridimensionare l'app, viene modificato il valore del DPI effettivo. Le dimensioni logiche non vengono modificate.
			m_effectiveDpi /= 2.0f;
		}
	}

	// Calcolare le dimensioni della destinazione di rendering necessarie in pixel.
	m_outputSize.Width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_effectiveDpi);

	// Impedisce la creazione di contenuto DirectX con dimensioni pari a zero.
	m_outputSize.Width = max(m_outputSize.Width, 1);
	m_outputSize.Height = max(m_outputSize.Height, 1);
}

// Questo metodo viene chiamato quando viene creato o ricreato l'oggetto CoreWindow.
void DX::DeviceResources::SetWindow(CoreWindow^ window)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_window = window;
	m_logicalSize = Windows::Foundation::Size(window->Bounds.Width, window->Bounds.Height);
	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;
	m_dpi = currentDisplayInformation->LogicalDpi;
	m_d2dContext->SetDpi(m_dpi, m_dpi);

	CreateWindowSizeDependentResources();
}

// Questo metodo viene chiamato nel gestore eventi per l'evento SizeChanged.
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

// Questo metodo viene chiamato nel gestore eventi per l'evento DpiChanged.
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;

		// Quando si modifica il valore DPI dello schermo, vengono modificate anche le dimensioni logiche della finestra (misurate in DIP) ed è necessario aggiornarle.
		m_logicalSize = Windows::Foundation::Size(m_window->Bounds.Width, m_window->Bounds.Height);

		m_d2dContext->SetDpi(m_dpi, m_dpi);
		CreateWindowSizeDependentResources();
	}
}

// Questo metodo viene chiamato nel gestore eventi per l'evento OrientationChanged.
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// Questo metodo viene chiamato nel gestore eventi per l'evento DisplayContentsInvalidated.
void DX::DeviceResources::ValidateDevice()
{
	// Il dispositivo D3D non è più valido se l'adattatore predefinito viene cambiato dopo che il dispositivo
	// è stato creato o se il dispositivo è stato rimosso.

	// Ottenere innanzitutto le informazioni per l'adattatore predefinito quando il dispositivo è stato creato.

	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

	ComPtr<IDXGIAdapter> deviceAdapter;
	DX::ThrowIfFailed(dxgiDevice->GetAdapter(&deviceAdapter));

	ComPtr<IDXGIFactory4> deviceFactory;
	DX::ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&deviceFactory)));

	ComPtr<IDXGIAdapter1> previousDefaultAdapter;
	DX::ThrowIfFailed(deviceFactory->EnumAdapters1(0, &previousDefaultAdapter));

	DXGI_ADAPTER_DESC1 previousDesc;
	DX::ThrowIfFailed(previousDefaultAdapter->GetDesc1(&previousDesc));

	// Quindi, ottenere le informazioni per l'adattatore predefinito corrente.

	ComPtr<IDXGIFactory4> currentFactory;
	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC1 currentDesc;
	DX::ThrowIfFailed(currentDefaultAdapter->GetDesc1(&currentDesc));

	// Se gli LUID adattatore non corrispondono o se il dispositivo segnala che è stato rimosso,
	// è necessario creare un nuovo dispositivo D3D.

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		// Rilascia i riferimenti alle risorse relative al dispositivo precedente.
		dxgiDevice = nullptr;
		deviceAdapter = nullptr;
		deviceFactory = nullptr;
		previousDefaultAdapter = nullptr;

		// Crea un nuovo dispositivo e la catena di scambio.
		HandleDeviceLost();
	}
}

// Ricrea tutte le risorse del dispositivo e ne reimposta lo stato corrente.
void DX::DeviceResources::HandleDeviceLost()
{
	m_swapChain = nullptr;

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceLost();
	}

	CreateDeviceResources();
	m_d2dContext->SetDpi(m_dpi, m_dpi);
	CreateWindowSizeDependentResources();

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceRestored();
	}
}

// Registra DeviceNotify per essere informato sulla creazione e sulla perdita di dispositivi.
void DX::DeviceResources::RegisterDeviceNotify(DX::IDeviceNotify* deviceNotify)
{
	m_deviceNotify = deviceNotify;
}

// Chiamare il metodo quando l'app viene sospesa. Fornisce un suggerimento al driver indicante che l'app 
// sta passando a uno stato di inattività e che è possibile recuperare i buffer temporanei per usarli in altre app.
void DX::DeviceResources::Trim()
{
	ComPtr<IDXGIDevice3> dxgiDevice;
	m_d3dDevice.As(&dxgiDevice);

	dxgiDevice->Trim();
}

// Presenta il contenuto della catena di scambio sullo schermo.
void DX::DeviceResources::Present() 
{
	// Il primo argomento indica a DXGI di bloccarsi fino a VSync, mettendo l'applicazione
	// in stato di attesa fino al prossimo VSync. Ciò garantisce che non vengano sprecati cicli per il rendering
	// di fotogrammi che non verranno mai visualizzati sullo schermo.
	DXGI_PRESENT_PARAMETERS parameters = { 0 };
	HRESULT hr = m_swapChain->Present1(1, 0, &parameters);

	// Rimuove il contenuto della destinazione di rendering.
	// Questa operazione è valida solo se il contenuto esistente verrà interamente
	// sovrascritto. Se si usano rettangoli dirty o scroll, questa chiamata deve essere rimossa.
	m_d3dContext->DiscardView1(m_d3dRenderTargetView.Get(), nullptr, 0);

	// Ignora il contenuto del depth stencil.
	m_d3dContext->DiscardView1(m_d3dDepthStencilView.Get(), nullptr, 0);

	// Se il dispositivo è stato rimosso in seguito a un aggiornamento del driver o a una disconnessione, 
	// si devono ricreare tutte le risorse del dispositivo.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		HandleDeviceLost();
	}
	else
	{
		DX::ThrowIfFailed(hr);
	}
}

// Questo metodo determina la rotazione tra l'orientamento nativo del dispositivo di visualizzazione e
// l'orientamento della visualizzazione corrente.
DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

	// Nota: il valore di NativeOrientation può essere solo Landscape o Portrait anche se
	// il valore dell'enumerazione DisplayOrientations è diverso.
	switch (m_nativeOrientation)
	{
	case DisplayOrientations::Landscape:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;
		}
		break;

	case DisplayOrientations::Portrait:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;
		}
		break;
	}
	return rotation;
}