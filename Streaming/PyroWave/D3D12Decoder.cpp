#include "pch.h"
#include "D3D12Decoder.h"
#include "pyrowave_d3d12.h"
#include <Utils.hpp>

using Microsoft::WRL::ComPtr;

namespace moonlight_xbox_dx {
namespace PyroWaveD3D12 {

namespace {
void LogMessage(void *, const char *msg) {
	Utils::Logf("PyroWave D3D12: %s\n", msg);
}
} // namespace

Decoder::~Decoder() {
#ifdef TRACY_ENABLE_D3D12
	// we've lost the connection, clean up
	if (g_tracyCtx12) {
		TracyD3D12Destroy(g_tracyCtx12);
		g_tracyCtx12 = nullptr;
	}
#endif

	// Drain the queue so nothing below is released while the GPU still uses it.
	if (m_queue && m_decodeFence && m_waitEvent) {
		m_queue->Signal(m_decodeFence.Get(), ++m_decodeValue);
		if (SUCCEEDED(m_decodeFence->SetEventOnCompletion(m_decodeValue, m_waitEvent)))
			WaitForSingleObject(m_waitEvent, 2000);
	}
	if (m_pwDecoder)
		pyrowave_d3d12_decoder_destroy(m_pwDecoder);
	if (m_pwDevice)
		pyrowave_d3d12_device_destroy(m_pwDevice);
	if (m_waitEvent)
		CloseHandle(m_waitEvent);
}

bool Decoder::Init(ID3D11Device *device11, ID3D11DeviceContext *context11, int width, int height, bool chroma444,
                   int poolSize) {
	ComPtr<ID3D11Device5> device5;
	if (FAILED(device11->QueryInterface(IID_PPV_ARGS(&device5))) ||
	    FAILED(context11->QueryInterface(IID_PPV_ARGS(&m_context4)))) {
		Utils::Log("PyroWave D3D12: no ID3D11Device5/ID3D11DeviceContext4 (shared fences)\n");
		return false;
	}

	// Same adapter as the renderer's D3D11 device.
	ComPtr<IDXGIDevice> dxgiDevice;
	ComPtr<IDXGIAdapter> adapter;
	if (SUCCEEDED(device11->QueryInterface(IID_PPV_ARGS(&dxgiDevice))))
		dxgiDevice->GetAdapter(&adapter);
	HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
	if (FAILED(hr)) {
		Utils::Logf("PyroWave D3D12: D3D12CreateDevice failed (hr 0x%08x)\n", hr);
		return false;
	}

	if (!pyrowave_d3d12_device_is_supported(m_device.Get())) {
		Utils::Log("PyroWave D3D12: device lacks Shader Model 6.4 wave operations\n");
		return false;
	}

	pyrowave_d3d12_device_create_info devInfo = {};
	devInfo.d3d12_device = m_device.Get();
	devInfo.message_callback = LogMessage;
	pyrowave_d3d12_result res = pyrowave_d3d12_device_create(&devInfo, &m_pwDevice);
	if (res != PYROWAVE_D3D12_SUCCESS) {
		Utils::Logf("PyroWave D3D12: device create failed: %s\n", pyrowave_d3d12_result_to_string(res));
		return false;
	}

	pyrowave_d3d12_decoder_create_info decInfo = {};
	decInfo.device = m_pwDevice;
	decInfo.width = width;
	decInfo.height = height;
	decInfo.chroma = chroma444 ? PYROWAVE_D3D12_CHROMA_SUBSAMPLING_444 : PYROWAVE_D3D12_CHROMA_SUBSAMPLING_420;
	res = pyrowave_d3d12_decoder_create(&decInfo, &m_pwDecoder);
	if (res != PYROWAVE_D3D12_SUCCESS) {
		Utils::Logf("PyroWave D3D12: decoder create failed: %s\n", pyrowave_d3d12_result_to_string(res));
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC qd = {};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	if (FAILED(m_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_queue))))
		return false;
	m_queue->SetName(L"PyroWave decode");
	for (auto &s : m_submissions) {
		if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&s.allocator))))
			return false;
	}
	if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_submissions[0].allocator.Get(),
	                                       nullptr, IID_PPV_ARGS(&m_list))))
		return false;
	m_list->Close();
	m_waitEvent = CreateEventExW(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	if (!m_waitEvent)
		return false;

	// Decode-done fence: D3D12 -> D3D11.
	HANDLE handle = nullptr;
	hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_decodeFence));
	if (SUCCEEDED(hr))
		hr = m_device->CreateSharedHandle(m_decodeFence.Get(), nullptr, GENERIC_ALL, nullptr, &handle);
	if (SUCCEEDED(hr))
		hr = device5->OpenSharedFence(handle, IID_PPV_ARGS(&m_decodeFenceOn11));
	if (handle)
		CloseHandle(handle);
	if (FAILED(hr)) {
		Utils::Logf("PyroWave D3D12: sharing the decode fence with D3D11 failed (hr 0x%08x)\n", hr);
		return false;
	}

	// Release fence: D3D11 -> D3D12.
	handle = nullptr;
	hr = device5->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_releaseFence));
	if (SUCCEEDED(hr))
		hr = m_releaseFence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &handle);
	if (SUCCEEDED(hr))
		hr = m_device->OpenSharedHandle(handle, IID_PPV_ARGS(&m_releaseFenceOn12));
	if (handle)
		CloseHandle(handle);
	if (FAILED(hr)) {
		Utils::Logf("PyroWave D3D12: sharing the release fence with D3D12 failed (hr 0x%08x)\n", hr);
		return false;
	}

	// GPU timing.
	D3D12_QUERY_HEAP_DESC qh = {};
	qh.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	qh.Count = 2 * kInFlight;
	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC rd = {};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = sizeof(UINT64) * 2 * kInFlight;
	rd.Height = 1;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	if (FAILED(m_device->CreateQueryHeap(&qh, IID_PPV_ARGS(&m_timestamps))) ||
	    FAILED(m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST,
	                                             nullptr, IID_PPV_ARGS(&m_timestampReadback))) ||
	    FAILED(m_queue->GetTimestampFrequency(&m_timestampFrequency))) {
		m_timestamps.Reset();
		m_timestampReadback.Reset();
	}

