#include "pch.h"
#include "FFMpegDecoder.h"
#include "../Plot/ImGuiPlots.h"
#include "StatsRenderer.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

#include <Common\DirectXHelper.h>
#include <d3d11_1.h>
#include "Utils.hpp"
#include "moonlight_xbox_dxMain.h"
#include <gamingdeviceinformation.h>

extern "C" {
#include "Limelight.h"
#include <third_party\h264bitstream\h264_stream.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/time.h>
}

using namespace moonlight_xbox_dx;

#define INITIAL_DECODER_BUFFER_SIZE (256 * 1024)
#define CAPTURE_LIMIT               (1024 * 1024 * 1024)
#define CAPTURE_AVIO_BUFFER_SIZE    (64 * 1024)

static bool ensure_buf_size(unsigned char **buf, int *buf_size, int required_size)
{
	if (*buf_size >= required_size)
		return true;

	FQLog("ensure_buf_size grew from %d -> %d\n", *buf_size, required_size);

	*buf_size = required_size;
	*buf = (unsigned char *)realloc(*buf, *buf_size);
	if (!*buf) {
		return false;
	}

	return true;
}

namespace moonlight_xbox_dx {
	FFMpegDecoder &FFMpegDecoder::instance() {
		static FFMpegDecoder inst;
		return inst;
	}

	FFMpegDecoder::FFMpegDecoder():
		width(0),
		height(0),
		videoFormat(0),
		decoder(nullptr),
		decoder_ctx(nullptr),
		device_ctx(nullptr),
		d3d11va_device_ctx(nullptr),
		ffmpeg_buffer(nullptr),
		ffmpeg_buffer_size(0),
		m_deviceResources(nullptr),
		m_LastFrameNumber(0),
		m_CaptureTail(Concurrency::task_from_result()) {
	}

	void lock_context(void *user) {
		auto me = (FFMpegDecoder*)user;
		me->m_mutex.lock();
	}

	void unlock_context(void *user) {
		auto me = (FFMpegDecoder*)user;
		me->m_mutex.unlock();
	}

	void ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
		char lineBuffer[1024];
		static int printPrefix = 1;

		if ((level & 0xFF) > av_log_get_level()) {
			return;
		}

		// We need to use the *previous* printPrefix value to determine whether to
		// print the prefix this time. av_log_format_line() will set the printPrefix
		// value to indicate whether the prefix should be printed *next time*.
		bool shouldPrefixThisMessage = printPrefix != 0;

