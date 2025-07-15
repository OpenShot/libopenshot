/**
 * @file
 * @brief Header file for FFmpegUtilities
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2024 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_FFMPEG_UTILITIES_H
#define OPENSHOT_FFMPEG_UTILITIES_H

#include "OpenShotVersion.h"  // For FFMPEG_USE_SWRESAMPLE

// Required for libavformat to build on Windows
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#ifndef IS_FFMPEG_3_2
#define IS_FFMPEG_3_2 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 64, 101))
#endif

#ifndef USE_HW_ACCEL
#define USE_HW_ACCEL (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 107, 100))
#endif

#ifndef USE_SW
#define USE_SW FFMPEG_USE_SWRESAMPLE
#endif

#define HAVE_CH_LAYOUT (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100))

// Include the FFmpeg headers
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/display.h>

#if (LIBAVFORMAT_VERSION_MAJOR >= 57)
    #include <libavutil/hwcontext.h> //PM
#endif
    #include <libswscale/swscale.h>

#if USE_SW
    #include <libswresample/swresample.h>
#else
    #include <libavresample/avresample.h>
#endif

    #include <libavutil/mathematics.h>
    #include <libavutil/pixfmt.h>
    #include <libavutil/pixdesc.h>

    // libavutil changed folders at some point
#if LIBAVFORMAT_VERSION_MAJOR >= 53
    #include <libavutil/opt.h>
#else
    #include <libavcodec/opt.h>
#endif

    // channel header refactored
#if LIBAVFORMAT_VERSION_MAJOR >= 54
    #include <libavutil/channel_layout.h>
#endif

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 0, 0)
    #include <libavutil/spherical.h>
#endif

#if IS_FFMPEG_3_2
    #include "libavutil/imgutils.h"
#endif
}

// This was removed from newer versions of FFmpeg (but still used in libopenshot)
#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
    // 1 second of 48khz 32bit audio
    #define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
#endif
#ifndef AV_ERROR_MAX_STRING_SIZE
    #define AV_ERROR_MAX_STRING_SIZE 64
#endif
#ifndef AUDIO_PACKET_ENCODING_SIZE
    // 48khz * S16 (2 bytes) * max channels (8)
    #define AUDIO_PACKET_ENCODING_SIZE 768000
#endif

// This wraps an unsafe C macro to be C++ compatible function
inline static const std::string av_err2string(int errnum)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return static_cast<std::string>(errbuf);
}

// Redefine the C macro to use our new C++ function
#undef av_err2str
#define av_err2str(errnum) av_err2string(errnum).c_str()

// Define this for compatibility
#ifndef PixelFormat
    #define PixelFormat AVPixelFormat
#endif
#ifndef PIX_FMT_RGBA
    #define PIX_FMT_RGBA AV_PIX_FMT_RGBA
#endif
#ifndef PIX_FMT_NONE
    #define PIX_FMT_NONE AV_PIX_FMT_NONE
#endif
#ifndef PIX_FMT_RGB24
    #define PIX_FMT_RGB24 AV_PIX_FMT_RGB24
#endif
#ifndef PIX_FMT_YUV420P
    #define PIX_FMT_YUV420P AV_PIX_FMT_YUV420P
#endif
#ifndef PIX_FMT_YUV444P
    #define PIX_FMT_YUV444P AV_PIX_FMT_YUV444P
#endif

// Does ffmpeg pixel format contain an alpha channel?
inline static bool ffmpeg_has_alpha(PixelFormat pix_fmt) {
    const AVPixFmtDescriptor *fmt_desc = av_pix_fmt_desc_get(pix_fmt);
    return bool(fmt_desc->flags & AV_PIX_FMT_FLAG_ALPHA);
}

// FFmpeg's libavutil/common.h defines an RSHIFT incompatible with Ruby's
// definition in ruby/config.h, so we move it to FF_RSHIFT
#ifdef RSHIFT
    #define FF_RSHIFT(a, b) RSHIFT(a, b)
    #undef RSHIFT
#endif

// libswresample/libavresample API switching
#if USE_SW
    #define SWR_CONVERT(ctx, out, linesize, out_count, in, linesize2, in_count) \
            swr_convert(ctx, out, out_count, (const uint8_t **)in, in_count)
    #define SWR_ALLOC() swr_alloc()
    #define SWR_CLOSE(ctx) {}
    #define SWR_FREE(ctx) swr_free(ctx)
    #define SWR_INIT(ctx)  swr_init(ctx)
    #define SWRCONTEXT SwrContext

#else
    #define SWR_CONVERT(ctx, out, linesize, out_count, in, linesize2, in_count) \
            avresample_convert(ctx, out, linesize, out_count, (uint8_t **)in, linesize2, in_count)
    #define SWR_ALLOC() avresample_alloc_context()
    #define SWR_CLOSE(ctx) avresample_close(ctx)
    #define SWR_FREE(ctx) avresample_free(ctx)
    #define SWR_INIT(ctx)  avresample_open(ctx)
    #define SWRCONTEXT AVAudioResampleContext
#endif


#if (LIBAVFORMAT_VERSION_MAJOR >= 58)
    #define AV_REGISTER_ALL
    #define AVCODEC_REGISTER_ALL
    #define AV_FILENAME url
    #define AV_SET_FILENAME(oc, f) oc->AV_FILENAME = av_strdup(f)
    #define MY_INPUT_BUFFER_PADDING_SIZE AV_INPUT_BUFFER_PADDING_SIZE
    #define AV_ALLOCATE_FRAME() av_frame_alloc()
    #define AV_ALLOCATE_IMAGE(av_frame, pix_fmt, width, height) \
            av_image_alloc(av_frame->data, av_frame->linesize, width, height, pix_fmt, 32)
    #define AV_RESET_FRAME(av_frame) av_frame_unref(av_frame)
    #define AV_FREE_FRAME(av_frame) av_frame_free(av_frame)
    #define AV_FREE_PACKET(av_packet) av_packet_unref(av_packet)
    #define AV_FREE_CONTEXT(av_context) avcodec_free_context(&av_context)
    #define AV_GET_CODEC_TYPE(av_stream) av_stream->codecpar->codec_type
    #define AV_FIND_DECODER_CODEC_ID(av_stream) av_stream->codecpar->codec_id
    #define AV_GET_CODEC_CONTEXT(av_stream, av_codec) \
            ({ AVCodecContext *context = avcodec_alloc_context3(av_codec); \
               avcodec_parameters_to_context(context, av_stream->codecpar); \
               context; })
    #define AV_GET_CODEC_PAR_CONTEXT(av_stream, av_codec) av_codec;
    #define AV_GET_CODEC_FROM_STREAM(av_stream,codec_in)
    #define AV_GET_CODEC_ATTRIBUTES(av_stream, av_context) av_stream->codecpar
    #define AV_GET_CODEC_PIXEL_FORMAT(av_stream, av_context) (AVPixelFormat) av_stream->codecpar->format
    #define AV_GET_SAMPLE_FORMAT(av_stream, av_context) av_stream->codecpar->format
    #define AV_GET_IMAGE_SIZE(pix_fmt, width, height) \
            av_image_get_buffer_size(pix_fmt, width, height, 1)
    #define AV_COPY_PICTURE_DATA(av_frame, buffer, pix_fmt, width, height) \
            av_image_fill_arrays(av_frame->data, av_frame->linesize, buffer, pix_fmt, width, height, 1)
    #define AV_OUTPUT_CONTEXT(output_context, path) avformat_alloc_output_context2( output_context, NULL, NULL, path)
    #define AV_OPTION_FIND(priv_data, name) av_opt_find(priv_data, name, NULL, 0, 0)
    #define AV_OPTION_SET( av_stream, priv_data, name, value, avcodec) \
            av_opt_set(priv_data, name, value, 0); \
            avcodec_parameters_from_context(av_stream->codecpar, avcodec);
    #define ALLOC_CODEC_CTX(ctx, codec, stream) \
            ctx = avcodec_alloc_context3(codec);
    #define AV_COPY_PARAMS_FROM_CONTEXT(av_stream, av_codec_ctx) \
            avcodec_parameters_from_context(av_stream->codecpar, av_codec_ctx);

#elif IS_FFMPEG_3_2
    #define AV_REGISTER_ALL av_register_all();
    #define AVCODEC_REGISTER_ALL    avcodec_register_all();
    #define AV_FILENAME filename
    #define AV_SET_FILENAME(oc, f) snprintf(oc->AV_FILENAME, sizeof(oc->AV_FILENAME), "%s", f)
    #define MY_INPUT_BUFFER_PADDING_SIZE FF_INPUT_BUFFER_PADDING_SIZE
    #define AV_ALLOCATE_FRAME() av_frame_alloc()
    #define AV_ALLOCATE_IMAGE(av_frame, pix_fmt, width, height) \
            av_image_alloc(av_frame->data, av_frame->linesize, width, height, pix_fmt, 32)
    #define AV_RESET_FRAME(av_frame) av_frame_unref(av_frame)
    #define AV_FREE_FRAME(av_frame) av_frame_free(av_frame)
    #define AV_FREE_PACKET(av_packet) av_packet_unref(av_packet)
    #define AV_FREE_CONTEXT(av_context) avcodec_free_context(&av_context)
    #define AV_GET_CODEC_TYPE(av_stream) av_stream->codecpar->codec_type
    #define AV_FIND_DECODER_CODEC_ID(av_stream) av_stream->codecpar->codec_id
    #define AV_GET_CODEC_CONTEXT(av_stream, av_codec) \
            ({ AVCodecContext *context = avcodec_alloc_context3(av_codec); \
               avcodec_parameters_to_context(context, av_stream->codecpar); \
               context; })
    #define AV_GET_CODEC_PAR_CONTEXT(av_stream, av_codec) av_codec;
    #define AV_GET_CODEC_FROM_STREAM(av_stream,codec_in)
    #define AV_GET_CODEC_ATTRIBUTES(av_stream, av_context) av_stream->codecpar
    #define AV_GET_CODEC_PIXEL_FORMAT(av_stream, av_context) \
            (AVPixelFormat) av_stream->codecpar->format
    #define AV_GET_SAMPLE_FORMAT(av_stream, av_context) av_stream->codecpar->format
    #define AV_GET_IMAGE_SIZE(pix_fmt, width, height) av_image_get_buffer_size(pix_fmt, width, height, 1)
    #define AV_COPY_PICTURE_DATA(av_frame, buffer, pix_fmt, width, height) \
            av_image_fill_arrays(av_frame->data, av_frame->linesize, buffer, pix_fmt, width, height, 1)
    #define AV_OUTPUT_CONTEXT(output_context, path) \
            avformat_alloc_output_context2( output_context, NULL, NULL, path)
    #define AV_OPTION_FIND(priv_data, name) av_opt_find(priv_data, name, NULL, 0, 0)
    #define AV_OPTION_SET( av_stream, priv_data, name, value, avcodec) \
            av_opt_set(priv_data, name, value, 0); \
            avcodec_parameters_from_context(av_stream->codecpar, avcodec);
    #define ALLOC_CODEC_CTX(ctx, codec, stream) \
            ctx = avcodec_alloc_context3(codec);
    #define AV_COPY_PARAMS_FROM_CONTEXT(av_stream, av_codec) \
            avcodec_parameters_from_context(av_stream->codecpar, av_codec);

#elif LIBAVFORMAT_VERSION_MAJOR >= 55
    #define AV_REGISTER_ALL av_register_all();
    #define AVCODEC_REGISTER_ALL    avcodec_register_all();
    #define AV_FILENAME filename
    #define AV_SET_FILENAME(oc, f) snprintf(oc->AV_FILENAME, sizeof(oc->AV_FILENAME), "%s", f)
    #define MY_INPUT_BUFFER_PADDING_SIZE FF_INPUT_BUFFER_PADDING_SIZE
    #define AV_ALLOCATE_FRAME() av_frame_alloc()
    #define AV_ALLOCATE_IMAGE(av_frame, pix_fmt, width, height) \
            avpicture_alloc((AVPicture *) av_frame, pix_fmt, width, height)
    #define AV_RESET_FRAME(av_frame) av_frame_unref(av_frame)
    #define AV_FREE_FRAME(av_frame) av_frame_free(av_frame)
    #define AV_FREE_PACKET(av_packet) av_packet_unref(av_packet)
    #define AV_FREE_CONTEXT(av_context) avcodec_close(av_context)
    #define AV_GET_CODEC_TYPE(av_stream) av_stream->codec->codec_type
    #define AV_FIND_DECODER_CODEC_ID(av_stream) av_stream->codec->codec_id
    #define AV_GET_CODEC_CONTEXT(av_stream, av_codec) av_stream->codec
    #define AV_GET_CODEC_PAR_CONTEXT(av_stream, av_codec) av_stream->codec
    #define AV_GET_CODEC_FROM_STREAM(av_stream, codec_in) codec_in = av_stream->codec;
    #define AV_GET_CODEC_ATTRIBUTES(av_stream, av_context) av_context
    #define AV_GET_CODEC_PIXEL_FORMAT(av_stream, av_context) av_context->pix_fmt
    #define AV_GET_SAMPLE_FORMAT(av_stream, av_context) av_context->sample_fmt
    #define AV_GET_IMAGE_SIZE(pix_fmt, width, height) avpicture_get_size(pix_fmt, width, height)
    #define AV_COPY_PICTURE_DATA(av_frame, buffer, pix_fmt, width, height) \
            avpicture_fill((AVPicture *) av_frame, buffer, pix_fmt, width, height)
    #define AV_OUTPUT_CONTEXT(output_context, path) oc = avformat_alloc_context()
    #define AV_OPTION_FIND(priv_data, name) av_opt_find(priv_data, name, NULL, 0, 0)
    #define AV_OPTION_SET(av_stream, priv_data, name, value, avcodec) av_opt_set (priv_data, name, value, 0)
    #define ALLOC_CODEC_CTX(ctx, av_codec, stream) \
            avcodec_get_context_defaults3(av_st->codec, av_codec); \
            ctx = av_st->codec;
    #define AV_COPY_PARAMS_FROM_CONTEXT(av_stream, av_codec)

#else
    #define AV_REGISTER_ALL av_register_all();
    #define AVCODEC_REGISTER_ALL    avcodec_register_all();
    #define AV_FILENAME filename
    #define AV_SET_FILENAME(oc, f) snprintf(oc->AV_FILENAME, sizeof(oc->AV_FILENAME), "%s", f)
    #define MY_INPUT_BUFFER_PADDING_SIZE FF_INPUT_BUFFER_PADDING_SIZE
    #define AV_ALLOCATE_FRAME() avcodec_alloc_frame()
    #define AV_ALLOCATE_IMAGE(av_frame, pix_fmt, width, height) \
            avpicture_alloc((AVPicture *) av_frame, pix_fmt, width, height)
    #define AV_RESET_FRAME(av_frame) avcodec_get_frame_defaults(av_frame)
    #define AV_FREE_FRAME(av_frame) avcodec_free_frame(av_frame)
    #define AV_FREE_PACKET(av_packet) av_free_packet(av_packet)
    #define AV_FREE_CONTEXT(av_context) avcodec_close(av_context)
    #define AV_GET_CODEC_TYPE(av_stream) av_stream->codec->codec_type
    #define AV_FIND_DECODER_CODEC_ID(av_stream) av_stream->codec->codec_id
    #define AV_GET_CODEC_CONTEXT(av_stream, av_codec) av_stream->codec
    #define AV_GET_CODEC_PAR_CONTEXT(av_stream, av_codec) av_stream->codec
    #define AV_GET_CODEC_FROM_STREAM(av_stream, codec_in ) codec_in = av_stream->codec;
    #define AV_GET_CODEC_ATTRIBUTES(av_stream, av_context) av_context
    #define AV_GET_CODEC_PIXEL_FORMAT(av_stream, av_context) av_context->pix_fmt
    #define AV_GET_SAMPLE_FORMAT(av_stream, av_context) av_context->sample_fmt
    #define AV_GET_IMAGE_SIZE(pix_fmt, width, height) avpicture_get_size(pix_fmt, width, height)
    #define AV_COPY_PICTURE_DATA(av_frame, buffer, pix_fmt, width, height) \
            avpicture_fill((AVPicture *) av_frame, buffer, pix_fmt, width, height)
    #define AV_OUTPUT_CONTEXT(output_context, path) oc = avformat_alloc_context()
    #define AV_OPTION_FIND(priv_data, name) av_opt_find(priv_data, name, NULL, 0, 0)
    #define AV_OPTION_SET(av_stream, priv_data, name, value, avcodec) av_opt_set (priv_data, name, value, 0)
    #define ALLOC_CODEC_CTX(ctx, av_codec, stream) \
            avcodec_get_context_defaults3(stream->codec, av_codec); \
            ctx = stream->codec;
    #define AV_COPY_PARAMS_FROM_CONTEXT(av_stream, av_codec)
#endif

// Aligned memory allocation helpers (cross-platform)
#if defined(_WIN32)
#include <malloc.h>
#endif

inline static void* aligned_malloc(size_t size, size_t alignment = 32)
{
#if defined(_WIN32)
    return _aligned_malloc(size, alignment);
#elif defined(__APPLE__) || defined(__linux__)
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0)
        return nullptr;
    return ptr;
#else
#error "aligned_malloc not implemented on this platform"
#endif
}

#endif  // OPENSHOT_FFMPEG_UTILITIES_H