#ifdef TRACY_ENABLE_D3D12
	g_tracyCtx12 = TracyD3D12Context(m_device.Get(), m_queue.Get());
#endif

	m_planes.resize(poolSize);
	return true;
}

bool Decoder::OpenPlanes(PyroWaveFrameSet *set) {
	if (set->index < 0 || set->index >= (int)m_planes.size())
		return false;
	Planes &planes = m_planes[set->index];
	if (planes.tex[0])
		return true;

	for (int c = 0; c < 3; c++) {
		ComPtr<IDXGIResource1> resource;
		HANDLE handle = nullptr;
		HRESULT hr = set->tex[c].As(&resource);
		if (SUCCEEDED(hr))
			hr = resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
			                                  &handle);
		if (SUCCEEDED(hr))
			hr = m_device->OpenSharedHandle(handle, IID_PPV_ARGS(&planes.tex[c]));
		if (handle)
			CloseHandle(handle);
		if (FAILED(hr)) {
			Utils::Logf("PyroWave D3D12: opening plane %d of set %d failed (hr 0x%08x)\n", c, set->index, hr);
			for (auto &t : planes.tex)
				t.Reset();
			return false;
		}
	}
	return true;
}

bool Decoder::PushPacket(const void *data, size_t size) {
	return pyrowave_d3d12_decoder_push_packet(m_pwDecoder, data, size) == PYROWAVE_D3D12_SUCCESS;
}

bool Decoder::DecodeIsReady(bool allowPartialFrame, int numPristineBands, float minimumPacketRatio) const {
	return pyrowave_d3d12_decoder_decode_is_ready_with_sideband(m_pwDecoder, allowPartialFrame, numPristineBands,
	                                                            minimumPacketRatio, nullptr, 0);
}

void Decoder::Clear() {
	pyrowave_d3d12_decoder_clear(m_pwDecoder);
}