		av_log_format_line(ptr, level, fmt, vl, lineBuffer, sizeof(lineBuffer), &printPrefix);
		Utils::Logf(shouldPrefixThisMessage ? "[ffmpeg] %s" : "%s", lineBuffer);
	}

	// ffmpeg calls this to let us pick the output pixel format. We use it as the
	// hook to allocate a D3D11VA frame pool with D3D11_BIND_SHADER_RESOURCE so the
	// renderer can sample decoder surfaces directly (skipping a per-frame copy).
	static enum AVPixelFormat ff_get_format(AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts) {
		for (const enum AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
			if (*p == AV_PIX_FMT_D3D11) {
				auto *me = reinterpret_cast<FFMpegDecoder *>(avctx->opaque);
				if (me->setupDirectSampleFramesContext(avctx)) {
					return AV_PIX_FMT_D3D11;
				}
				return AV_PIX_FMT_NONE;
			}
		}
		return AV_PIX_FMT_NONE;
	}

	// Allocate the hwaccel frame pool ourselves so we can add D3D11_BIND_SHADER_RESOURCE
	// to its textures, which the renderer requires to sample decoder surfaces directly.
	bool FFMpegDecoder::setupDirectSampleFramesContext(AVCodecContext *avctx) {
		AVBufferRef *frames_ref = nullptr;
		int err = avcodec_get_hw_frames_parameters(avctx, avctx->hw_device_ctx, AV_PIX_FMT_D3D11, &frames_ref);
		if (err < 0 || frames_ref == nullptr) {
			Utils::Logf("Direct sampling: avcodec_get_hw_frames_parameters failed (%d)\n", err);
			return false;
		}

		auto *frames_ctx = reinterpret_cast<AVHWFramesContext *>(frames_ref->data);
		auto *d3d11_frames = reinterpret_cast<AVD3D11VAFramesContext *>(frames_ctx->hwctx);

		// Default is D3D11_BIND_DECODER only. Add SHADER_RESOURCE so we can create SRVs
		// over the decoder surfaces. This keeps the pool as a single array texture
		// (decoding requires that), just with an extra bind flag.
		d3d11_frames->BindFlags |= D3D11_BIND_SHADER_RESOURCE;

		err = av_hwframe_ctx_init(frames_ref);
		if (err < 0) {
			// Most likely the driver won't allow BIND_DECODER | BIND_SHADER_RESOURCE
			// on the same texture.
			char e[256];
			av_strerror(err, e, sizeof(e));
			Utils::Logf("Direct sampling unavailable (av_hwframe_ctx_init: %s)\n", e);
			av_buffer_unref(&frames_ref);
			return false;
		}

		// Release any pool from a previous get_format call (e.g. a mid-stream format
		// change) before taking ownership of the new one, so we don't leak it.
		if (avctx->hw_frames_ctx) {
			av_buffer_unref(&avctx->hw_frames_ctx);
		}
		avctx->hw_frames_ctx = frames_ref; // transfer ownership to the codec
		return true;
	}

    void FFMpegDecoder::CompleteInitialization(const std::shared_ptr<DX::DeviceResources>& res, STREAM_CONFIGURATION *config, bool framePacingImmediate) {
		this->m_deviceResources = res;
		this->fps = config->fps;
		Pacer::instance().init(res, config->fps, res->GetRefreshRate(), framePacingImmediate);
	}

	int FFMpegDecoder::Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
		this->videoFormat = videoFormat;
		this->width = width;
		this->height = height;
		this->fps = 60; // correctly set in CompleteInitialization

		this->m_LastFrameNumber = 0;
		this->ffmpeg_buffer_size = 0;
		this->m_StreamEpochQpc = 0;


#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58,10,100)
		avcodec_register_all();
#endif

		// Increase log level until the first frame is decoded
		av_log_set_level(AV_LOG_INFO);

		av_log_set_callback(&ffmpeg_log_callback);
