#pragma once

#include <atomic>
#include <functional>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include "../Common/StepTimer.h"
#include "Pacer.h"
#include "Utils.hpp"
#include "VideoRenderer.h"

extern "C" {
#include <Limelight.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libswscale/swscale.h>
}

#define MAX_BUFFER 1024 * 1024

typedef struct MLFrameData {
	int64_t decodeEndQpc;     // when we finished decoding
	int64_t presentTargetQpc; // timestamp when frame should be presented (slightly earlier than vsync)
	int64_t presentVsyncQpc;  // hard vsync deadline
} MLFrameData;

// A copy of one access unit, kept alive until the capture writer thread muxes it.
using CapturePacket = std::shared_ptr<AVPacket>;

namespace moonlight_xbox_dx {

// Capture only ever starts on an IDR, so arming it and recording it are
// separate states: SetCapture(true) asks the host for an IDR and the decode
// thread promotes us to Recording when that frame shows up.
enum class CaptureState { Idle, WaitingForIdr, Recording };

class FFMpegDecoder {
  public:
	// Singleton accessor
	static FFMpegDecoder &instance();

	void CompleteInitialization(const std::shared_ptr<DX::DeviceResources> &res, STREAM_CONFIGURATION *config, bool framePacingImmediate);
	int Init(int videoFormat, int width, int height, int redrawRate, void *context, int drFlags);
	void Cleanup();
	int SubmitDecodeUnit(PDECODE_UNIT decodeUnit);
	static FFMpegDecoder *getInstance();
	static DECODER_RENDERER_CALLBACKS getDecoder();
	void ToggleCapture();
	void SetCapture(bool wanted);
	bool IsCaptureActive() const;

	// Called from the get_format callback to set up a frame pool with
	// D3D11_BIND_SHADER_RESOURCE so the renderer can sample decoder surfaces
	// directly. Returns false on failure, which aborts decoding.
	bool setupDirectSampleFramesContext(AVCodecContext *avctx);

	int videoFormat, width, height, fps;
	std::recursive_mutex m_mutex;

	// locking helper
	class LockGuard {
	  public:
		explicit LockGuard(FFMpegDecoder &ff)
		    : m_ff(ff) {
			m_ff.m_mutex.lock();
		}
		~LockGuard() {
			m_ff.m_mutex.unlock();
		}
		LockGuard(const LockGuard &) = delete;
		LockGuard &operator=(const LockGuard &) = delete;

	  private:
		FFMpegDecoder &m_ff;
	};

	[[nodiscard]] static LockGuard Lock() {
		return LockGuard(instance());
	}

  private:
	FFMpegDecoder();
	FFMpegDecoder(const FFMpegDecoder &) = delete;
	FFMpegDecoder &operator=(const FFMpegDecoder &) = delete;
	// Producer side, called on the decode thread.
	void QueueCaptureFrame(const uint8_t *data, size_t length, uint32_t rtpTimestamp, bool isKeyFrame);
	void EnqueueCaptureTask(std::function<void()> work); // caller holds m_CaptureQueueMutex
	void DrainCaptureQueue();
	void StopRecordingFromWriter();

	// Consumer side, only ever touched by the serialized capture task chain.
	bool CaptureOpen();
	void CaptureWriteFrame(const CapturePacket &packet, uint32_t rtpTimestamp, bool isKeyFrame);
	void CaptureClose();
	void CaptureAbort();
	static int CaptureAvioWrite(void *opaque, uint8_t *buf, int size);

	const AVCodec *decoder;
	AVCodecContext *decoder_ctx;
	AVHWDeviceContext *device_ctx;
	AVD3D11VADeviceContext *d3d11va_device_ctx;
	unsigned char *ffmpeg_buffer;
	int ffmpeg_buffer_size;
	std::shared_ptr<DX::DeviceResources> m_deviceResources;
	int m_LastFrameNumber;
	int64_t m_StreamEpochQpc;

	// m_CaptureState is atomic so the decode thread can skip the capture path with
	// a single relaxed load, but every *change* to it happens under
	// m_CaptureQueueMutex so the state transition and the task it enqueues stay
	// ordered against each other.
	std::atomic<CaptureState> m_CaptureState{CaptureState::Idle};
	std::mutex m_CaptureQueueMutex;
	Concurrency::task<void> m_CaptureTail;
	bool m_CaptureQueueStopping = false;

	// Writer-thread state
	AVFormatContext *m_CaptureFormatCtx = nullptr;
	AVIOContext *m_CaptureAvioCtx = nullptr;
	AVStream *m_CaptureStream = nullptr;
	std::ofstream m_CaptureFile;
	std::string m_CapturePath;
	int64_t m_CaptureBytesWritten = 0;
	int64_t m_CapturePts = 0;
	uint32_t m_CaptureLastRtp = 0;
	bool m_CaptureHavePts = false;
	bool m_CaptureHeaderWritten = false;
};
} // namespace moonlight_xbox_dx
