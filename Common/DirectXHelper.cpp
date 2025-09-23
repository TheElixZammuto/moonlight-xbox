#include "pch.h"
#include "Common\DirectXHelper.h"
#include "Common\DeviceResources.h"
#include <numeric>

using namespace Microsoft::WRL;

DX::GpuPerformanceTimer::GpuPerformanceTimer(
    const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources)
{
    ID3D11Device* device = m_deviceResources->GetD3DDevice();

    const D3D11_QUERY_DESC timestampQuery = CD3D11_QUERY_DESC(D3D11_QUERY_TIMESTAMP);
    const D3D11_QUERY_DESC disjointQuery = CD3D11_QUERY_DESC(D3D11_QUERY_TIMESTAMP_DISJOINT);

    for (uint32_t i = 0; i < QueryCount; ++i)
    {
        device->CreateQuery(&timestampQuery, m_startTimestampQuery[i].ReleaseAndGetAddressOf());
        device->CreateQuery(&timestampQuery, m_endTimestampQuery[i].ReleaseAndGetAddressOf());
        device->CreateQuery(&disjointQuery, m_disjointQuery[i].ReleaseAndGetAddressOf());
    }
}

void DX::GpuPerformanceTimer::StartTimerForFrame()
{
    ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

    // Just some book-keeping to see how long this algorithm is running
    m_currentQuery = (m_currentQuery + 1) % QueryCount;
    const uint32_t ReadQuery = (m_currentQuery + 1) % QueryCount;

    if (m_currentFrameIndex >= QueryCount)
    {
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
        while (context->GetData(m_disjointQuery[ReadQuery].Get(), &disjointData, sizeof(disjointData), 0) != S_OK);

        if (disjointData.Disjoint == 0)
        {
            uint64_t start;
            while (context->GetData(m_startTimestampQuery[ReadQuery].Get(), &start, sizeof(start), 0) != S_OK);

            uint64_t end;
            while (context->GetData(m_endTimestampQuery[ReadQuery].Get(), &end, sizeof(end), 0) != S_OK);

            constexpr float secondsToMilliseconds = 1000.0f;
            const float ticksToSeconds = 1.0f / static_cast<float>(disjointData.Frequency);
            const uint64_t ticksElapsed = end - start;

            m_processingTimeMs = static_cast<float>(ticksElapsed) * ticksToSeconds * secondsToMilliseconds;

            // Reset every ~10 seconds so the min/max has a chance to update
            if (m_currentFrameIndex % 300 == 0)
            {
                m_processingTimeMinMs = +std::numeric_limits<float>::infinity();
                m_processingTimeMaxMs = -std::numeric_limits<float>::infinity();
            }

            m_processingTimeMinMs = std::min(m_processingTimeMinMs, m_processingTimeMs);
            m_processingTimeMaxMs = std::max(m_processingTimeMaxMs, m_processingTimeMs);
        }
        else
        {
            m_processingTimeMs = std::numeric_limits<float>::quiet_NaN();;
        }

        m_processingTimeHistory[m_processingTimeHistoryIndex] = m_processingTimeMs;
        m_processingTimeHistoryIndex = (m_processingTimeHistoryIndex + 1) % TimeHistoryCount;

        m_processingTimeAvgMs = std::accumulate(m_processingTimeHistory, m_processingTimeHistory + TimeHistoryCount, 0.0f) / static_cast<float>(TimeHistoryCount);
    }

    ++m_currentFrameIndex;

    context->Begin(m_disjointQuery[m_currentQuery].Get());
    context->End(m_startTimestampQuery[m_currentQuery].Get());
}

void DX::GpuPerformanceTimer::EndTimerForFrame()
{
    ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();
    context->End(m_endTimestampQuery[m_currentQuery].Get());
    context->End(m_disjointQuery[m_currentQuery].Get());
}

float DX::GpuPerformanceTimer::GetFrameTime() const
{
    return m_processingTimeMs;
}

float DX::GpuPerformanceTimer::GetAvgFrameTime() const
{
    return m_processingTimeAvgMs;
}

float DX::GpuPerformanceTimer::GetMinFrameTime() const
{
    return m_processingTimeMinMs;
}

float DX::GpuPerformanceTimer::GetMaxFrameTime() const
{
    return m_processingTimeMaxMs;
}