#pragma warning(suppress : 4996)

		if (videoFormat & VIDEO_FORMAT_MASK_H264) {
			decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
			Utils::Log("Using H264\n");
		}
		else if (videoFormat & VIDEO_FORMAT_MASK_H265) {
			decoder = avcodec_find_decoder(AV_CODEC_ID_HEVC);
			Utils::Log("Using HEVC\n");
		}

		if (decoder == NULL) {
			Utils::Log("Couldn't find decoder\n");
			return -1;
		}

		decoder_ctx = avcodec_alloc_context3(decoder);
		if (decoder_ctx == NULL) {
			Utils::Log("Couldn't allocate context\n");
			return -1;
		}
		decoder_ctx->opaque = this;
		decoder_ctx->extra_hw_frames = 5;

		AVBufferRef* hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
		device_ctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
		d3d11va_device_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(device_ctx->hwctx);
		d3d11va_device_ctx->device = m_deviceResources->GetD3DDevice();
		d3d11va_device_ctx->device_context = m_deviceResources->GetD3DDeviceContext();
		d3d11va_device_ctx->lock = lock_context;
		d3d11va_device_ctx->unlock = unlock_context;
		d3d11va_device_ctx->lock_ctx = this;
		int err2;
		if ((err2 = av_hwdevice_ctx_init(hw_device_ctx)) < 0) {
			Utils::Logf("Failed to create specified DirectX Video device: %d\n", err2);
			Cleanup();
			return err2;
		}

		decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
		av_buffer_unref(&hw_device_ctx);
		decoder_ctx->pix_fmt = AV_PIX_FMT_D3D11;
		// get_format lets us allocate a frame pool we can sample directly (no per-frame copy)
		decoder_ctx->get_format = ff_get_format;
		decoder_ctx->sw_pix_fmt = (videoFormat & VIDEO_FORMAT_MASK_10BIT) ? AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;
		decoder_ctx->pkt_timebase.num = 1;
		decoder_ctx->pkt_timebase.den = 90000;
		decoder_ctx->width = width;
		decoder_ctx->height = height;

		int err = avcodec_open2(decoder_ctx, decoder, NULL);
		if (err < 0) {
			char msg[2048];
			sprintf(msg, "Failed to create FFMpeg Codec: %d\n", err);
			Utils::Log(msg);
			return err;
		}

		if (decoder_ctx->pix_fmt != AV_PIX_FMT_D3D11) {
    		Utils::Log("Warning: decoder did not select AV_PIX_FMT_D3D11\n");
		}

		if (!ensure_buf_size(&ffmpeg_buffer, &ffmpeg_buffer_size, INITIAL_DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE)) {
			Utils::Log("Couldn't allocate initial ffmpeg_buffer\n");
			Cleanup();
			return -1;
		}

		return 0;
	}

	void FFMpegDecoder::Cleanup() {
		avcodec_free_context(&decoder_ctx);
		if (ffmpeg_buffer != NULL) {
			free(ffmpeg_buffer);
			ffmpeg_buffer = NULL;
			ffmpeg_buffer_size = 0;
		}
		m_LastFrameNumber = 0;

		Pacer::instance().deinit();
		DrainCaptureQueue();

		Utils::Log("FFMpegDecoder::Cleanup\n");
	}

    static inline int frame_attach_userdata(AVFrame *frame, int64_t decodeEndQpc) {
	    if (!frame) return AVERROR(EINVAL);

	    if (frame->opaque_ref) {
		    av_buffer_unref(&frame->opaque_ref);
	    }

	    AVBufferRef *buf = av_buffer_allocz(sizeof(MLFrameData));
	    if (!buf) return AVERROR(ENOMEM);

	    MLFrameData *data = (MLFrameData *)buf->data;
	    data->decodeEndQpc = decodeEndQpc;
	    frame->opaque_ref = buf;

	    return 0;
    }

    // Called by the VideoDec thread
	int FFMpegDecoder::SubmitDecodeUnit(PDECODE_UNIT decodeUnit) {
		LARGE_INTEGER decodeStart, decodeEnd;
		PLENTRY entry = decodeUnit->bufferList;
		int length = 0;
		QueryPerformanceCounter(&decodeStart);

		if (m_StreamEpochQpc == 0) m_StreamEpochQpc = decodeStart.QuadPart;

		if (!ensure_buf_size(&ffmpeg_buffer, &ffmpeg_buffer_size, decodeUnit->fullLength + AV_INPUT_BUFFER_PADDING_SIZE)) {
			Utils::Logf("Couldn't realloc ffmpeg_buffer\n");
			return DR_NEED_IDR;
		}

	    while (entry != NULL) {
		    memcpy(ffmpeg_buffer + length, entry->data, entry->length);
		    length += entry->length;
		    entry = entry->next;
	    }
		memset(ffmpeg_buffer + length, 0, AV_INPUT_BUFFER_PADDING_SIZE);

		// Detect breaks in the frame sequence indicating dropped packets
		uint32_t droppedFramesNetwork = 0;
		if (m_LastFrameNumber > 0 && decodeUnit->frameNumber > (m_LastFrameNumber + 1)) {
			// Any frame number greater than m_LastFrameNumber + 1 represents a dropped frame
			droppedFramesNetwork = decodeUnit->frameNumber - (m_LastFrameNumber + 1);
		}
		m_LastFrameNumber = decodeUnit->frameNumber;

		if (!decodeUnit->rtpTimestamp) {
			// Estimate for hosts that don't send timestamps (e.g. Wolf)
			LogOnce("Warning: host is not sending RTP timestamps, this may hurt frame pacing\n");
			double ptsMs = QpcToMs(QpcNow() - m_StreamEpochQpc);
			decodeUnit->rtpTimestamp = (uint32_t)(ptsMs * 90.0);
			decodeUnit->presentationTimeUs = (uint64_t)(ptsMs * 1000.0);
		}

		// track stats for a variety of things we can track at the same time
		Stats::instance().SubmitVideoBytesAndReassemblyTime(length, decodeUnit, droppedFramesNetwork);

		// Debug hook for saving out the bitstream to disk for later analysis.
		// Dev Mode gets an additional quick menu option to toggle capture when
		// Utils::ShowDevTools() is false.
		if (m_CaptureState.load(std::memory_order_relaxed) != CaptureState::Idle) {
			QueueCaptureFrame(ffmpeg_buffer, length, decodeUnit->rtpTimestamp,
			                  decodeUnit->frameType == FRAME_TYPE_IDR);
		}

		// ffmpeg_decode
		AVPacket *pkt = av_packet_alloc();
		pkt->data = ffmpeg_buffer;
		pkt->size = length;
		pkt->pts = (int64_t)decodeUnit->rtpTimestamp;
		pkt->dts = pkt->pts;

		int err = avcodec_send_packet(decoder_ctx, pkt);
		av_packet_unref(pkt);
		av_packet_free(&pkt);
		if (err < 0) {
			char ffmpegError[1024];
			av_strerror(err, ffmpegError, 1024);
			Utils::Logf("avcodec_send_packet failed: %s\n", ffmpegError);
			return DR_NEED_IDR;
		}

		while (err >= 0) {
			AVFrame* frame = av_frame_alloc();
			err = avcodec_receive_frame(decoder_ctx, frame);
			if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
				av_frame_free(&frame);
				break;
			}
			else if (err < 0) {
				char ffmpegError[1024];
				av_strerror(err, ffmpegError, sizeof(ffmpegError));
				Utils::Logf("avcodec_receive_frame failed: %s\n", ffmpegError);
				av_frame_free(&frame);
				return DR_NEED_IDR;
			}

			// Capture a frame timestamp to measuring pacing delay
			QueryPerformanceCounter(&decodeEnd);
			frame_attach_userdata(frame, decodeEnd.QuadPart);

			FQLog("✓ Frame decoded [pts: %.3fms] [in#: %d] [out#: %d] [lost: %d] decode time %.3fms\n",
				frame->pts / 90.0,
				decodeUnit->frameNumber, decoder_ctx->frame_num,
				decodeUnit->frameNumber - decoder_ctx->frame_num,
				QpcToMs(decodeEnd.QuadPart - decodeStart.QuadPart));

			// Queue the frame for rendering. frame is now owned by Pacer.
			Pacer::instance().submitFrame(frame);

			// Even though we have a valid frame, the ffmpeg API needs us to loop and call avcodec_receive_frame()
			// again where we expect to get AVERROR(EAGAIN) and break out.
		}

		double decodeTimeMs = QpcToMs(decodeEnd.QuadPart - decodeStart.QuadPart);
		if (decodeEnd.QuadPart > decodeStart.QuadPart) {
			Stats::instance().SubmitDecodeMs(decodeTimeMs);
		}

		// Not the best way to handle this. BUT IT DOES FIX XBOX ONE TEARING!!!!
		// Honestly this did take too much time of my life (and AndyG life too) to care to make a better version
		// If you want to fix this, have fun! (And hopefully you have Microsoft blessing/tools/support for that)
		// if (IsXboxOne()) {
		// 	float remainingMs = (1000.0f / fps) - decodeTimeMs - 2.0; // 2ms buffer time
		// 	if (remainingMs > 0.0) {
		// 		//Utils::Logf("SubmitDecodeUnit sleeping %.3fms\n", remainingMs);
		// 		SleepUntilQpc(QpcNow() + MsToQpc(remainingMs));
		// 	}
		// }

		// Restore default log level after a successful decode
		if (av_log_get_level() > AV_LOG_WARNING) {
			av_log_set_level(AV_LOG_WARNING);
		}

		return DR_OK;
	}

	//Helpers
	int initCallback(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) noexcept {
		return FFMpegDecoder::instance().Init(videoFormat, width, height, redrawRate, context, drFlags);
	}

	void cleanupCallback()noexcept {
		FFMpegDecoder::instance().Cleanup();
	}

	int submitDecodeUnit(PDECODE_UNIT decodeUnit) noexcept {
		return FFMpegDecoder::instance().SubmitDecodeUnit(decodeUnit);
	}

	DECODER_RENDERER_CALLBACKS FFMpegDecoder::getDecoder() {
		DECODER_RENDERER_CALLBACKS decoder_callbacks_sdl;
		LiInitializeVideoCallbacks(&decoder_callbacks_sdl);
		decoder_callbacks_sdl.setup = initCallback;
		decoder_callbacks_sdl.cleanup = cleanupCallback;
		decoder_callbacks_sdl.submitDecodeUnit = submitDecodeUnit;
		decoder_callbacks_sdl.capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_INTRA_REFRESH;
		//decoder_callbacks_sdl.capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC;
		return decoder_callbacks_sdl;
	}
}