bool Decoder::Decode(ID3D11DeviceContext *context11, PyroWaveFrameSet *set) {
	if (!OpenPlanes(set))
		return false;

	// 1. Everything D3D11 has been given so far, including the renderer's reads of
	//    this set from its previous trip through the pipeline, precedes this signal.
	//    Flush so the D3D12 queue is not left waiting on buffered D3D11 commands.
	m_context4->Signal(m_releaseFence.Get(), ++m_releaseValue);
	context11->Flush();

	// Reuse the oldest allocator once the GPU has finished with it.
	const int slot = m_nextSubmission;
	Submission &sub = m_submissions[slot];
	if (m_decodeFence->GetCompletedValue() < sub.fenceValue) {
		if (SUCCEEDED(m_decodeFence->SetEventOnCompletion(sub.fenceValue, m_waitEvent)))
			WaitForSingleObject(m_waitEvent, INFINITE);
	}
	// A timing nobody polled before the slot came round again is simply dropped.
	for (auto it = m_pendingTimings.begin(); it != m_pendingTimings.end(); ++it) {
		if (*it == slot) {
			m_pendingTimings.erase(it);
			break;
		}
	}
	sub.allocator->Reset();
	m_list->Reset(sub.allocator.Get(), nullptr);

	{
		TracyD3D12Zone(g_tracyCtx12, m_list.Get(), "PyroWave wait D3D11");
		// 2. Decode on the D3D12 queue once D3D11 is done with the planes.
		m_queue->Wait(m_releaseFenceOn12.Get(), m_releaseValue);
	}

	const UINT64 decodeValue = m_decodeValue + 1;
	if (m_timestamps)
		m_list->EndQuery(m_timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2 * slot);

	{
		TracyD3D12Zone(g_tracyCtx12, m_list.Get(), "PyroWave decode");
		// The shared planes carry ALLOW_SIMULTANEOUS_ACCESS, so they promote from COMMON
		// to UNORDERED_ACCESS implicitly and decay back after execution.
		Planes &planes = m_planes[set->index];
		pyrowave_d3d12_gpu_buffers buffers = { { planes.tex[0].Get(), planes.tex[1].Get(), planes.tex[2].Get() } };
		pyrowave_d3d12_result res =
			pyrowave_d3d12_decoder_decode_gpu_buffer(m_pwDecoder, m_list.Get(), &buffers, m_decodeFence.Get(), decodeValue);
		if (res != PYROWAVE_D3D12_SUCCESS) {
			Utils::Logf("PyroWave D3D12: decode failed: %s\n", pyrowave_d3d12_result_to_string(res));
			m_list->Close();
			// Nothing was submitted; keep the queue's fence sequence consistent anyway.
			m_queue->Signal(m_decodeFence.Get(), ++m_decodeValue);
			sub.fenceValue = m_decodeValue;
			m_nextSubmission = (slot + 1) % kInFlight;
			return false;
		}
	}

	if (m_timestamps) {
		m_list->EndQuery(m_timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2 * slot + 1);
		m_list->ResolveQueryData(m_timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2 * slot, 2,
		                         m_timestampReadback.Get(), sizeof(UINT64) * 2 * slot);
		m_pendingTimings.push_back(slot);
	}

	m_list->Close();
	ID3D12CommandList *lists[] = { m_list.Get() };
	m_queue->ExecuteCommandLists(1, lists);
	m_queue->Signal(m_decodeFence.Get(), decodeValue);
	m_decodeValue = decodeValue;
	sub.fenceValue = decodeValue;
	m_nextSubmission = (slot + 1) % kInFlight;

	{
		TracyD3D12Zone(g_tracyCtx12, m_list.Get(), "PyroWave wait decode fence");
		// 3. Later D3D11 work (the draw that presents this frame) waits for the decode.
		m_context4->Wait(m_decodeFenceOn11.Get(), decodeValue);
	}

	TracyD3D12Collect(g_tracyCtx12);

	return true;
}

bool Decoder::PollGpuTimeMs(double *outMs) {
	if (!m_timestamps || !m_timestampFrequency || m_pendingTimings.empty())
		return false;

	const int slot = m_pendingTimings.front();
	if (m_decodeFence->GetCompletedValue() < m_submissions[slot].fenceValue)
		return false;
	m_pendingTimings.pop_front();

	UINT64 *ts = nullptr;
	D3D12_RANGE range = { sizeof(UINT64) * 2 * slot, sizeof(UINT64) * 2 * (slot + 1) };
	bool ok = false;
	if (SUCCEEDED(m_timestampReadback->Map(0, &range, reinterpret_cast<void **>(&ts)))) {
		const UINT64 begin = ts[2 * slot], end = ts[2 * slot + 1];
		if (end > begin) {
			*outMs = double(end - begin) * 1000.0 / double(m_timestampFrequency);
			ok = true;
		}
		D3D12_RANGE none = { 0, 0 };
		m_timestampReadback->Unmap(0, &none);
	}
	return ok;
}

} // namespace PyroWaveD3D12
} // namespace moonlight_xbox_dx
