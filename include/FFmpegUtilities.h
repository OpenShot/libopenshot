/**
 * @file
 * @brief Header file for FFmpegUtilities
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_FFMPEG_UTILITIES_H
#define OPENSHOT_FFMPEG_UTILITIES_H

	// Required for libavformat to build on Windows
	#ifndef INT64_C
	#define INT64_C(c) (c ## LL)
	#define UINT64_C(c) (c ## ULL)
	#endif

	#ifndef IS_FFMPEG_3_2
	#define IS_FFMPEG_3_2 (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 64, 101))
	#endif
	
	#ifndef HAVE_HW_ACCEL
	#define HAVE_HW_ACCEL (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 107, 100))
	#endif

	// Include the FFmpeg headers
	extern "C" {
		#include <libavcodec/avcodec.h>
		#include <libavformat/avformat.h>
	#if (LIBAVFORMAT_VERSION_MAJOR >= 57)
		#include <libavutil/hwcontext.h> //PM
	#endif
		#include <libswscale/swscale.h>
		// Change this to the first version swrescale works
	#if (LIBAVFORMAT_VERSION_MAJOR >= 57)
			#define USE_SW
	#endif
	#ifdef USE_SW
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

		#if IS_FFMPEG_3_2
			#include "libavutil/imgutils.h"
		#endif
	}

	// This was removed from newer versions of FFmpeg (but still used in libopenshot)
	#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
		#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
	#endif
	#ifndef AV_ERROR_MAX_STRING_SIZE
		#define AV_ERROR_MAX_STRING_SIZE 64
	#endif
	#ifndef AUDIO_PACKET_ENCODING_SIZE
		#define AUDIO_PACKET_ENCODING_SIZE 768000		// 48khz * S16 (2 bytes) * max channels (8)
	#endif

	// This wraps an unsafe C macro to be C++ compatible function
	inline static const std::string av_make_error_string(int errnum)
	{
		char errbuf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
		return (std::string)errbuf;
	}

	// Redefine the C macro to use our new C++ function
	#undef av_err2str
	#define av_err2str(errnum) av_make_error_string(errnum).c_str()

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

	// FFmpeg's libavutil/common.h defines an RSHIFT incompatible with Ruby's
	// definition in ruby/config.h, so we move it to FF_RSHIFT
	#ifdef RSHIFT
		#define FF_RSHIFT(a, b) RSHIFT(a, b)
		#undef RSHIFT
	#endif

	#ifdef USE_SW
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
		#define AV_ALLOCATE_IMAGE(av_frame, pix_fmt, width, height) av_image_alloc(av_frame->data, av_frame->linesize, width, height, pix_fmt, 1)
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
		#define AV_GET_IMAGE_SIZE(pix_fmt, width, height) av_image_get_buffer_size(pix_fmt, width, height, 1)
		#define AV_COPY_PICTURE_DATA(av_frame, buffer, pix_fmt, width, height) av_image_fill_arrays(av_frame->data, av_frame->linesize, buffer, pix_fmt, width, height, 1)
		#define AV_OUTPUT_CONTEXT(output_context, path) avformat_alloc_output_context2( output_context, NULL, NULL, path)
		#define AV_OPTION_FIND(priv_data, name) av_opt_find(priv_data, name, NULL, 0, 0)
		#define AV_OPTION_SET( av_stream, priv_data, name, value, avcodec) 	av_opt_set(priv_data, name, value, 0); avcodec_parameters_from_context(av_stream->codecpar, avcodec);
		#define AV_FORMAT_NEW_STREAM(oc, st_codec, av_codec, av_st) 	av_st = avformat_new_stream(oc, NULL);\
			if (!av_st) \
				throw OutOfMemory("Could not allocate memory for the video stream.", path); \
			c = avcodec_alloc_context3(av_codec); \
			st_codec = c; \
			av_st->codecpar->codec_id = av_codec->id;
		#define AV_COPY_PARAMS_FROM_CONTEXT(av_stream, av_codec) avcodec_parameters_from_context(av_stream->codecpar, av_codec);
	#elif IS_FFMPEG_3_2
		#define AV_REGISTER_ALL av_register_all();
		#define AVCODEC_REGISTER_ALL	avcodec_register_all();
		#define AV_FILENAME filename
		#define AV_SET_FILENAME(oc, f) snprintf(oc->AV_FILENAME, sizeof(oc->AV_FILENAME), "%s", f)
		#define MY_INPUT_BUFFER_PADDING_SIZE FF_INPUT_BUFFER_PADDING_SIZE
		#define AV_ALLOCATE_FRAME() av_frame_alloc()
		#define AV_ALLOCATE_IMAGE(av_frame, pix_fmt, width, height) av_image_alloc(av_frame->data, av_frame->linesize, width, height, pix_fmt, 1)
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
		#define AV_GET_IMAGE_SIZE(pix_fmt, width, height) av_image_get_buffer_size(pix_fmt, width, height, 1)
		#define AV_COPY_PICTURE_DATA(av_frame, buffer, pix_fmt, width, height) av_image_fill_arrays(av_frame->data, av_frame->linesize, buffer, pix_fmt, width, height, 1)
		#define AV_OUTPUT_CONTEXT(output_context, path) avformat_alloc_output_context2( output_context, NULL, NULL, path)
		#define AV_OPTION_FIND(priv_data, name) av_opt_find(priv_data, name, NULL, 0, 0)
		#define AV_OPTION_SET( av_stream, priv_data, name, value, avcodec) 	av_opt_set(priv_data, name, value, 0); avcodec_parameters_from_context(av_stream->codecpar, avcodec);
		#define AV_FORMAT_NEW_STREAM(oc, st_codec, av_codec, av_st) 	av_st = avformat_new_stream(oc, NULL);\
			if (!av_st) \
				throw OutOfMemory("Could not allocate memory for the video stream.", path); \
			_Pragma ("GCC diagnostic push"); \
			_Pragma ("GCC diagnostic ignored \"-Wdeprecated-declarations\""); \
			avcodec_get_context_defaults3(av_st->codec, av_codec); \
			c = av_st->codec; \
			_Pragma ("GCC diagnostic pop"); \
			st_codec = c;
		#define AV_COPY_PARAMS_FROM_CONTEXT(av_stream, av_codec) avcodec_parameters_from_context(av_stream->codecpar, av_codec);
	#elif LIBAVFORMAT_VERSION_MAJOR >= 55
		#define AV_REGISTER_ALL av_register_all();
		#define AVCODEC_REGISTER_ALL	avcodec_register_all();
		#define AV_FILENAME filename
		#define AV_SET_FILENAME(oc, f) snprintf(oc->AV_FILENAME, sizeof(oc->AV_FILENAME), "%s", f)
		#define MY_INPUT_BUFFER_PADDING_SIZE FF_INPUT_BUFFER_PADDING_SIZE
		#define AV_ALLOCATE_FRAME() av_frame_alloc()
		#define AV_ALLOCATE_IMAGE(av_frame, pix_fmt, width, height) avpicture_alloc((AVPicture *) av_frame, pix_fmt, width, height)
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
		#define AV_COPY_PICTURE_DATA(av_frame, buffer, pix_fmt, width, height) avpicture_fill((AVPicture *) av_frame, buffer, pix_fmt, width, height)
		#define AV_OUTPUT_CONTEXT(output_context, path) oc = avformat_alloc_context()
		#define AV_OPTION_FIND(priv_data, name) av_opt_find(priv_data, name, NULL, 0, 0)
		#define AV_OPTION_SET(av_stream, priv_data, name, value, avcodec) av_opt_set (priv_data, name, value, 0)
		#define AV_FORMAT_NEW_STREAM( oc,  av_context,  av_codec, av_st) av_st = avformat_new_stream(oc, av_codec); \
			if (!av_st) \
				throw OutOfMemory("Could not allocate memory for the video stream.", path); \
			avcodec_get_context_defaults3(av_st->codec, av_codec); \
			c = av_st->codec;
		#define AV_COPY_PARAMS_FROM_CONTEXT(av_stream, av_codec)
	#else
		#define AV_REGISTER_ALL av_register_all();
		#define AVCODEC_REGISTER_ALL	avcodec_register_all();
		#define AV_FILENAME filename
		#define AV_SET_FILENAME(oc, f) snprintf(oc->AV_FILENAME, sizeof(oc->AV_FILENAME), "%s", f)
		#define MY_INPUT_BUFFER_PADDING_SIZE FF_INPUT_BUFFER_PADDING_SIZE
		#define AV_ALLOCATE_FRAME() avcodec_alloc_frame()
		#define AV_ALLOCATE_IMAGE(av_frame, pix_fmt, width, height) avpicture_alloc((AVPicture *) av_frame, pix_fmt, width, height)
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
		#define AV_COPY_PICTURE_DATA(av_frame, buffer, pix_fmt, width, height) avpicture_fill((AVPicture *) av_frame, buffer, pix_fmt, width, height)
		#define AV_OUTPUT_CONTEXT(output_context, path) oc = avformat_alloc_context()
		#define AV_OPTION_FIND(priv_data, name) av_opt_find(priv_data, name, NULL, 0, 0)
		#define AV_OPTION_SET(av_stream, priv_data, name, value, avcodec) av_opt_set (priv_data, name, value, 0)
		#define AV_FORMAT_NEW_STREAM( oc,  av_context,  av_codec, av_st) av_st = avformat_new_stream(oc, av_codec); \
			if (!av_st) \
				throw OutOfMemory("Could not allocate memory for the video stream.", path); \
			avcodec_get_context_defaults3(av_st->codec, av_codec); \
			c = av_st->codec;
		#define AV_COPY_PARAMS_FROM_CONTEXT(av_stream, av_codec)
	#endif


#endif