// ---------------------------------------------------------------------------
// Frame capture
//
// The decode thread copies each access unit into an AVPacket and hands it to a
// serialized task chain. That chain owns the mpegts muxer and does all of the
// file I/O, so nothing blocking ever lands on the decode thread.
// ---------------------------------------------------------------------------

// Called from the capture task thread when the capture can't continue (open
// failed, a write failed, or we hit the size limit). Stops the decode thread
// from queueing any more frames.
void FFMpegDecoder::StopRecordingFromWriter() {
	std::lock_guard<std::mutex> lock(m_CaptureQueueMutex);
	CaptureState expected = CaptureState::Recording;
	m_CaptureState.compare_exchange_strong(expected, CaptureState::Idle);
}

// Caller must hold m_CaptureQueueMutex.
void FFMpegDecoder::EnqueueCaptureTask(std::function<void()> work) {
	m_CaptureTail = m_CaptureTail.then(
		[work = std::move(work)](Concurrency::task<void> previous) {
			// Observe any unexpected failure from the preceding task
			try {
				previous.get();
			}
			catch (const std::exception& e) {
				Utils::Logf("Previous capture task failed: %s\n", e.what());
			}

			try {
				work();
			}
			catch (const std::exception& e) {
				Utils::Logf("Capture task failed: %s\n", e.what());
			}
		}
	);
}

