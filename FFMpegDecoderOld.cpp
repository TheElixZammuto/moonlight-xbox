#include "pch.h"
#include "FFMpegDecoder.h"
#include <queue>
#include <mutex>
#define DECODER_BUFFER_SIZE 1024 * 1024
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
#include <libgamestream/sps.h>
}
using namespace moonlight_xbox_dx;

static AVPacket pkt;
static AVCodec* decoder;
static AVCodecContext* decoder_ctx;
static AVFrame** dec_frames;
static unsigned char* ffmpeg_buffer;

static int dec_frames_cnt;
static int current_frame, next_frame;

enum decoders ffmpeg_decoder;

#define BYTES_PER_PIXEL 4
static enum AVPixelFormat hw_pix_fmt;
static std::queue<AVFrame*> renderedFrame;
static AVFrame* lastFrame;
static bool setup = false;

AVFrame* GetLastFrame() {
    return lastFrame;
}

bool HasSetup() {
    return setup;
}

void ResetLastFrame() {
    lastFrame = NULL;
}

namespace moonlight_xbox_dx {
    AVFrame* ffmpeg_get_frame();
    static void Cleanup();
    
    static int Init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
        // Initialize the avcodec library and register codecs
        av_log_set_level(AV_LOG_QUIET);
        gs_sps_init(width, height);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58,10,100)
        avcodec_register_all();
#endif

#pragma warning(suppress : 4996)
        av_init_packet(&pkt);

        switch (videoFormat) {
        case VIDEO_FORMAT_H264:
            decoder = avcodec_find_decoder_by_name("h264");
            break;
        case VIDEO_FORMAT_H265:
            decoder = avcodec_find_decoder_by_name("hevc");
            break;
        }

        if (decoder == NULL) {
            printf("Couldn't find decoder\n");
            return -1;
        }




        decoder_ctx = avcodec_alloc_context3(decoder);
        if (decoder_ctx == NULL) {
            printf("Couldn't allocate context");
            return -1;
        }

        static AVBufferRef* hw_device_ctx = NULL;
        int err2;
        AVDictionary* dictionary = NULL;
        av_dict_set_int(&dictionary, "debug", 1, 0);
        if ((err2 = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_D3D11VA, NULL, dictionary, 0)) < 0) {
            fprintf(stderr, "Failed to create specified HW device.\n");
            return err2;

        }

        decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);



        //decoder_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
        //decoder_ctx->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
        //decoder_ctx->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;
        //decoder_ctx->err_recognition = AV_EF_EXPLODE;

        decoder_ctx->width = width;
        decoder_ctx->height = height;
        decoder_ctx->pix_fmt = AV_PIX_FMT_D3D11;

        int err = avcodec_open2(decoder_ctx, decoder, NULL);
        if (err < 0) {
            printf("Couldn't open codec");
            return err;
        }
        ffmpeg_buffer = (unsigned char*)malloc(DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
        if (ffmpeg_buffer == NULL) {
            fprintf(stderr, "Not enough memory\n");
            Cleanup();
            return -1;
        }
        int buffer_count = 2;
        dec_frames_cnt = buffer_count;
        dec_frames = (AVFrame**)malloc(buffer_count * sizeof(AVFrame*));
        if (dec_frames == NULL) {
            fprintf(stderr, "Couldn't allocate frames");
            return -1;
        }

        for (int i = 0; i < buffer_count; i++) {
            dec_frames[i] = av_frame_alloc();
            if (dec_frames[i] == NULL) {
                fprintf(stderr, "Couldn't allocate frame");
                return -1;
            }
        }
        setup = true;
        return 0;
    }

    static void Start() {

    }

    static void Stop() {

    }

    static void Cleanup() {
        if (decoder_ctx) {
            avcodec_close(decoder_ctx);
            av_free(decoder_ctx);
            decoder_ctx = NULL;
        }
        if (dec_frames) {
            for (int i = 0; i < dec_frames_cnt; i++) {
                if (dec_frames[i])
                    av_frame_free(&dec_frames[i]);
            }
        }
    }


    int SubmitDU(PDECODE_UNIT decodeUnit) {
        if (decodeUnit->fullLength > DECODER_BUFFER_SIZE) {
            printf("C");
            return -1;
        }
        //frameLock.lock();
        PLENTRY entry = decodeUnit->bufferList;
        uint32_t length = 0;
        while (entry != NULL) {
            /*if (entry->bufferType == BUFFER_TYPE_SPS)
                gs_sps_fix(entry, GS_SPS_BITSTREAM_FIXUP, ffmpeg_buffer, &length);
            else {*/
                memcpy(ffmpeg_buffer + length, entry->data, entry->length);
                length += entry->length;
                entry = entry->next;
            /*}*/
        }
        int err;
        pkt.data = ffmpeg_buffer;
        pkt.size = length;

        err = avcodec_send_packet(decoder_ctx, &pkt);
        if (err < 0) {
            char errorstringnew[1024];
            sprintf(errorstringnew, "Error from FFMPEG: %d", AVERROR(err));
            return DR_NEED_IDR;
        }
        ffmpeg_get_frame();
        return DR_OK;
    }

    AVFrame* ffmpeg_get_frame() {
        AVFrame* frame;
        AVFrame* softwareFrame;
        int retries = 0;
        while (1) {
            frame = av_frame_alloc();
            softwareFrame = av_frame_alloc();
            int err = avcodec_receive_frame(decoder_ctx, frame);
            if (err == 0 && frame->format == AV_PIX_FMT_D3D11) {
                lastFrame = frame;
                int a = av_hwframe_transfer_data(softwareFrame, frame, 0);
                if (a < 0) {
                    return 0;
                }
                if (softwareFrame->format != AV_PIX_FMT_NV12) {
                    return NULL;
                }
                av_frame_free(&softwareFrame);
                return frame;
            }
            else if (err != AVERROR(EAGAIN)) {
                av_frame_free(&frame);
                char errorstring[512];
                av_strerror(err, errorstring, sizeof(errorstring));
                char errorstringnew[1024];
                sprintf(errorstringnew, "Error from FFMPEG: %s", errorstring);
                return NULL;
            }
            else {
                retries++;
                av_frame_free(&frame);
                if (retries > 10)return NULL;
            }
        }
        
        return NULL;
    }

    DECODER_RENDERER_CALLBACKS FFMpegDecoder::GetCallbacks() {
        DECODER_RENDERER_CALLBACKS decoder_callbacks_sdl;
        decoder_callbacks_sdl.setup = Init;
        decoder_callbacks_sdl.start = Start;
        decoder_callbacks_sdl.stop = Stop;
        decoder_callbacks_sdl.cleanup = Cleanup;
        decoder_callbacks_sdl.submitDecodeUnit = SubmitDU;
        decoder_callbacks_sdl.capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC | CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC;
        return decoder_callbacks_sdl;
    }
}