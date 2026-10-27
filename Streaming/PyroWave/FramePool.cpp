#include "pch.h"
#include "FramePool.h"
#include <Utils.hpp>

extern "C" {
#include <libavutil/buffer.h>
#include <libavutil/pixfmt.h>
}

namespace moonlight_xbox_dx {

bool PyroWaveFramePool::Init(ID3D11Device *device, int width, int height, bool chroma444, int count, bool shared) {
	m_width = width;
	m_height = height;
	m_chroma444 = chroma444;
	m_sets.resize(count);
	m_freeList.clear();

	D3D11_TEXTURE2D_DESC desc = {};
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R16_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	if (shared)
		desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = desc.Format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Texture2DArray.MipSlice = 0;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.ArraySize = 1;

	for (int i = 0; i < count; i++) {
		PyroWaveFrameSet &set = m_sets[i];
		set.pool = this;
		set.index = i;
		for (int c = 0; c < 3; c++) {
			bool chromaPlane = (c != 0) && !chroma444;
			desc.Width = chromaPlane ? width / 2 : width;
			desc.Height = chromaPlane ? height / 2 : height;
			if (FAILED(device->CreateTexture2D(&desc, nullptr, &set.tex[c])) ||
			    FAILED(device->CreateShaderResourceView(set.tex[c].Get(), nullptr, &set.srv[c])) ||
			    FAILED(device->CreateUnorderedAccessView(set.tex[c].Get(), &uavDesc, &set.uav[c]))) {
				Utils::Logf("PyroWave: frame pool plane creation failed (set %d plane %d, %dx%d)\n",
				            i, c, width, height);
				return false;
			}
		}
		m_freeList.push_back(i);
	}
	return true;
}

PyroWaveFrameSet *PyroWaveFramePool::Acquire() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_freeList.empty())
		return nullptr;
	int index = m_freeList.back();
	m_freeList.pop_back();
	return &m_sets[index];
}

void PyroWaveFramePool::Recycle(int index) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_freeList.push_back(index);
}

void PyroWaveFramePool::FreeCallback(void *opaque, uint8_t *data) {
	(void)data;
	PyroWaveFrameSet *set = static_cast<PyroWaveFrameSet *>(opaque);
	set->pool->Recycle(set->index);
}

AVFrame *PyroWaveFramePool::WrapFrame(PyroWaveFrameSet *set) {
	AVFrame *frame = av_frame_alloc();
	if (!frame) {
		Recycle(set->index);
		return nullptr;
	}
	// The buffer's data pointer is just the set itself; size is nominal.
	// The free callback is the piece that matters: every av_frame_free
	// anywhere in the pipeline returns the planes to the pool.
	frame->buf[0] = av_buffer_create(reinterpret_cast<uint8_t *>(set), sizeof(PyroWaveFrameSet),
	                                 &PyroWaveFramePool::FreeCallback, set, AV_BUFFER_FLAG_READONLY);
	if (!frame->buf[0]) {
		av_frame_free(&frame);
		Recycle(set->index);
		return nullptr;
	}
	// PyroWave sentinel; also drives CSC bit depth and chroma cositing
	frame->format = m_chroma444 ? AV_PIX_FMT_YUV444P16 : AV_PIX_FMT_YUV420P16;
	frame->width = m_width;
	frame->height = m_height;
	for (int c = 0; c < 3; c++)
		frame->data[c] = reinterpret_cast<uint8_t *>(set->tex[c].Get());
	frame->data[3] = reinterpret_cast<uint8_t *>(set);
	// 4:4:4 has no siting; 4:2:0 is center-sited by the PyroWave encoder
	frame->chroma_location = AVCHROMA_LOC_CENTER;
	return frame;
}

} // namespace moonlight_xbox_dx