// Called on the decode thread for every frame while capture is armed or recording.
void FFMpegDecoder::QueueCaptureFrame(const uint8_t* data, size_t length, uint32_t rtpTimestamp, bool isKeyFrame) {
	std::lock_guard<std::mutex> lock(m_CaptureQueueMutex);

	CaptureState state = m_CaptureState.load();
	if (m_CaptureQueueStopping || state == CaptureState::Idle) {
		return;
	}

	bool opening = false;
	if (state == CaptureState::WaitingForIdr) {
		// Drop everything until the IDR we asked for lands, so the file starts
		// with parameter sets and a fully refreshed picture.
		if (!isKeyFrame) {
			return;
		}

		m_CaptureState.store(CaptureState::Recording);
		opening = true;
	}

	// av_new_packet gives us a refcounted, padded buffer, so the muxer can point
	// straight at it later without another copy.
	AVPacket* raw = av_packet_alloc();
	if (raw == nullptr) {
		return;
	}
	if (av_new_packet(raw, (int)length) < 0) {
		av_packet_free(&raw);
		Utils::Logf("Capture: could not allocate a %zu byte packet\n", length);
		return;
	}
	memcpy(raw->data, data, length);

	CapturePacket packet(raw, [](AVPacket* p) { av_packet_free(&p); });

	if (opening) {
		EnqueueCaptureTask([this] {
			if (!CaptureOpen()) {
				StopRecordingFromWriter();
			}
		});
	}

	EnqueueCaptureTask([this, packet = std::move(packet), rtpTimestamp, isKeyFrame] {
		CaptureWriteFrame(packet, rtpTimestamp, isKeyFrame);
	});
}

