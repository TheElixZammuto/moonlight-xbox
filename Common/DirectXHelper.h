#pragma once

#include <ppltasks.h>	// For create_task
#include <Utils.hpp>

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// Set a breakpoint on this line to catch Win32 API errors.
			char msg[4096];
			sprintf(msg, "Got generic error from HRESULT: %x\n", hr);
			moonlight_xbox_dx::Utils::Log(msg);

			throw Platform::Exception::CreateException(hr);
		}
	}

	inline void ThrowIfFailed(HRESULT hr,const char *reason)
	{
		if (FAILED(hr))
		{
			// Set a breakpoint on this line to catch Win32 API errors.
			char msg[4096];
			sprintf(msg, "Got generic error from %s: %x\n", reason, hr);
			moonlight_xbox_dx::Utils::Log(msg);

			throw Platform::Exception::CreateException(hr);
		}
	}

	// Function that reads from a binary file asynchronously.
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

	// Converts a length in device-independent pixels (DIPs) to a length in physical pixels.
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
	}

#if defined(_DEBUG)
	// Check for SDK Layer support.
	inline bool SdkLayersAvailable()
	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
			0,
			D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
			nullptr,                    // Any feature level will do.
			0,
			D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Microsoft Store apps.
			nullptr,                    // No need to keep the D3D device reference.
			nullptr,                    // No need to know the feature level.
			nullptr                     // No need to keep the D3D device context reference.
			);

		return SUCCEEDED(hr);
	}
#endif

    class DeviceResources;

    // Used to track time spent executing Gpu work
    class GpuPerformanceTimer
    {
    public:
        GpuPerformanceTimer(
            const std::shared_ptr<DX::DeviceResources>& deviceResources);

        void StartTimerForFrame();
        void EndTimerForFrame();

        float GetFrameTime() const;
        float GetAvgFrameTime() const;
        float GetMinFrameTime() const;
        float GetMaxFrameTime() const;

    private:
        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        static constexpr uint32_t QueryCount = 5u;

        Microsoft::WRL::ComPtr<ID3D11Query> m_startTimestampQuery[QueryCount];
        Microsoft::WRL::ComPtr<ID3D11Query> m_endTimestampQuery[QueryCount];
        Microsoft::WRL::ComPtr<ID3D11Query> m_disjointQuery[QueryCount];

        uint32_t m_currentQuery = 0u;
        uint32_t m_currentFrameIndex = 0u;

        static constexpr uint32_t TimeHistoryCount = 64u;

        uint32_t m_processingTimeHistoryIndex = 0u;
        float m_processingTimeHistory[TimeHistoryCount] = {};

        float m_processingTimeMs = 0.0f;
        float m_processingTimeAvgMs = 0.0f;
        float m_processingTimeMinMs = +std::numeric_limits<float>::infinity();
        float m_processingTimeMaxMs = -std::numeric_limits<float>::infinity();
    };
}
