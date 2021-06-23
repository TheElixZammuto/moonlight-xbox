#pragma once

#include <ppltasks.h>	// Per create_task

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// Imposta un punto di interruzione in questa riga per intercettare gli errori di API Win32.
			throw Platform::Exception::CreateException(hr);
		}
	}

	// Funzione che legge da un file binario in modo asincrono.
	inline Concurrency::task<std::vector<byte>> ReadDataAsync(const std::wstring& filename)
	{
		using namespace Windows::Storage;
		using namespace Concurrency;

		auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;

		return create_task(folder->GetFileAsync(Platform::StringReference(filename.c_str()))).then([] (StorageFile^ file) 
		{
			return FileIO::ReadBufferAsync(file);
		}).then([] (Streams::IBuffer^ fileBuffer) -> std::vector<byte> 
		{
			std::vector<byte> returnBuffer;
			returnBuffer.resize(fileBuffer->Length);
			Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(Platform::ArrayReference<byte>(returnBuffer.data(), fileBuffer->Length));
			return returnBuffer;
		});
	}

	// Converte una lunghezza in DIP (Device Independent Pixel) in una lunghezza in pixel fisici.
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // Arrotonda all'intero più vicino.
	}

#if defined(_DEBUG)
	// Verifica il supporto dei livelli SDK.
	inline bool SdkLayersAvailable()
	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_NULL,       // Non è necessario creare un dispositivo hardware reale.
			0,
			D3D11_CREATE_DEVICE_DEBUG,  // Verificare i livelli SDK.
			nullptr,                    // È valido qualsiasi livello di funzionalità.
			0,
			D3D11_SDK_VERSION,          // Impostare sempre questo parametro su D3D11_SDK_VERSION per le app di Microsoft Store.
			nullptr,                    // Non è necessario mantenere il riferimento al dispositivo D3D.
			nullptr,                    // Non è necessario conoscere il livello di funzionalità.
			nullptr                     // Non è necessario mantenere il riferimento al contesto del dispositivo D3D.
			);

		return SUCCEEDED(hr);
	}
#endif
}