void FFMpegDecoder::DrainCaptureQueue() {
	Concurrency::task<void> tail;
	{
		std::lock_guard<std::mutex> lock(m_CaptureQueueMutex);
		m_CaptureQueueStopping = true;
		if (m_CaptureState.exchange(CaptureState::Idle) == CaptureState::Recording) {
			EnqueueCaptureTask([this] { CaptureClose(); });
		}
		tail = m_CaptureTail;
	}

	try {
		tail.get();
	}
	catch (const std::exception& e) {
		Utils::Logf("Capture queue shutdown failed: %s\n", e.what());
	}

	// The decoder is a singleton reused across streams, so reset the chain and
	// let the next session capture again.
	std::lock_guard<std::mutex> lock(m_CaptureQueueMutex);
	m_CaptureQueueStopping = false;
	m_CaptureTail = Concurrency::task_from_result();
}

// ffmpeg writer
int FFMpegDecoder::CaptureAvioWrite(void* opaque, uint8_t* buf, int size) {
	auto* me = reinterpret_cast<FFMpegDecoder*>(opaque);
	if (size <= 0) {
		return 0;
	}

	me->m_CaptureFile.write(reinterpret_cast<const char*>(buf), size);
	if (!me->m_CaptureFile) {
		Utils::Logf("Capture: failed writing %d bytes to %s\n", size, me->m_CapturePath.c_str());
		return AVERROR(EIO);
	}

	me->m_CaptureBytesWritten += size;
	return size;
}

// Runs on the capture task thread. One file per capture session.
bool FFMpegDecoder::CaptureOpen() {
	namespace fs = std::filesystem;

	// Shouldn't happen, but never leak a half-open session.
	CaptureAbort();

	Platform::String^ developmentPath = L"D:\\DevelopmentFiles";
	Platform::String^ backupPath = Windows::Storage::ApplicationData::Current->LocalFolder->Path;
	if (backupPath == nullptr || backupPath->Length() == 0) {
		Utils::Log("Capture: could not resolve the LocalState path\n");
		return false;
	}

	const std::time_t now = std::time(nullptr);
	std::tm localTime{};

	if (localtime_s(&localTime, &now) != 0) {
		Utils::Log("Capture: could not determine the local time\n");
		return false;
	}

	std::ostringstream filename;
	filename
		<< std::put_time(&localTime, "%Y-%m-%d-%H%M%S")
		<< "-Moonlight_Xbox_"
		<< width << 'x' << height
		<< ".ts";

	if (m_CaptureFile.is_open()) {
		m_CaptureFile.close();
	}

	auto tryOpenCapture = [&](Platform::String^ basePath) -> bool {
		if (basePath == nullptr || basePath->Length() == 0) {
			return false;
		}

		const fs::path candidate = fs::path(basePath->Data()) / filename.str();

		// A failed open sets the stream's fail bit, so clear it before retrying.
		m_CaptureFile.clear();
		m_CaptureFile.open(candidate, std::ios::binary | std::ios::trunc);

		if (!m_CaptureFile.is_open()) {
			Utils::Logf("Capture: path is unavailable or not writable: %s\n", candidate.string().c_str());
			m_CaptureFile.clear();
			return false;
		}

		m_CapturePath = candidate.string();
		return true;
	};

	// Prefer DevelopmentFiles, then fall back to LocalState.
	if (!tryOpenCapture(developmentPath) &&
		!tryOpenCapture(backupPath)) {
		Utils::Log("Capture: no writable capture location is available\n");
		CaptureAbort();
		return false;
	}

	char ffmpegError[256];
	int err = avformat_alloc_output_context2(&m_CaptureFormatCtx, nullptr, "mpegts", nullptr);
	if (err < 0 || m_CaptureFormatCtx == nullptr) {
		av_strerror(err, ffmpegError, sizeof(ffmpegError));
		Utils::Logf("Capture: no mpegts muxer available: %s\n", ffmpegError);
		CaptureAbort();
		return false;
	}

	auto* avioBuffer = (unsigned char*)av_malloc(CAPTURE_AVIO_BUFFER_SIZE);
	if (avioBuffer == nullptr) {
		Utils::Log("Capture: could not allocate the AVIO buffer\n");
		CaptureAbort();
		return false;
	}

	m_CaptureAvioCtx = avio_alloc_context(avioBuffer, CAPTURE_AVIO_BUFFER_SIZE, 1, this,
	                                      nullptr, CaptureAvioWrite, nullptr);
	if (m_CaptureAvioCtx == nullptr) {
		av_free(avioBuffer);
		Utils::Log("Capture: could not allocate the AVIO context\n");
		CaptureAbort();
		return false;
	}
	m_CaptureFormatCtx->pb = m_CaptureAvioCtx;
	m_CaptureFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

	m_CaptureStream = avformat_new_stream(m_CaptureFormatCtx, nullptr);
	if (m_CaptureStream == nullptr) {
		Utils::Log("Capture: could not allocate the output stream\n");
		CaptureAbort();
		return false;
	}

	AVCodecParameters* par = m_CaptureStream->codecpar;
	par->codec_type = AVMEDIA_TYPE_VIDEO;
	par->codec_id = (videoFormat & VIDEO_FORMAT_MASK_H265) ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
	par->codec_tag = 0;
	par->width = width;
	par->height = height;
	par->format = (videoFormat & VIDEO_FORMAT_MASK_10BIT) ? AV_PIX_FMT_YUV420P10LE : AV_PIX_FMT_YUV420P;

	m_CaptureStream->time_base = AVRational{ 1, 90000 };
	m_CaptureStream->avg_frame_rate = AVRational{ fps, 1 };

	m_CaptureBytesWritten = 0;
	m_CapturePts = 0;
	m_CaptureLastRtp = 0;
	m_CaptureHavePts = false;

	err = avformat_write_header(m_CaptureFormatCtx, nullptr);
	if (err < 0) {
		av_strerror(err, ffmpegError, sizeof(ffmpegError));
		Utils::Logf("Capture: avformat_write_header failed: %s\n", ffmpegError);
		CaptureAbort();
		return false;
	}
	m_CaptureHeaderWritten = true;

	Utils::Logf("Capture started, writing to %s\n", m_CapturePath.c_str());
	return true;
}

// Runs on the capture task thread.
void FFMpegDecoder::CaptureWriteFrame(const CapturePacket& packet, uint32_t rtpTimestamp, bool isKeyFrame) {
	// Frames queued before the limit was hit (or before an open failure) land here
	// with nothing to write to.
	if (m_CaptureFormatCtx == nullptr || !packet || packet->size <= 0) {
		return;
	}

	// Only used to paper over a bad timestamp below; real frame timing comes from
	// the host's own deltas.
	const int64_t nominalFrameTicks = (fps > 0) ? (90000 / fps) : 1500;

	// Capture pts starts at 0
	if (!m_CaptureHavePts) {
		m_CapturePts = 0;
		m_CaptureHavePts = true;
	}
	else {
		int64_t delta = (uint32_t)(rtpTimestamp - m_CaptureLastRtp);
		if (delta <= 0 || delta > 10 * 90000) {
			delta = nominalFrameTicks;
		}

		m_CapturePts += delta;
	}
	m_CaptureLastRtp = rtpTimestamp;

	AVPacket* pkt = packet.get();
	pkt->stream_index = m_CaptureStream->index;
	pkt->pts = m_CapturePts;
	pkt->dts = m_CapturePts;
	pkt->duration = 0; // .ts doesn't support duration
	pkt->flags = isKeyFrame ? AV_PKT_FLAG_KEY : 0;

	// av_write_frame borrows the packet rather than taking ownership of it, so
	// the CapturePacket deleter still frees it once this task is done.
	int err = av_write_frame(m_CaptureFormatCtx, pkt);
	if (err < 0) {
		char ffmpegError[256];
		av_strerror(err, ffmpegError, sizeof(ffmpegError));
		Utils::Logf("Capture: av_write_frame failed: %s\n", ffmpegError);
		CaptureClose();
		StopRecordingFromWriter();
		return;
	}

	if (m_CaptureBytesWritten >= CAPTURE_LIMIT) {
		Utils::Logf("Capture: %lld MB limit reached\n", (long long)(CAPTURE_LIMIT / (1024 * 1024)));
		CaptureClose();
		StopRecordingFromWriter();
	}
}

// Runs on the capture task thread. Finishes the file and reports where it went.
void FFMpegDecoder::CaptureClose() {
	if (m_CaptureFormatCtx != nullptr && m_CaptureHeaderWritten) {
		int err = av_write_trailer(m_CaptureFormatCtx);
		if (err < 0) {
			char ffmpegError[256];
			av_strerror(err, ffmpegError, sizeof(ffmpegError));
			Utils::Logf("Capture: av_write_trailer failed: %s\n", ffmpegError);
		}
	}

	// Push whatever is still sitting in the AVIO buffer out to the file while it
	// is still open.
	if (m_CaptureAvioCtx != nullptr) {
		avio_flush(m_CaptureAvioCtx);
	}

	const int64_t bytes = m_CaptureBytesWritten;
	const std::string path = m_CapturePath;

	CaptureAbort();

	if (bytes > 0) {
		Utils::Logf("Capture saved %.1f MB to %s\n", bytes / (1024.0 * 1024.0), path.c_str());
	}
}

// Tear down the muxer and file without writing a trailer. Safe to call at any
// point, including from a partially failed CaptureOpen().
void FFMpegDecoder::CaptureAbort() {
	if (m_CaptureFormatCtx != nullptr) {
		// AVFMT_FLAG_CUSTOM_IO means avformat won't touch our pb, so free it below.
		avformat_free_context(m_CaptureFormatCtx);
		m_CaptureFormatCtx = nullptr;
		m_CaptureStream = nullptr;
	}

	if (m_CaptureAvioCtx != nullptr) {
		av_freep(&m_CaptureAvioCtx->buffer);
		avio_context_free(&m_CaptureAvioCtx);
	}

	if (m_CaptureFile.is_open()) {
		m_CaptureFile.close();
		if (!m_CaptureFile) {
			Utils::Logf("Capture: failed closing %s\n", m_CapturePath.c_str());
		}
	}
	m_CaptureFile.clear();

	m_CapturePath.clear();
	m_CaptureBytesWritten = 0;
	m_CapturePts = 0;
	m_CaptureLastRtp = 0;
	m_CaptureHavePts = false;
	m_CaptureHeaderWritten = false;
}

bool FFMpegDecoder::IsCaptureActive() const {
	return m_CaptureState.load() != CaptureState::Idle;
}

void FFMpegDecoder::ToggleCapture() {
	SetCapture(!IsCaptureActive());
}

void FFMpegDecoder::SetCapture(bool wanted) {
	std::lock_guard<std::mutex> lock(m_CaptureQueueMutex);

	if (wanted) {
		if (m_CaptureQueueStopping || m_CaptureState.load() != CaptureState::Idle) {
			return;
		}

		// A capture is only useful if it opens with a decodable picture, so ask
		// the host for an IDR and let the decode thread start on its arrival.
		m_CaptureState.store(CaptureState::WaitingForIdr);
		LiRequestIdrFrame();
		Utils::Log("Capture armed, waiting for an IDR frame\n");
		return;
	}

	CaptureState previous = m_CaptureState.exchange(CaptureState::Idle);
	if (previous == CaptureState::Recording) {
		EnqueueCaptureTask([this] { CaptureClose(); });
	}
	else if (previous == CaptureState::WaitingForIdr) {
		Utils::Log("Capture cancelled before an IDR frame arrived\n");
	}
}
