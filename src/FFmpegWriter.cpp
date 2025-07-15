/**
 * @file
 * @brief Source file for FFmpegWriter class
 * @author Jonathan Thomas <jonathan@openshot.org>, Fabrice Bellard
 *
 * This file is originally based on the Libavformat API example, and then modified
 * by the libopenshot project.
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC, Fabrice Bellard
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <algorithm>
#include <iostream>
#include <cmath>
#include <ctime>
#include <unistd.h>

#include "FFmpegUtilities.h"

#include "FFmpegWriter.h"
#include "Exceptions.h"
#include "Frame.h"
#include "OpenMPUtilities.h"
#include "Settings.h"
#include "ZmqLogger.h"

using namespace openshot;

// Multiplexer parameters temporary storage
AVDictionary *mux_dict = NULL;

#if USE_HW_ACCEL
int hw_en_on = 1;					// Is set in UI
int hw_en_supported = 0;	// Is set by FFmpegWriter
AVPixelFormat hw_en_av_pix_fmt = AV_PIX_FMT_NONE;
AVHWDeviceType hw_en_av_device_type = AV_HWDEVICE_TYPE_VAAPI;
static AVBufferRef *hw_device_ctx = NULL;
AVFrame *hw_frame = NULL;

static int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx, int64_t width, int64_t height)
{
	AVBufferRef *hw_frames_ref;
	AVHWFramesContext *frames_ctx = NULL;
	int err = 0;

	if (!(hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx))) {
		std::clog << "Failed to create HW frame context.\n";
		return -1;
	}
	frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
	frames_ctx->format = hw_en_av_pix_fmt;
	frames_ctx->sw_format = AV_PIX_FMT_NV12;
	frames_ctx->width = width;
	frames_ctx->height = height;
	frames_ctx->initial_pool_size = 20;
	if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
		std::clog << "Failed to initialize HW frame context. " <<
			"Error code: " << av_err2string(err) << "\n";
		av_buffer_unref(&hw_frames_ref);
		return err;
	}
	ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
	if (!ctx->hw_frames_ctx)
		err = AVERROR(ENOMEM);

	av_buffer_unref(&hw_frames_ref);
	return err;
}
#endif // USE_HW_ACCEL

FFmpegWriter::FFmpegWriter(const std::string& path) :
		path(path), oc(NULL), audio_st(NULL), video_st(NULL), samples(NULL),
		audio_outbuf(NULL), audio_outbuf_size(0), audio_input_frame_size(0), audio_input_position(0),
		initial_audio_input_frame_size(0), img_convert_ctx(NULL),
		video_codec_ctx(NULL), audio_codec_ctx(NULL), is_writing(false), video_timestamp(0), audio_timestamp(0),
		original_sample_rate(0), original_channels(0), avr(NULL), avr_planar(NULL), is_open(false), prepare_streams(false),
		write_header(false), write_trailer(false), audio_encoder_buffer_size(0), audio_encoder_buffer(NULL) {

	// Disable audio & video (so they can be independently enabled)
	info.has_audio = false;
	info.has_video = false;

	// Initialize FFMpeg, and register all formats and codecs
	AV_REGISTER_ALL

	// auto detect format
	auto_detect_format();
}

// Open the writer
void FFmpegWriter::Open() {
	if (!is_open) {
		// Open the writer
		is_open = true;

		// Prepare streams (if needed)
		if (!prepare_streams)
			PrepareStreams();

		// Now that all the parameters are set, we can open the audio and video codecs and allocate the necessary encode buffers
		if (info.has_video && video_st)
			open_video(oc, video_st);
		if (info.has_audio && audio_st)
			open_audio(oc, audio_st);

		// Write header (if needed)
		if (!write_header)
			WriteHeader();
	}
}

// auto detect format (from path)
void FFmpegWriter::auto_detect_format() {

	// Allocate the output media context
	AV_OUTPUT_CONTEXT(&oc, path.c_str());
	if (!oc) {
		throw OutOfMemory(
			"Could not allocate memory for AVFormatContext.", path);
	}

	// Determine what format to use when encoding this output filename
	oc->oformat = av_guess_format(NULL, path.c_str(), NULL);
	if (oc->oformat == nullptr) {
		throw InvalidFormat(
			"Could not deduce output format from file extension.", path);
	}

	// Update video codec name
	if (oc->oformat->video_codec != AV_CODEC_ID_NONE && info.has_video)
		info.vcodec = avcodec_find_encoder(oc->oformat->video_codec)->name;

	// Update audio codec name
	if (oc->oformat->audio_codec != AV_CODEC_ID_NONE && info.has_audio)
		info.acodec = avcodec_find_encoder(oc->oformat->audio_codec)->name;
}

// initialize streams
void FFmpegWriter::initialize_streams() {
	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::initialize_streams",
		"oc->oformat->video_codec", oc->oformat->video_codec,
		"oc->oformat->audio_codec", oc->oformat->audio_codec,
		"AV_CODEC_ID_NONE", AV_CODEC_ID_NONE);

	// Add the audio and video streams using the default format codecs and initialize the codecs
	video_st = NULL;
	audio_st = NULL;
	if (oc->oformat->video_codec != AV_CODEC_ID_NONE && info.has_video)
		// Add video stream
		video_st = add_video_stream();

	if (oc->oformat->audio_codec != AV_CODEC_ID_NONE && info.has_audio)
		// Add audio stream
		audio_st = add_audio_stream();
}

// Set video export options
void FFmpegWriter::SetVideoOptions(bool has_video, std::string codec, Fraction fps, int width, int height, Fraction pixel_ratio, bool interlaced, bool top_field_first, int bit_rate) {
	// Set the video options
	if (codec.length() > 0) {
		const AVCodec *new_codec;
		// Check if the codec selected is a hardware accelerated codec
#if USE_HW_ACCEL
#if defined(__linux__)
		if (strstr(codec.c_str(), "_vaapi") != NULL) {
			new_codec = avcodec_find_encoder_by_name(codec.c_str());
			hw_en_on = 1;
			hw_en_supported = 1;
			hw_en_av_pix_fmt = AV_PIX_FMT_VAAPI;
			hw_en_av_device_type = AV_HWDEVICE_TYPE_VAAPI;
		} else if (strstr(codec.c_str(), "_nvenc") != NULL) {
			new_codec = avcodec_find_encoder_by_name(codec.c_str());
			hw_en_on = 1;
			hw_en_supported = 1;
			hw_en_av_pix_fmt = AV_PIX_FMT_CUDA;
			hw_en_av_device_type = AV_HWDEVICE_TYPE_CUDA;
		} else {
			new_codec = avcodec_find_encoder_by_name(codec.c_str());
			hw_en_on = 0;
			hw_en_supported = 0;
		}
#elif defined(_WIN32)
		if (strstr(codec.c_str(), "_dxva2") != NULL) {
			new_codec = avcodec_find_encoder_by_name(codec.c_str());
			hw_en_on = 1;
			hw_en_supported = 1;
			hw_en_av_pix_fmt = AV_PIX_FMT_DXVA2_VLD;
			hw_en_av_device_type = AV_HWDEVICE_TYPE_DXVA2;
		} else if (strstr(codec.c_str(), "_nvenc") != NULL) {
			new_codec = avcodec_find_encoder_by_name(codec.c_str());
			hw_en_on = 1;
			hw_en_supported = 1;
			hw_en_av_pix_fmt = AV_PIX_FMT_CUDA;
			hw_en_av_device_type = AV_HWDEVICE_TYPE_CUDA;
		} else {
			new_codec = avcodec_find_encoder_by_name(codec.c_str());
			hw_en_on = 0;
			hw_en_supported = 0;
		}
#elif defined(__APPLE__)
		if (strstr(codec.c_str(), "_videotoolbox") != NULL) {
			new_codec = avcodec_find_encoder_by_name(codec.c_str());
			hw_en_on = 1;
			hw_en_supported = 1;
			hw_en_av_pix_fmt = AV_PIX_FMT_VIDEOTOOLBOX;
			hw_en_av_device_type = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
		} else {
			new_codec = avcodec_find_encoder_by_name(codec.c_str());
			hw_en_on = 0;
			hw_en_supported = 0;
		}
#else  // unknown OS
		new_codec = avcodec_find_encoder_by_name(codec.c_str());
#endif //__linux__/_WIN32/__APPLE__
#else // USE_HW_ACCEL
		new_codec = avcodec_find_encoder_by_name(codec.c_str());
#endif // USE_HW_ACCEL
		if (new_codec == NULL)
			throw InvalidCodec("A valid video codec could not be found for this file.", path);
		else {
			// Set video codec
			info.vcodec = new_codec->name;
		}
	}
	if (fps.num > 0) {
		// Set frames per second (if provided)
		info.fps.num = fps.num;
		info.fps.den = fps.den;

		// Set the timebase (inverse of fps)
		info.video_timebase.num = info.fps.den;
		info.video_timebase.den = info.fps.num;
	}
	if (width >= 1)
		info.width = width;
	if (height >= 1)
		info.height = height;
	if (pixel_ratio.num > 0) {
		info.pixel_ratio.num = pixel_ratio.num;
		info.pixel_ratio.den = pixel_ratio.den;
	}
	if (bit_rate >= 1000)  // bit_rate is the bitrate in b/s
		info.video_bit_rate = bit_rate;
	if ((bit_rate >= 0) && (bit_rate < 256))  // bit_rate is the bitrate in crf
		info.video_bit_rate = bit_rate;

	info.interlaced_frame = interlaced;
	info.top_field_first = top_field_first;

	// Calculate the DAR (display aspect ratio)
	Fraction size(info.width * info.pixel_ratio.num, info.height * info.pixel_ratio.den);

	// Reduce size fraction
	size.Reduce();

	// Set the ratio based on the reduced fraction
	info.display_ratio.num = size.num;
	info.display_ratio.den = size.den;

	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::SetVideoOptions (" + codec + ")",
		"width", width, "height", height,
		"size.num", size.num, "size.den", size.den,
		"fps.num", fps.num, "fps.den", fps.den);

	// Enable / Disable video
	info.has_video = has_video;
}

// Set video export options (overloaded function)
void FFmpegWriter::SetVideoOptions(std::string codec, int width, int height,  Fraction fps, int bit_rate) {
	// Call full signature with some default parameters
	FFmpegWriter::SetVideoOptions(
		true, codec, fps, width, height,
		openshot::Fraction(1, 1), false, true, bit_rate
	);
}


// Set audio export options
void FFmpegWriter::SetAudioOptions(bool has_audio, std::string codec, int sample_rate, int channels, ChannelLayout channel_layout, int bit_rate) {
	// Set audio options
	if (codec.length() > 0) {
		const AVCodec *new_codec = avcodec_find_encoder_by_name(codec.c_str());
		if (new_codec == NULL)
			throw InvalidCodec("A valid audio codec could not be found for this file.", path);
		else {
			// Set audio codec
			info.acodec = new_codec->name;
		}
	}
	if (sample_rate > 7999)
		info.sample_rate = sample_rate;
	if (channels > 0)
		info.channels = channels;
	if (bit_rate > 999)
		info.audio_bit_rate = bit_rate;
	info.channel_layout = channel_layout;

	// init resample options (if zero)
	if (original_sample_rate == 0)
		original_sample_rate = info.sample_rate;
	if (original_channels == 0)
		original_channels = info.channels;

	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::SetAudioOptions (" + codec + ")",
		"sample_rate", sample_rate,
		"channels", channels,
		"bit_rate", bit_rate);

	// Enable / Disable audio
	info.has_audio = has_audio;
}


// Set audio export options (overloaded function)
void FFmpegWriter::SetAudioOptions(std::string codec, int sample_rate, int bit_rate) {
	// Call full signature with some default parameters
	FFmpegWriter::SetAudioOptions(
		true, codec, sample_rate, 2,
		openshot::LAYOUT_STEREO, bit_rate
	);
}


// Set custom options (some codecs accept additional params)
void FFmpegWriter::SetOption(StreamType stream, std::string name, std::string value) {
	// Declare codec context
	AVCodecContext *c = NULL;
	AVStream *st = NULL;
	std::stringstream convert(value);

	if (info.has_video && stream == VIDEO_STREAM && video_st) {
		st = video_st;
		// Get codec context
		c = AV_GET_CODEC_PAR_CONTEXT(st, video_codec_ctx);
		// Was a codec / stream found?
		if (c) {
			if (info.interlaced_frame) {
				c->field_order = info.top_field_first ? AV_FIELD_TT : AV_FIELD_BB;
				// We only use these two version and ignore AV_FIELD_TB and AV_FIELD_BT
				// Otherwise we would need to change the whole export window
			}
		}
	} else if (info.has_audio && stream == AUDIO_STREAM && audio_st) {
		st = audio_st;
		// Get codec context
		c = AV_GET_CODEC_PAR_CONTEXT(st, audio_codec_ctx);
	} else
		throw NoStreamsFound("The stream was not found. Be sure to call PrepareStreams() first.", path);

	// Init AVOption
	const AVOption *option = NULL;

	// Was a codec / stream found?
	if (c)
		// Find AVOption (if it exists)
		option = AV_OPTION_FIND(c->priv_data, name.c_str());

	// Was option found?
	if (option || (name == "g" || name == "qmin" || name == "qmax" || name == "max_b_frames" || name == "mb_decision" ||
				   name == "level" || name == "profile" || name == "slices" || name == "rc_min_rate" || name == "rc_max_rate" ||
				   name == "rc_buffer_size" || name == "crf" || name == "cqp" || name == "qp")) {
		// Check for specific named options
		if (name == "g")
			// Set gop_size
			convert >> c->gop_size;

		else if (name == "qmin")
			// Minimum quantizer
			convert >> c->qmin;

		else if (name == "qmax")
			// Maximum quantizer
			convert >> c->qmax;

		else if (name == "max_b_frames")
			// Maximum number of B-frames between non-B-frames
			convert >> c->max_b_frames;

		else if (name == "mb_decision")
			// Macroblock decision mode
			convert >> c->mb_decision;

		else if (name == "level")
			// Set codec level
			convert >> c->level;

		else if (name == "profile")
			// Set codec profile
			convert >> c->profile;

		else if (name == "slices")
			// Indicates number of picture subdivisions
			convert >> c->slices;

		else if (name == "rc_min_rate")
			// Minimum bitrate
			convert >> c->rc_min_rate;

		else if (name == "rc_max_rate")
			// Maximum bitrate
			convert >> c->rc_max_rate;

		else if (name == "rc_buffer_size")
			// Buffer size
			convert >> c->rc_buffer_size;

		else if (name == "cqp") {
			// encode quality and special settings like lossless
#if USE_HW_ACCEL
			if (hw_en_on) {
				av_opt_set_int(c->priv_data, "qp", std::min(std::stoi(value),63), 0); // 0-63
			} else
#endif // USE_HW_ACCEL
			{
				switch (c->codec_id) {
#if (LIBAVCODEC_VERSION_MAJOR >= 58)
				// FFmpeg 4.0+
				case AV_CODEC_ID_AV1 :
					c->bit_rate = 0;
					av_opt_set_int(c->priv_data, "qp", std::min(std::stoi(value),63), 0); // 0-63
					break;
#endif
				case AV_CODEC_ID_VP8 :
					c->bit_rate = 10000000;
					av_opt_set_int(c->priv_data, "qp", std::max(std::min(std::stoi(value), 63), 4), 0); // 4-63
					break;
				case AV_CODEC_ID_VP9 :
					c->bit_rate = 0;  // Must be zero!
					av_opt_set_int(c->priv_data, "qp", std::min(std::stoi(value), 63), 0); // 0-63
					if (std::stoi(value) == 0) {
						av_opt_set(c->priv_data, "preset", "veryslow", 0);
						av_opt_set_int(c->priv_data, "lossless", 1, 0);
					}
					break;
				case AV_CODEC_ID_H264 :
					av_opt_set_int(c->priv_data, "qp", std::min(std::stoi(value), 51), 0); // 0-51
					if (std::stoi(value) == 0) {
						av_opt_set(c->priv_data, "preset", "veryslow", 0);
							c->pix_fmt = PIX_FMT_YUV444P; // no chroma subsampling
					}
					break;
				case AV_CODEC_ID_HEVC :
					av_opt_set_int(c->priv_data, "qp", std::min(std::stoi(value), 51), 0); // 0-51
					if (std::stoi(value) == 0) {
						av_opt_set(c->priv_data, "preset", "veryslow", 0);
						av_opt_set_int(c->priv_data, "lossless", 1, 0);
					}
					break;
				default:
					// For all other codecs assume a range of 0-63
					av_opt_set_int(c->priv_data, "qp", std::min(std::stoi(value), 63), 0); // 0-63
					c->bit_rate = 0;
				}
			}
		} else if (name == "crf") {
			// encode quality and special settings like lossless
#if USE_HW_ACCEL
			if (hw_en_on) {
				double mbs = 15000000.0;
				if (info.video_bit_rate > 0) {
					if (info.video_bit_rate > 42) {
						mbs = 380000.0;
					}
					else {
						mbs *= std::pow(0.912,info.video_bit_rate);
					}
				}
				c->bit_rate = (int)(mbs);
			} else
#endif // USE_HW_ACCEL
			{
				switch (c->codec_id) {
#if (LIBAVCODEC_VERSION_MAJOR >= 58)
  				// FFmpeg 4.0+
					case AV_CODEC_ID_AV1 :
						c->bit_rate = 0;
						// AV1 only supports "crf" quality values
						av_opt_set_int(c->priv_data, "crf", std::min(std::stoi(value),63), 0);
						break;
#endif
					case AV_CODEC_ID_VP8 :
						c->bit_rate = 10000000;
						av_opt_set_int(c->priv_data, "crf", std::max(std::min(std::stoi(value), 63), 4), 0); // 4-63
						break;
					case AV_CODEC_ID_VP9 :
						c->bit_rate = 0;  // Must be zero!
						av_opt_set_int(c->priv_data, "crf", std::min(std::stoi(value), 63), 0); // 0-63
						if (std::stoi(value) == 0) {
							av_opt_set(c->priv_data, "preset", "veryslow", 0);
							av_opt_set_int(c->priv_data, "lossless", 1, 0);
						}
						break;
					case AV_CODEC_ID_H264 :
						av_opt_set_int(c->priv_data, "crf", std::min(std::stoi(value), 51), 0); // 0-51
						if (std::stoi(value) == 0) {
							av_opt_set(c->priv_data, "preset", "veryslow", 0);
							c->pix_fmt = PIX_FMT_YUV444P; // no chroma subsampling
						}
						break;
					case AV_CODEC_ID_HEVC :
						if (strstr(info.vcodec.c_str(), "svt_hevc") != NULL) {
							av_opt_set_int(c->priv_data, "preset", 7, 0);
							av_opt_set_int(c->priv_data, "forced-idr",1,0);
							av_opt_set_int(c->priv_data, "qp",std::min(std::stoi(value), 51),0);
						}
						else {
							av_opt_set_int(c->priv_data, "crf", std::min(std::stoi(value), 51), 0); // 0-51
						}
						if (std::stoi(value) == 0) {
							av_opt_set(c->priv_data, "preset", "veryslow", 0);
							av_opt_set_int(c->priv_data, "lossless", 1, 0);
						}
						break;
					default:
						// If this codec doesn't support crf calculate a bitrate
						// TODO: find better formula
						double mbs = 15000000.0;
						if (info.video_bit_rate > 0) {
							if (info.video_bit_rate > 42) {
								mbs = 380000.0;
							} else {
								mbs *= std::pow(0.912, info.video_bit_rate);
							}
						}
						c->bit_rate = (int) (mbs);
				}
			}
		} else if (name == "qp") {
			// encode quality and special settings like lossless
#if (LIBAVCODEC_VERSION_MAJOR >= 58)
	// FFmpeg 4.0+
				switch (c->codec_id) {
					case AV_CODEC_ID_AV1 :
						c->bit_rate = 0;
						if (strstr(info.vcodec.c_str(), "svtav1") != NULL) {
							av_opt_set_int(c->priv_data, "qp", std::min(std::stoi(value),63), 0);
						}
						else if (strstr(info.vcodec.c_str(), "rav1e") != NULL) {
							// Set number of tiles to a fixed value
							// TODO Let user choose number of tiles
							av_opt_set_int(c->priv_data, "qp", std::min(std::stoi(value),255), 0);
						}
						else if (strstr(info.vcodec.c_str(), "aom") != NULL) {
							// Set number of tiles to a fixed value
							// TODO Let user choose number of tiles
							// libaom doesn't have qp only crf
							av_opt_set_int(c->priv_data, "crf", std::min(std::stoi(value),63), 0);
						}
						else {
							av_opt_set_int(c->priv_data, "crf", std::min(std::stoi(value),63), 0);
						}
					case AV_CODEC_ID_HEVC :
						c->bit_rate = 0;
						if (strstr(info.vcodec.c_str(), "svt_hevc") != NULL) {
							av_opt_set_int(c->priv_data, "qp", std::min(std::stoi(value),51), 0);
							av_opt_set_int(c->priv_data, "preset", 7, 0);
							av_opt_set_int(c->priv_data, "forced-idr",1,0);
						}
						break;
				}
#endif  // FFmpeg 4.0+
		} else {
			// Set AVOption
			AV_OPTION_SET(st, c->priv_data, name.c_str(), value.c_str(), c);
		}

		ZmqLogger::Instance()->AppendDebugMethod(
			"FFmpegWriter::SetOption (" + (std::string)name + ")",
			"stream == VIDEO_STREAM", stream == VIDEO_STREAM);

	// Muxing dictionary is not part of the codec context.
	// Just reusing SetOption function to set popular multiplexing presets.
	} else if (name == "muxing_preset") {
		if (value == "mp4_faststart") {
			// 'moov' box to the beginning; only for MOV, MP4
			av_dict_set(&mux_dict, "movflags", "faststart", 0);
		} else if (value == "mp4_fragmented") {
			// write selfcontained fragmented file, minimum length of the fragment 8 sec; only for MOV, MP4
			av_dict_set(&mux_dict, "movflags", "frag_keyframe", 0);
			av_dict_set(&mux_dict, "min_frag_duration", "8000000", 0);
		}
	} else {
		throw InvalidOptions("The option is not valid for this codec.", path);
	}

}

/// Determine if codec name is valid
bool FFmpegWriter::IsValidCodec(std::string codec_name) {
	// Initialize FFMpeg, and register all formats and codecs
	AV_REGISTER_ALL

	// Find the codec (if any)
	return avcodec_find_encoder_by_name(codec_name.c_str()) != NULL;
}

// Prepare & initialize streams and open codecs
void FFmpegWriter::PrepareStreams() {
	if (!info.has_audio && !info.has_video)
		throw InvalidOptions("No video or audio options have been set.  You must set has_video or has_audio (or both).", path);

	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::PrepareStreams [" + path + "]",
		"info.has_audio", info.has_audio,
		"info.has_video", info.has_video);

	// Initialize the streams (i.e. add the streams)
	initialize_streams();

	// Mark as 'prepared'
	prepare_streams = true;
}

// Write the file header (after the options are set)
void FFmpegWriter::WriteHeader() {
	if (!info.has_audio && !info.has_video)
		throw InvalidOptions("No video or audio options have been set.  You must set has_video or has_audio (or both).", path);

	// Open the output file, if needed
	if (!(oc->oformat->flags & AVFMT_NOFILE)) {
		if (avio_open(&oc->pb, path.c_str(), AVIO_FLAG_WRITE) < 0)
			throw InvalidFile("Could not open or write file.", path);
	}

	// Force the output filename (which doesn't always happen for some reason)
	AV_SET_FILENAME(oc, path.c_str());

	// Add general metadata (if any)
	for (auto iter = info.metadata.begin(); iter != info.metadata.end(); ++iter) {
		av_dict_set(&oc->metadata, iter->first.c_str(), iter->second.c_str(), 0);
	}

	// Set multiplexing parameters (only for MP4/MOV containers)
	AVDictionary *dict = NULL;
	if (mux_dict) {
		av_dict_copy(&dict, mux_dict, 0);
	}

	// Write the stream header
	if (avformat_write_header(oc, &dict) != 0) {
		ZmqLogger::Instance()->AppendDebugMethod(
			"FFmpegWriter::WriteHeader (avformat_write_header)");
		throw InvalidFile("Could not write header to file.", path);
	};

	// Free multiplexing dictionaries sets
	if (dict) av_dict_free(&dict);
	if (mux_dict) av_dict_free(&mux_dict);

	// Mark as 'written'
	write_header = true;

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::WriteHeader");
}

// Add a frame to the queue waiting to be encoded.
void FFmpegWriter::WriteFrame(std::shared_ptr<openshot::Frame> frame) {
	// Check for open reader (or throw exception)
	if (!is_open)
		throw WriterClosed("The FFmpegWriter is closed.  Call Open() before calling this method.", path);

	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::WriteFrame",
		"frame->number", frame->number,
		"is_writing", is_writing);

	// Write frames to video file
	write_frame(frame);

	// Keep track of the last frame added
	last_frame = frame;
}

// Write all frames in the queue to the video file.
void FFmpegWriter::write_frame(std::shared_ptr<Frame> frame) {
	// Flip writing flag
	is_writing = true;

	// Create blank exception
	bool has_error_encoding_video = false;

	// Process audio frame
	if (info.has_audio && audio_st)
		write_audio_packets(false, frame);

	// Process video frame
	if (info.has_video && video_st)
		process_video_packet(frame);

	if (info.has_video && video_st) {
		// Does this frame's AVFrame still exist
		if (av_frames.count(frame)) {
			// Get AVFrame
			AVFrame *frame_final = av_frames[frame];

			// Write frame to video file
			if (!write_video_packet(frame, frame_final)) {
				has_error_encoding_video = true;
			}

			// Deallocate buffer and AVFrame
			av_freep(&(frame_final->data[0]));
			AV_FREE_FRAME(&frame_final);
			av_frames.erase(frame);
		}
	}

	// Done writing
	is_writing = false;

	// Raise exception from main thread
	if (has_error_encoding_video)
		throw ErrorEncodingVideo("Error while writing raw video frame", -1);
}

// Write a block of frames from a reader
void FFmpegWriter::WriteFrame(ReaderBase *reader, int64_t start, int64_t length) {
	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::WriteFrame (from Reader)",
		"start", start,
		"length", length);

	// Loop through each frame (and encoded it)
	for (int64_t number = start; number <= length; number++) {
		// Get the frame
		std::shared_ptr<Frame> f = reader->GetFrame(number);

		// Encode frame
		WriteFrame(f);
	}
}

// Write the file trailer (after all frames are written)
void FFmpegWriter::WriteTrailer() {
	// Process final audio frame (if any)
	if (info.has_audio && audio_st)
		write_audio_packets(true, NULL);

	// Flush encoders (who sometimes hold on to frames)
	flush_encoders();

	/* write the trailer, if any. The trailer must be written
	 * before you close the CodecContexts open when you wrote the
	 * header; otherwise write_trailer may try to use memory that
	 * was freed on av_codec_close() */
	av_write_trailer(oc);

	// Mark as 'written'
	write_trailer = true;

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::WriteTrailer");
}

// Flush encoders
void FFmpegWriter::flush_encoders() {
	if (info.has_audio && audio_codec_ctx && AV_GET_CODEC_TYPE(audio_st) == AVMEDIA_TYPE_AUDIO && AV_GET_CODEC_ATTRIBUTES(audio_st, audio_codec_ctx)->frame_size <= 1)
		return;
#if (LIBAVFORMAT_VERSION_MAJOR < 58)
	// FFmpeg < 4.0
	if (info.has_video && video_codec_ctx && AV_GET_CODEC_TYPE(video_st) == AVMEDIA_TYPE_VIDEO && (oc->oformat->flags & AVFMT_RAWPICTURE) && AV_FIND_DECODER_CODEC_ID(video_st) == AV_CODEC_ID_RAWVIDEO)
		return;
#else
	if (info.has_video && video_codec_ctx && AV_GET_CODEC_TYPE(video_st) == AVMEDIA_TYPE_VIDEO && AV_FIND_DECODER_CODEC_ID(video_st) == AV_CODEC_ID_RAWVIDEO)
		return;
#endif

	// FLUSH VIDEO ENCODER
	if (info.has_video) {
		for (;;) {

			// Increment PTS (in frames and scaled to the codec's timebase)
			video_timestamp += av_rescale_q(1, av_make_q(info.fps.den, info.fps.num), video_codec_ctx->time_base);

#if IS_FFMPEG_3_2
			AVPacket* pkt = av_packet_alloc();
#else
			AVPacket* pkt;
			av_init_packet(pkt);
#endif
			pkt->data = NULL;
			pkt->size = 0;

			/* encode the image */
			int got_packet = 0;
			int error_code = 0;

#if IS_FFMPEG_3_2
			// Encode video packet (latest version of FFmpeg)
			error_code = avcodec_send_frame(video_codec_ctx, NULL);
			got_packet = 0;
			while (error_code >= 0) {
				error_code = avcodec_receive_packet(video_codec_ctx, pkt);
				if (error_code == AVERROR(EAGAIN)|| error_code == AVERROR_EOF) {
					got_packet = 0;
					// Write packet
					avcodec_flush_buffers(video_codec_ctx);
					break;
				}
				av_packet_rescale_ts(pkt, video_codec_ctx->time_base, video_st->time_base);
				pkt->stream_index = video_st->index;
				error_code = av_interleaved_write_frame(oc, pkt);
			}
#else // IS_FFMPEG_3_2

			// Encode video packet (older than FFmpeg 3.2)
			error_code = avcodec_encode_video2(video_codec_ctx, pkt, NULL, &got_packet);

#endif // IS_FFMPEG_3_2

			if (error_code < 0) {
				ZmqLogger::Instance()->AppendDebugMethod(
					"FFmpegWriter::flush_encoders ERROR ["
						+ av_err2string(error_code) + "]",
					"error_code", error_code);
			}
			if (!got_packet) {
				break;
			}

			// set the timestamp
			av_packet_rescale_ts(pkt, video_codec_ctx->time_base, video_st->time_base);
			pkt->stream_index = video_st->index;

			// Write packet
			error_code = av_interleaved_write_frame(oc, pkt);
			if (error_code < 0) {
				ZmqLogger::Instance()->AppendDebugMethod(
					"FFmpegWriter::flush_encoders ERROR ["
						+ av_err2string(error_code) + "]",
					"error_code", error_code);
			}
		}
	}

	// FLUSH AUDIO ENCODER
	if (info.has_audio) {
		for (;;) {
#if IS_FFMPEG_3_2
			AVPacket* pkt = av_packet_alloc();
#else
			AVPacket* pkt;
			av_init_packet(pkt);
#endif
			pkt->data = NULL;
			pkt->size = 0;
			pkt->pts = pkt->dts = audio_timestamp;

			/* encode the image */
			int error_code = 0;
			int got_packet = 0;
#if IS_FFMPEG_3_2
			error_code = avcodec_send_frame(audio_codec_ctx, NULL);
#else
			error_code = avcodec_encode_audio2(audio_codec_ctx, pkt, NULL, &got_packet);
#endif
			if (error_code < 0) {
				ZmqLogger::Instance()->AppendDebugMethod(
					"FFmpegWriter::flush_encoders ERROR ["
						+ av_err2string(error_code) + "]",
					"error_code", error_code);
			}
			if (!got_packet) {
				break;
			}

			// Since the PTS can change during encoding, set the value again.  This seems like a huge hack,
			// but it fixes lots of PTS related issues when I do this.
			pkt->pts = pkt->dts = audio_timestamp;

			// Scale the PTS to the audio stream timebase (which is sometimes different than the codec's timebase)
			av_packet_rescale_ts(pkt, audio_codec_ctx->time_base, audio_st->time_base);

			// set stream
			pkt->stream_index = audio_st->index;
			pkt->flags |= AV_PKT_FLAG_KEY;

			// Write packet
			error_code = av_interleaved_write_frame(oc, pkt);
			if (error_code < 0) {
				ZmqLogger::Instance()->AppendDebugMethod(
					"FFmpegWriter::flush_encoders ERROR ["
						+ av_err2string(error_code) + "]",
					"error_code", error_code);
			}

			// Increment PTS by duration of packet
			audio_timestamp += pkt->duration;

			// deallocate memory for packet
			AV_FREE_PACKET(pkt);
		}
	}

}

// Close the video codec
void FFmpegWriter::close_video(AVFormatContext *oc, AVStream *st)
{
#if USE_HW_ACCEL
	if (hw_en_on && hw_en_supported) {
		if (hw_device_ctx) {
			av_buffer_unref(&hw_device_ctx);
			hw_device_ctx = NULL;
		}
	}
#endif // USE_HW_ACCEL

	// Free any previous memory allocations
	if (video_codec_ctx != nullptr) {
		AV_FREE_CONTEXT(video_codec_ctx);
		av_free(video_codec_ctx);
	}
}

// Close the audio codec
void FFmpegWriter::close_audio(AVFormatContext *oc, AVStream *st)
{
	// Clear buffers
	delete[] samples;
	delete[] audio_outbuf;
	delete[] audio_encoder_buffer;
	samples = NULL;
	audio_outbuf = NULL;
	audio_encoder_buffer = NULL;

	// Deallocate resample buffer
	if (avr) {
		SWR_CLOSE(avr);
		SWR_FREE(&avr);
		avr = NULL;
	}

	if (avr_planar) {
		SWR_CLOSE(avr_planar);
		SWR_FREE(&avr_planar);
		avr_planar = NULL;
	}

	// Free any previous memory allocations
	if (audio_codec_ctx != nullptr) {
		AV_FREE_CONTEXT(audio_codec_ctx);
		av_free(audio_codec_ctx);
	}
}

// Close the writer
void FFmpegWriter::Close() {
	// Write trailer (if needed)
	if (!write_trailer)
		WriteTrailer();

	// Close each codec
	if (video_st)
		close_video(oc, video_st);
	if (audio_st)
		close_audio(oc, audio_st);

	// Remove single software scaler
	if (img_convert_ctx)
		sws_freeContext(img_convert_ctx);

	if (!(oc->oformat->flags & AVFMT_NOFILE)) {
		/* close the output file */
		avio_close(oc->pb);
	}

	// Reset frame counters
	video_timestamp = 0;
	audio_timestamp = 0;

	// Free the context which frees the streams too
	avformat_free_context(oc);
	oc = NULL;

	// Close writer
	is_open = false;
	prepare_streams = false;
	write_header = false;
	write_trailer = false;

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::Close");
}

// Add an AVFrame to the cache
void FFmpegWriter::add_avframe(std::shared_ptr<Frame> frame, AVFrame *av_frame) {
	// Add AVFrame to map (if it does not already exist)
	if (!av_frames.count(frame)) {
		// Add av_frame
		av_frames[frame] = av_frame;
	} else {
		// Do not add, and deallocate this AVFrame
		AV_FREE_FRAME(&av_frame);
	}
}

// Add an audio output stream
AVStream *FFmpegWriter::add_audio_stream() {
	// Find the audio codec
	const AVCodec *codec = avcodec_find_encoder_by_name(info.acodec.c_str());
	if (codec == NULL)
		throw InvalidCodec("A valid audio codec could not be found for this file.", path);

	// Free any previous memory allocations
	if (audio_codec_ctx != nullptr) {
		AV_FREE_CONTEXT(audio_codec_ctx);
	}

	// Create a new audio stream
	AVStream* st = avformat_new_stream(oc, codec);
	if (!st)
		throw OutOfMemory("Could not allocate memory for the audio stream.", path);

	// Allocate a new codec context for the stream
	ALLOC_CODEC_CTX(audio_codec_ctx, codec, st)
#if (LIBAVFORMAT_VERSION_MAJOR >= 58)
	st->codecpar->codec_id = codec->id;
#endif
	AVCodecContext* c = audio_codec_ctx;

	c->codec_id = codec->id;
	c->codec_type = AVMEDIA_TYPE_AUDIO;

	// Set the sample parameters
	c->bit_rate = info.audio_bit_rate;
#if !HAVE_CH_LAYOUT
	c->channels = info.channels;
#endif

	// Set valid sample rate (or throw error)
	if (codec->supported_samplerates) {
		int i;
		for (i = 0; codec->supported_samplerates[i] != 0; i++)
			if (info.sample_rate == codec->supported_samplerates[i]) {
				// Set the valid sample rate
				c->sample_rate = info.sample_rate;
				break;
			}
		if (codec->supported_samplerates[i] == 0)
			throw InvalidSampleRate("An invalid sample rate was detected for this codec.", path);
	} else
		// Set sample rate
		c->sample_rate = info.sample_rate;

	uint64_t channel_layout = info.channel_layout;
#if HAVE_CH_LAYOUT
	// Set a valid number of channels (or throw error)
	AVChannelLayout ch_layout;
	av_channel_layout_from_mask(&ch_layout, info.channel_layout);
	if (codec->ch_layouts) {
		int i;
		for (i = 0; av_channel_layout_check(&codec->ch_layouts[i]); i++)
			if (av_channel_layout_compare(&ch_layout, &codec->ch_layouts[i])) {
				// Set valid channel layout
				av_channel_layout_copy(&c->ch_layout, &ch_layout);
				break;
			}
		if (!av_channel_layout_check(&codec->ch_layouts[i]))
			throw InvalidChannels("An invalid channel layout was detected (i.e. MONO / STEREO).", path);
	} else
		// Set valid channel layout
		av_channel_layout_copy(&c->ch_layout, &ch_layout);
#else
	// Set a valid number of channels (or throw error)
	if (codec->channel_layouts) {
		int i;
		for (i = 0; codec->channel_layouts[i] != 0; i++)
			if (channel_layout == codec->channel_layouts[i]) {
				// Set valid channel layout
				c->channel_layout = channel_layout;
				break;
			}
		if (codec->channel_layouts[i] == 0)
			throw InvalidChannels("An invalid channel layout was detected (i.e. MONO / STEREO).", path);
		} else
			// Set valid channel layout
			c->channel_layout = channel_layout;
#endif

	// Choose a valid sample_fmt
	if (codec->sample_fmts) {
		for (int i = 0; codec->sample_fmts[i] != AV_SAMPLE_FMT_NONE; i++) {
			// Set sample format to 1st valid format (and then exit loop)
			c->sample_fmt = codec->sample_fmts[i];
			break;
		}
	}
	if (c->sample_fmt == AV_SAMPLE_FMT_NONE) {
		// Default if no sample formats found
		c->sample_fmt = AV_SAMPLE_FMT_S16;
	}

	// some formats want stream headers to be separate
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
#if (LIBAVCODEC_VERSION_MAJOR >= 57)
		// FFmpeg 3.0+
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
#else
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
#endif

	AV_COPY_PARAMS_FROM_CONTEXT(st, c);

	int nb_channels;
	const char* nb_channels_label;
	const char* channel_layout_label;

#if HAVE_CH_LAYOUT
    nb_channels = c->ch_layout.nb_channels;
    channel_layout = c->ch_layout.u.mask;
    nb_channels_label = "c->ch_layout.nb_channels";
    channel_layout_label = "c->ch_layout.u.mask";
#else
    nb_channels = c->channels;
    nb_channels_label = "c->channels";
    channel_layout_label = "c->channel_layout";
#endif

	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::add_audio_stream",
		"c->codec_id", c->codec_id,
		"c->bit_rate", c->bit_rate,
		nb_channels_label, nb_channels,
		"c->sample_fmt", c->sample_fmt,
		channel_layout_label, channel_layout,
		"c->sample_rate", c->sample_rate);

	return st;
}

// Add a video output stream
AVStream *FFmpegWriter::add_video_stream() {
	// Find the video codec
	const AVCodec *codec = avcodec_find_encoder_by_name(info.vcodec.c_str());
	if (codec == NULL)
		throw InvalidCodec("A valid video codec could not be found for this file.", path);

	// Free any previous memory allocations
	if (video_codec_ctx != nullptr) {
		AV_FREE_CONTEXT(video_codec_ctx);
	}

	// Create a new video stream
	AVStream* st = avformat_new_stream(oc, codec);
	if (!st)
		throw OutOfMemory("Could not allocate memory for the video stream.", path);

	// Allocate a new codec context for the stream
	ALLOC_CODEC_CTX(video_codec_ctx, codec, st)
#if (LIBAVFORMAT_VERSION_MAJOR >= 58)
	st->codecpar->codec_id = codec->id;
#endif

	AVCodecContext* c = video_codec_ctx;

	c->codec_id = codec->id;
	c->codec_type = AVMEDIA_TYPE_VIDEO;

	// Set sample aspect ratio
	c->sample_aspect_ratio.num = info.pixel_ratio.num;
	c->sample_aspect_ratio.den = info.pixel_ratio.den;

	/* Init video encoder options */
	if (info.video_bit_rate >= 1000
#if (LIBAVCODEC_VERSION_MAJOR >= 58)
		&& c->codec_id != AV_CODEC_ID_AV1
#endif
		) {
		c->bit_rate = info.video_bit_rate;
		if (info.video_bit_rate >= 1500000) {
			if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO)  {
				c->qmin = 2;
				c->qmax = 30;
			}
		}
		// Here should be the setting for low fixed bitrate
		// Defaults are used because mpeg2 otherwise had problems
	} else {
		// Check if codec supports crf or qp
		switch (c->codec_id) {
#if (LIBAVCODEC_VERSION_MAJOR >= 58)
			// FFmpeg 4.0+
			case AV_CODEC_ID_AV1 :
			// TODO: Set `crf` or `qp` according to bitrate, as bitrate is not supported by these encoders yet.
			if (info.video_bit_rate >= 1000) {
				c->bit_rate = 0;
				if (strstr(info.vcodec.c_str(), "aom") != NULL) {
					int calculated_quality = 35;
					if (info.video_bit_rate < 500000) calculated_quality = 50;
					if (info.video_bit_rate > 5000000) calculated_quality = 10;
					av_opt_set_int(c->priv_data, "crf", calculated_quality, 0);
					info.video_bit_rate = calculated_quality;
				} else {
					int calculated_quality = 50;
					if (info.video_bit_rate < 500000) calculated_quality = 60;
					if (info.video_bit_rate > 5000000) calculated_quality = 15;
					av_opt_set_int(c->priv_data, "qp", calculated_quality, 0);
					info.video_bit_rate = calculated_quality;
				} // medium
			}
			if (strstr(info.vcodec.c_str(), "svtav1") != NULL) {
				av_opt_set_int(c->priv_data, "preset", 6, 0);
				av_opt_set_int(c->priv_data, "forced-idr",1,0);
			}
			else if (strstr(info.vcodec.c_str(), "rav1e") != NULL) {
				av_opt_set_int(c->priv_data, "speed", 7, 0);
				av_opt_set_int(c->priv_data, "tile-rows", 2, 0);
				av_opt_set_int(c->priv_data, "tile-columns", 4, 0);
			}
			else if (strstr(info.vcodec.c_str(), "aom") != NULL) {
				// Set number of tiles to a fixed value
				// TODO: Allow user to chose their own number of tiles
				av_opt_set_int(c->priv_data, "tile-rows", 1, 0);		// log2 of number of rows
				av_opt_set_int(c->priv_data, "tile-columns", 2, 0);		// log2 of number of columns
				av_opt_set_int(c->priv_data, "row-mt", 1, 0);			// use multiple cores
				av_opt_set_int(c->priv_data, "cpu-used", 3, 0);			// default is 1, usable is 4
			}
			//break;
#endif
			case AV_CODEC_ID_VP9 :
			case AV_CODEC_ID_HEVC :
			case AV_CODEC_ID_VP8 :
			case AV_CODEC_ID_H264 :
				if (info.video_bit_rate < 40) {
					c->qmin = 0;
					c->qmax = 63;
				} else {
					c->qmin = info.video_bit_rate - 5;
					c->qmax = 63;
				}
				break;
			default:
				// Here should be the setting for codecs that don't support crf
				// For now defaults are used
				break;
		}
	}

	//TODO: Implement variable bitrate feature (which actually works). This implementation throws
	//invalid bitrate errors and rc buffer underflow errors, etc...
	//c->rc_min_rate = info.video_bit_rate;
	//c->rc_max_rate = info.video_bit_rate;
	//c->rc_buffer_size = FFMAX(c->rc_max_rate, 15000000) * 112L / 15000000 * 16384;
	//if ( !c->rc_initial_buffer_occupancy )
	//	c->rc_initial_buffer_occupancy = c->rc_buffer_size * 3/4;

	/* resolution must be a multiple of two */
	// TODO: require /2 height and width
	c->width = info.width;
	c->height = info.height;

	/* time base: this is the fundamental unit of time (in seconds) in terms
	 of which frame timestamps are represented. for fixed-fps content,
	 timebase should be 1/framerate and timestamp increments should be
	 identically 1. */
	c->time_base.num = info.video_timebase.num;
	c->time_base.den = info.video_timebase.den;
// AVCodecContext->framerate was added in FFmpeg 2.6
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 26, 0)
	c->framerate = av_inv_q(c->time_base);
#endif
	st->avg_frame_rate = av_inv_q(c->time_base);
	st->time_base.num = info.video_timebase.num;
	st->time_base.den = info.video_timebase.den;

	c->gop_size = 12; /* TODO: add this to "info"... emit one intra frame every twelve frames at most */
	c->max_b_frames = 10;
	if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
		/* just for testing, we also add B frames */
		c->max_b_frames = 2;
	if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
		/* Needed to avoid using macroblocks in which some coeffs overflow.
		 This does not happen with normal video, it just happens here as
		 the motion of the chroma plane does not match the luma plane. */
		c->mb_decision = 2;
	// some formats want stream headers to be separate
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
#if (LIBAVCODEC_VERSION_MAJOR >= 57)
		// FFmpeg 3.0+
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
#else
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
#endif

	// Find all supported pixel formats for this codec
	const PixelFormat *supported_pixel_formats = codec->pix_fmts;
	while (supported_pixel_formats != NULL && *supported_pixel_formats != PIX_FMT_NONE) {
		// Assign the 1st valid pixel format (if one is missing)
		if (c->pix_fmt == PIX_FMT_NONE)
			c->pix_fmt = *supported_pixel_formats;
		++supported_pixel_formats;
	}

	// Codec doesn't have any pix formats?
	if (c->pix_fmt == PIX_FMT_NONE) {
		if (oc->oformat->video_codec == AV_CODEC_ID_RAWVIDEO) {
			// Raw video should use RGB24
			c->pix_fmt = PIX_FMT_RGB24;

#if (LIBAVFORMAT_VERSION_MAJOR < 58)
			// FFmpeg < 4.0
			if (strcmp(oc->oformat->name, "gif") != 0)
				// If not GIF format, skip the encoding process
				// Set raw picture flag (so we don't encode this video)
				oc->oformat->flags |= AVFMT_RAWPICTURE;
#endif
		} else {
			// Set the default codec
			c->pix_fmt = PIX_FMT_YUV420P;
		}
	}

	AV_COPY_PARAMS_FROM_CONTEXT(st, c);
	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::add_video_stream ("
			+ (std::string)oc->oformat->name + " : "
			+ (std::string)av_get_pix_fmt_name(c->pix_fmt) + ")",
		"c->codec_id", c->codec_id,
		"c->bit_rate", c->bit_rate,
		"c->pix_fmt", c->pix_fmt,
		"oc->oformat->flags", oc->oformat->flags);
	return st;
}

// open audio codec
void FFmpegWriter::open_audio(AVFormatContext *oc, AVStream *st) {
	const AVCodec *codec;
	AV_GET_CODEC_FROM_STREAM(st, audio_codec_ctx)

	// Audio encoding does not typically use more than 2 threads (most codecs use 1 thread)
	audio_codec_ctx->thread_count = std::min(FF_AUDIO_NUM_PROCESSORS, 2);

	// Find the audio encoder
	codec = avcodec_find_encoder_by_name(info.acodec.c_str());
	if (!codec)
		codec = avcodec_find_encoder(audio_codec_ctx->codec_id);
	if (!codec)
		throw InvalidCodec("Could not find codec", path);

	// Init options
	AVDictionary *opts = NULL;
	av_dict_set(&opts, "strict", "experimental", 0);

	// Open the codec
	if (avcodec_open2(audio_codec_ctx, codec, &opts) < 0)
		throw InvalidCodec("Could not open audio codec", path);
	AV_COPY_PARAMS_FROM_CONTEXT(st, audio_codec_ctx);

	// Free options
	av_dict_free(&opts);

	// Calculate the size of the input frame (i..e how many samples per packet), and the output buffer
	// TODO: Ugly hack for PCM codecs (will be removed ASAP with new PCM support to compute the input frame size in samples
	if (audio_codec_ctx->frame_size <= 1) {
		// No frame size found... so calculate
		audio_input_frame_size = 50000 / info.channels;

		int s = AV_FIND_DECODER_CODEC_ID(st);
		switch (s) {
			case AV_CODEC_ID_PCM_S16LE:
			case AV_CODEC_ID_PCM_S16BE:
			case AV_CODEC_ID_PCM_U16LE:
			case AV_CODEC_ID_PCM_U16BE:
				audio_input_frame_size >>= 1;
				break;
			default:
				break;
		}
	} else {
		// Set frame size based on the codec
		audio_input_frame_size = audio_codec_ctx->frame_size;
	}

	// Set the initial frame size (since it might change during resampling)
	initial_audio_input_frame_size = audio_input_frame_size;

	// Allocate array for samples
	samples = new int16_t[AVCODEC_MAX_AUDIO_FRAME_SIZE];

	// Set audio output buffer (used to store the encoded audio)
	audio_outbuf_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
	audio_outbuf = new uint8_t[audio_outbuf_size];

	// Set audio packet encoding buffer
	audio_encoder_buffer_size = AUDIO_PACKET_ENCODING_SIZE;
	audio_encoder_buffer = new uint8_t[audio_encoder_buffer_size];

	// Add audio metadata (if any)
	for (std::map<std::string, std::string>::iterator iter = info.metadata.begin(); iter != info.metadata.end(); ++iter) {
		av_dict_set(&st->metadata, iter->first.c_str(), iter->second.c_str(), 0);
	}

	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::open_audio",
		"audio_codec_ctx->thread_count", audio_codec_ctx->thread_count,
		"audio_input_frame_size", audio_input_frame_size,
		"buffer_size", AVCODEC_MAX_AUDIO_FRAME_SIZE + MY_INPUT_BUFFER_PADDING_SIZE);
}

// open video codec
void FFmpegWriter::open_video(AVFormatContext *oc, AVStream *st) {
	const AVCodec *codec;
	AV_GET_CODEC_FROM_STREAM(st, video_codec_ctx)

	// Set number of threads equal to number of processors (not to exceed 16, FFmpeg doesn't recommend more than 16)
	video_codec_ctx->thread_count = std::min(FF_VIDEO_NUM_PROCESSORS, 16);

#if USE_HW_ACCEL
	if (hw_en_on && hw_en_supported) {
		//char *dev_hw = NULL;
		char adapter[256];
		char *adapter_ptr = NULL;
		int adapter_num;
		// Use the hw device given in the environment variable HW_EN_DEVICE_SET or the default if not set
		adapter_num = openshot::Settings::Instance()->HW_EN_DEVICE_SET;
		std::clog << "Encoding Device Nr: " << adapter_num << "\n";
		if (adapter_num < 3 && adapter_num >=0) {
#if defined(__linux__)
				snprintf(adapter,sizeof(adapter),"/dev/dri/renderD%d", adapter_num+128);
				// Maybe 127 is better because the first card would be 1?!
				adapter_ptr = adapter;
#elif defined(_WIN32) || defined(__APPLE__)
				adapter_ptr = NULL;
#endif
			}
			else {
				adapter_ptr = NULL; // Just to be sure
			}
// Check if it is there and writable
#if defined(__linux__)
		if( adapter_ptr != NULL && access( adapter_ptr, W_OK ) == 0 ) {
#elif defined(_WIN32) || defined(__APPLE__)
		if( adapter_ptr != NULL ) {
#endif
			ZmqLogger::Instance()->AppendDebugMethod(
				"Encode Device present using device",
				"adapter", adapter_num);
		}
		else {
			adapter_ptr = NULL;  // use default
			ZmqLogger::Instance()->AppendDebugMethod(
				"Encode Device not present, using default");
		}
		if (av_hwdevice_ctx_create(&hw_device_ctx,
				hw_en_av_device_type, adapter_ptr, NULL, 0) < 0)
		{
			ZmqLogger::Instance()->AppendDebugMethod(
				"FFmpegWriter::open_video ERROR creating hwdevice, Codec name:",
				info.vcodec.c_str(), -1);
			throw InvalidCodec("Could not create hwdevice", path);
		}
	}
#endif // USE_HW_ACCEL

	/* find the video encoder */
	codec = avcodec_find_encoder_by_name(info.vcodec.c_str());
	if (!codec)
		codec = avcodec_find_encoder(AV_FIND_DECODER_CODEC_ID(st));
	if (!codec)
		throw InvalidCodec("Could not find codec", path);

	/* Force max_b_frames to 0 in some cases (i.e. for mjpeg image sequences */
	if (video_codec_ctx->max_b_frames && video_codec_ctx->codec_id != AV_CODEC_ID_MPEG4 && video_codec_ctx->codec_id != AV_CODEC_ID_MPEG1VIDEO && video_codec_ctx->codec_id != AV_CODEC_ID_MPEG2VIDEO)
		video_codec_ctx->max_b_frames = 0;

	// Init options
	AVDictionary *opts = NULL;
	av_dict_set(&opts, "strict", "experimental", 0);

#if USE_HW_ACCEL
	if (hw_en_on && hw_en_supported) {
		video_codec_ctx->pix_fmt   = hw_en_av_pix_fmt;

		// for the list of possible options, see the list of codec-specific options:
		// e.g. ffmpeg -h encoder=h264_vaapi or ffmpeg -h encoder=hevc_vaapi
		// and "man ffmpeg-codecs"

		// For VAAPI, it is safer to explicitly set rc_mode instead of relying on auto-selection
		// which is ffmpeg version-specific.
		if (hw_en_av_pix_fmt == AV_PIX_FMT_VAAPI) {
			int64_t qp;
			if (av_opt_get_int(video_codec_ctx->priv_data, "qp", 0, &qp) != 0 || qp == 0) {
				// unless "qp" was set for CQP, switch to VBR RC mode
				av_opt_set(video_codec_ctx->priv_data, "rc_mode", "VBR", 0);

				// In the current state (ffmpeg-4.2-4 libva-mesa-driver-19.1.5-1) to use VBR,
				// one has to specify both bit_rate and maxrate, otherwise a small low quality file is generated on Intel iGPU).
				video_codec_ctx->rc_max_rate = video_codec_ctx->bit_rate;
			}
		}

		switch (video_codec_ctx->codec_id) {
			case AV_CODEC_ID_H264:
				video_codec_ctx->max_b_frames = 0;  // At least this GPU doesn't support b-frames
				video_codec_ctx->profile = FF_PROFILE_H264_BASELINE | FF_PROFILE_H264_CONSTRAINED;
				av_opt_set(video_codec_ctx->priv_data, "preset", "slow", 0);
				av_opt_set(video_codec_ctx->priv_data, "tune", "zerolatency", 0);
				av_opt_set(video_codec_ctx->priv_data, "vprofile", "baseline", AV_OPT_SEARCH_CHILDREN);
				break;
			case AV_CODEC_ID_HEVC:
				// tested to work with defaults
				break;
			case AV_CODEC_ID_VP9:
				// tested to work with defaults
				break;
			default:
				ZmqLogger::Instance()->AppendDebugMethod(
					"No codec-specific options defined for this codec. HW encoding may fail",
					"codec_id", video_codec_ctx->codec_id);
				break;
		}

		// set hw_frames_ctx for encoder's AVCodecContext
		int err;
		if ((err = set_hwframe_ctx(video_codec_ctx, hw_device_ctx, info.width, info.height)) < 0)
		{
			ZmqLogger::Instance()->AppendDebugMethod(
				"FFmpegWriter::open_video (set_hwframe_ctx) ERROR faled to set hwframe context",
				"width", info.width,
				"height", info.height,
				av_err2string(err), -1);
		}
	}
#endif // USE_HW_ACCEL

// Set libx265 hvc1 tag (for Apple playback compatibility).
#if USE_HW_ACCEL
		if (!(hw_en_on && hw_en_supported) && video_codec_ctx->codec_id == AV_CODEC_ID_HEVC) {
			video_codec_ctx->codec_tag = MKTAG('h', 'v', 'c', '1');
		}
#else
		if (video_codec_ctx->codec_id == AV_CODEC_ID_HEVC) {
			video_codec_ctx->codec_tag = MKTAG('h', 'v', 'c', '1');
		}
#endif

	/* open the codec */
	if (avcodec_open2(video_codec_ctx, codec, &opts) < 0)
		throw InvalidCodec("Could not open video codec", path);
	AV_COPY_PARAMS_FROM_CONTEXT(st, video_codec_ctx);

	// Free options
	av_dict_free(&opts);

	// Add video metadata (if any)
	for (auto iter = info.metadata.begin(); iter != info.metadata.end(); ++iter) {
		av_dict_set(&st->metadata, iter->first.c_str(), iter->second.c_str(), 0);
	}

	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::open_video",
		"video_codec_ctx->thread_count", video_codec_ctx->thread_count);

}

// write all queued frames' audio to the video file
void FFmpegWriter::write_audio_packets(bool is_final, std::shared_ptr<openshot::Frame> frame) {
	if (!frame && !is_final)
		return;

	// Init audio buffers / variables
	int total_frame_samples = 0;
	int frame_position = 0;
	int channels_in_frame = 0;
	int sample_rate_in_frame = 0;
	int samples_in_frame = 0;
	ChannelLayout channel_layout_in_frame = LAYOUT_MONO; // default channel layout

	// Create a new array (to hold all S16 audio samples, for the current queued frames
	unsigned int all_queued_samples_size = sizeof(int16_t) * AVCODEC_MAX_AUDIO_FRAME_SIZE;
	int16_t *all_queued_samples = (int16_t *) av_malloc(all_queued_samples_size);
	int16_t *all_resampled_samples = NULL;
	int16_t *final_samples_planar = NULL;
	int16_t *final_samples = NULL;

	// Get audio sample array
	float *frame_samples_float = NULL;

	// Get the audio details from this frame
	if (frame) {
		sample_rate_in_frame = frame->SampleRate();
		samples_in_frame = frame->GetAudioSamplesCount();
		channels_in_frame = frame->GetAudioChannelsCount();
		channel_layout_in_frame = frame->ChannelsLayout();

		// Get samples interleaved together (c1 c2 c1 c2 c1 c2)
		frame_samples_float = frame->GetInterleavedAudioSamples(&samples_in_frame);
	}

	// Calculate total samples
	total_frame_samples = samples_in_frame * channels_in_frame;

	// Translate audio sample values back to 16 bit integers with saturation
	const int16_t max16 = 32767;
	const int16_t min16 = -32768;
	for (int s = 0; s < total_frame_samples; s++, frame_position++) {
		float valF = frame_samples_float[s] * (1 << 15);
		int16_t conv;
		if (valF > max16) {
			conv = max16;
		} else if (valF < min16) {
			conv = min16;
		} else {
			conv = int(valF + 32768.5) - 32768; // +0.5 is for rounding
		}

		// Copy into buffer
		all_queued_samples[frame_position] = conv;
	}

	// Deallocate float array
	delete[] frame_samples_float;


	// Update total samples (since we've combined all queued frames)
	total_frame_samples = frame_position;
	int remaining_frame_samples = total_frame_samples;
	int samples_position = 0;


	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::write_audio_packets",
		"is_final", is_final,
		"total_frame_samples", total_frame_samples,
		"channel_layout_in_frame", channel_layout_in_frame,
		"channels_in_frame", channels_in_frame,
		"samples_in_frame", samples_in_frame,
		"LAYOUT_MONO", LAYOUT_MONO);

	// Keep track of the original sample format
	AVSampleFormat output_sample_fmt = audio_codec_ctx->sample_fmt;

	AVFrame *audio_frame = NULL;
	if (!is_final) {
		// Create input frame (and allocate arrays)
		audio_frame = AV_ALLOCATE_FRAME();
		AV_RESET_FRAME(audio_frame);
		audio_frame->nb_samples = total_frame_samples / channels_in_frame;

		// Fill input frame with sample data
		int error_code = avcodec_fill_audio_frame(audio_frame, channels_in_frame, AV_SAMPLE_FMT_S16, (uint8_t *) all_queued_samples, all_queued_samples_size, 0);
		if (error_code < 0) {
			ZmqLogger::Instance()->AppendDebugMethod(
				"FFmpegWriter::write_audio_packets ERROR ["
					+ av_err2string(error_code) + "]",
				"error_code", error_code);
		}

		// Do not convert audio to planar format (yet). We need to keep everything interleaved at this point.
		switch (audio_codec_ctx->sample_fmt) {
			case AV_SAMPLE_FMT_FLTP: {
				output_sample_fmt = AV_SAMPLE_FMT_FLT;
				break;
			}
			case AV_SAMPLE_FMT_S32P: {
				output_sample_fmt = AV_SAMPLE_FMT_S32;
				break;
			}
			case AV_SAMPLE_FMT_S16P: {
				output_sample_fmt = AV_SAMPLE_FMT_S16;
				break;
			}
			case AV_SAMPLE_FMT_U8P: {
				output_sample_fmt = AV_SAMPLE_FMT_U8;
				break;
			}
			default: {
				// This is only here to silence unused-enum warnings
				break;
			}
		}

		// Update total samples & input frame size (due to bigger or smaller data types)
		total_frame_samples *= (float(info.sample_rate) / sample_rate_in_frame); // adjust for different byte sizes
		total_frame_samples *= (float(info.channels) / channels_in_frame); // adjust for different # of channels

		// Create output frame (and allocate arrays)
		AVFrame *audio_converted = AV_ALLOCATE_FRAME();
		AV_RESET_FRAME(audio_converted);
		audio_converted->nb_samples = total_frame_samples / channels_in_frame;
		av_samples_alloc(audio_converted->data, audio_converted->linesize, info.channels, audio_converted->nb_samples, output_sample_fmt, 0);

		ZmqLogger::Instance()->AppendDebugMethod(
			"FFmpegWriter::write_audio_packets (1st resampling)",
			"in_sample_fmt", AV_SAMPLE_FMT_S16,
			"out_sample_fmt", output_sample_fmt,
			"in_sample_rate", sample_rate_in_frame,
			"out_sample_rate", info.sample_rate,
			"in_channels", channels_in_frame,
			"out_channels", info.channels);

		// setup resample context
		if (!avr) {
			avr = SWR_ALLOC();
#if HAVE_CH_LAYOUT
			AVChannelLayout in_chlayout;
			AVChannelLayout out_chlayout;
			av_channel_layout_from_mask(&in_chlayout, channel_layout_in_frame);
			av_channel_layout_from_mask(&out_chlayout, info.channel_layout);
			av_opt_set_chlayout(avr, "in_chlayout", &in_chlayout, 0);
			av_opt_set_chlayout(avr, "out_chlayout", &out_chlayout, 0);
#else
			av_opt_set_int(avr, "in_channel_layout", channel_layout_in_frame, 0);
			av_opt_set_int(avr, "out_channel_layout", info.channel_layout, 0);
			av_opt_set_int(avr, "in_channels", channels_in_frame, 0);
			av_opt_set_int(avr, "out_channels", info.channels, 0);
#endif
			av_opt_set_int(avr, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
			av_opt_set_int(avr, "out_sample_fmt", output_sample_fmt, 0); // planar not allowed here
			av_opt_set_int(avr, "in_sample_rate", sample_rate_in_frame, 0);
			av_opt_set_int(avr, "out_sample_rate", info.sample_rate, 0);
			SWR_INIT(avr);
		}
		// Convert audio samples
		int nb_samples = SWR_CONVERT(
			avr,	// audio resample context
			audio_converted->data,		   // output data pointers
			audio_converted->linesize[0],	// output plane size, in bytes. (0 if unknown)
			audio_converted->nb_samples,	 // maximum number of samples that the output buffer can hold
			audio_frame->data,			   // input data pointers
			audio_frame->linesize[0],		// input plane size, in bytes (0 if unknown)
			audio_frame->nb_samples		// number of input samples to convert
		);

		// Set remaining samples
		remaining_frame_samples = total_frame_samples;

		// Create a new array (to hold all resampled S16 audio samples)
		all_resampled_samples = (int16_t *) av_malloc(
			sizeof(int16_t) * nb_samples * info.channels
			* (av_get_bytes_per_sample(output_sample_fmt) /
			   av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) )
		);

		// Copy audio samples over original samples
		memcpy(all_resampled_samples, audio_converted->data[0],
			   static_cast<size_t>(nb_samples)
				   * info.channels
				   * av_get_bytes_per_sample(output_sample_fmt));

		// Remove converted audio
		av_freep(&(audio_frame->data[0]));
		AV_FREE_FRAME(&audio_frame);
		av_freep(&audio_converted->data[0]);
		AV_FREE_FRAME(&audio_converted);
		all_queued_samples = NULL; // this array cleared with above call

		ZmqLogger::Instance()->AppendDebugMethod(
			"FFmpegWriter::write_audio_packets (Successfully completed 1st resampling)",
			"nb_samples", nb_samples,
			"remaining_frame_samples", remaining_frame_samples);
	}

	// Loop until no more samples
	while (remaining_frame_samples > 0 || is_final) {
		// Get remaining samples needed for this packet
		int remaining_packet_samples = (audio_input_frame_size * info.channels) - audio_input_position;

		// Determine how many samples we need
		int diff = 0;
		if (remaining_frame_samples >= remaining_packet_samples) {
			diff = remaining_packet_samples;
		} else {
			diff = remaining_frame_samples;
		}

		// Copy frame samples into the packet samples array
		if (!is_final)
			//TODO: Make this more sane
			memcpy(
				samples + (audio_input_position
					* (av_get_bytes_per_sample(output_sample_fmt) /
					   av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) )
				),
				all_resampled_samples + samples_position,
				static_cast<size_t>(diff)
					* av_get_bytes_per_sample(output_sample_fmt)
			);

		// Increment counters
		audio_input_position += diff;
		samples_position += diff * (av_get_bytes_per_sample(output_sample_fmt) / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16));
		remaining_frame_samples -= diff;

		// Do we have enough samples to proceed?
		if (audio_input_position < (audio_input_frame_size * info.channels) && !is_final)
			// Not enough samples to encode... so wait until the next frame
			break;

		// Convert to planar (if needed by audio codec)
		AVFrame *frame_final = AV_ALLOCATE_FRAME();
		AV_RESET_FRAME(frame_final);
		if (av_sample_fmt_is_planar(audio_codec_ctx->sample_fmt)) {
			ZmqLogger::Instance()->AppendDebugMethod(
				"FFmpegWriter::write_audio_packets (2nd resampling for Planar formats)",
				"in_sample_fmt", output_sample_fmt,
				"out_sample_fmt", audio_codec_ctx->sample_fmt,
				"in_sample_rate", info.sample_rate,
				"out_sample_rate", info.sample_rate,
				"in_channels", info.channels,
				"out_channels", info.channels
			);

			// setup resample context
			if (!avr_planar) {
				avr_planar = SWR_ALLOC();
#if HAVE_CH_LAYOUT
				AVChannelLayout layout;
				av_channel_layout_from_mask(&layout, info.channel_layout);
				av_opt_set_chlayout(avr_planar, "in_chlayout", &layout, 0);
				av_opt_set_chlayout(avr_planar, "out_chlayout", &layout, 0);
#else
				av_opt_set_int(avr_planar, "in_channel_layout", info.channel_layout, 0);
				av_opt_set_int(avr_planar, "out_channel_layout", info.channel_layout, 0);
				av_opt_set_int(avr_planar, "in_channels", info.channels, 0);
				av_opt_set_int(avr_planar, "out_channels", info.channels, 0);
#endif
				av_opt_set_int(avr_planar, "in_sample_fmt", output_sample_fmt, 0);
				av_opt_set_int(avr_planar, "out_sample_fmt", audio_codec_ctx->sample_fmt, 0); // planar not allowed here
				av_opt_set_int(avr_planar, "in_sample_rate", info.sample_rate, 0);
				av_opt_set_int(avr_planar, "out_sample_rate", info.sample_rate, 0);
				SWR_INIT(avr_planar);
			}

			// Create input frame (and allocate arrays)
			audio_frame = AV_ALLOCATE_FRAME();
			AV_RESET_FRAME(audio_frame);
			audio_frame->nb_samples = audio_input_position / info.channels;

			// Create a new array
			final_samples_planar = (int16_t *) av_malloc(
				sizeof(int16_t) * audio_frame->nb_samples * info.channels
				* (av_get_bytes_per_sample(output_sample_fmt) /
				   av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) )
			);

			// Copy audio into buffer for frame
			memcpy(final_samples_planar, samples,
				   static_cast<size_t>(audio_frame->nb_samples)
					   * info.channels
					   * av_get_bytes_per_sample(output_sample_fmt));

			// Fill input frame with sample data
			avcodec_fill_audio_frame(audio_frame, info.channels, output_sample_fmt,
				(uint8_t *) final_samples_planar, audio_encoder_buffer_size, 0);

			// Create output frame (and allocate arrays)
			frame_final->nb_samples = audio_input_frame_size;
#if HAVE_CH_LAYOUT
			av_channel_layout_from_mask(&frame_final->ch_layout, info.channel_layout);
#else
			frame_final->channels = info.channels;
			frame_final->channel_layout = info.channel_layout;
#endif
			frame_final->format = audio_codec_ctx->sample_fmt;
			av_samples_alloc(frame_final->data, frame_final->linesize, info.channels,
				frame_final->nb_samples, audio_codec_ctx->sample_fmt, 0);

			// Convert audio samples
			int nb_samples = SWR_CONVERT(
				avr_planar,				  // audio resample context
				frame_final->data,		   // output data pointers
				frame_final->linesize[0],	// output plane size, in bytes. (0 if unknown)
				frame_final->nb_samples,	 // maximum number of samples that the output buffer can hold
				audio_frame->data,		   // input data pointers
				audio_frame->linesize[0],	// input plane size, in bytes (0 if unknown)
				audio_frame->nb_samples	  // number of input samples to convert
			);

			// Copy audio samples over original samples
			const auto copy_length = static_cast<size_t>(nb_samples)
				* av_get_bytes_per_sample(audio_codec_ctx->sample_fmt)
				* info.channels;

			if (nb_samples > 0)
				memcpy(samples, frame_final->data[0], copy_length);

			// deallocate AVFrame
			av_freep(&(audio_frame->data[0]));
			AV_FREE_FRAME(&audio_frame);
			all_queued_samples = NULL; // this array cleared with above call

			ZmqLogger::Instance()->AppendDebugMethod(
				"FFmpegWriter::write_audio_packets (Successfully completed 2nd resampling for Planar formats)",
				"nb_samples", nb_samples);

		} else {
			// Create a new array
			const auto buf_size = static_cast<size_t>(audio_input_position)
				* (av_get_bytes_per_sample(audio_codec_ctx->sample_fmt) /
				   av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)
				);
			final_samples = reinterpret_cast<int16_t*>(
					av_malloc(sizeof(int16_t) * buf_size));

			// Copy audio into buffer for frame
			memcpy(final_samples, samples,
				audio_input_position * av_get_bytes_per_sample(audio_codec_ctx->sample_fmt));

			// Init the nb_samples property
			frame_final->nb_samples = audio_input_frame_size;

			// Fill the final_frame AVFrame with audio (non planar)
#if HAVE_CH_LAYOUT
			int nb_channels = audio_codec_ctx->ch_layout.nb_channels;
#else
			int nb_channels = audio_codec_ctx->channels;
#endif
			avcodec_fill_audio_frame(frame_final, nb_channels,
				audio_codec_ctx->sample_fmt, (uint8_t *) final_samples,
				audio_encoder_buffer_size, 0);
		}

		// Set the AVFrame's PTS
		frame_final->pts = audio_timestamp;

		// Init the packet
#if IS_FFMPEG_3_2
		AVPacket* pkt = av_packet_alloc();
#else
		AVPacket* pkt;
		av_init_packet(pkt);
#endif
		pkt->data = audio_encoder_buffer;
		pkt->size = audio_encoder_buffer_size;

		// Set the packet's PTS prior to encoding
		pkt->pts = pkt->dts = audio_timestamp;

		/* encode the audio samples */
		int got_packet_ptr = 0;

#if IS_FFMPEG_3_2
		// Encode audio (latest version of FFmpeg)
		int error_code;
		int ret = 0;
		int frame_finished = 0;
		error_code = ret =  avcodec_send_frame(audio_codec_ctx, frame_final);
		if (ret < 0 && ret !=  AVERROR(EINVAL) && ret != AVERROR_EOF) {
			avcodec_send_frame(audio_codec_ctx, NULL);
		}
		else {
			if (ret >= 0)
				pkt->size = 0;
			ret =  avcodec_receive_packet(audio_codec_ctx, pkt);
			if (ret >= 0)
				frame_finished = 1;
			if(ret == AVERROR(EINVAL) || ret == AVERROR_EOF) {
				avcodec_flush_buffers(audio_codec_ctx);
				ret = 0;
			}
			if (ret >= 0) {
				ret = frame_finished;
			}
		}
		if (!pkt->data && !frame_finished)
		{
			ret = -1;
		}
		got_packet_ptr = ret;
#else
		// Encode audio (older versions of FFmpeg)
		int error_code = avcodec_encode_audio2(audio_codec_ctx, pkt, frame_final, &got_packet_ptr);
#endif
		/* if zero size, it means the image was buffered */
		if (error_code == 0 && got_packet_ptr) {

			// Since the PTS can change during encoding, set the value again.  This seems like a huge hack,
			// but it fixes lots of PTS related issues when I do this.
			pkt->pts = pkt->dts = audio_timestamp;

			// Scale the PTS to the audio stream timebase (which is sometimes different than the codec's timebase)
			av_packet_rescale_ts(pkt, audio_codec_ctx->time_base, audio_st->time_base);

			// set stream
			pkt->stream_index = audio_st->index;
			pkt->flags |= AV_PKT_FLAG_KEY;

			/* write the compressed frame in the media file */
			error_code = av_interleaved_write_frame(oc, pkt);
		}

		if (error_code < 0) {
			ZmqLogger::Instance()->AppendDebugMethod(
				"FFmpegWriter::write_audio_packets ERROR ["
					+ av_err2string(error_code) + "]",
				"error_code", error_code);
		}

		// Increment PTS (no pkt.duration, so calculate with maths)
		audio_timestamp += FFMIN(audio_input_frame_size, audio_input_position);

		// deallocate AVFrame
		av_freep(&(frame_final->data[0]));
		AV_FREE_FRAME(&frame_final);

		// deallocate memory for packet
		AV_FREE_PACKET(pkt);

		// Reset position
		audio_input_position = 0;
		is_final = false;
	}

	// Delete arrays (if needed)
	if (all_resampled_samples) {
		av_freep(&all_resampled_samples);
		all_resampled_samples = NULL;
	}
	if (all_queued_samples) {
		av_freep(&all_queued_samples);
		all_queued_samples = NULL;
	}
}

// Allocate an AVFrame object
AVFrame *FFmpegWriter::allocate_avframe(PixelFormat pix_fmt, int width, int height, int *buffer_size, uint8_t *new_buffer) {
	// Create an RGB AVFrame
	AVFrame *new_av_frame = NULL;

	// Allocate an AVFrame structure
	new_av_frame = AV_ALLOCATE_FRAME();
	if (new_av_frame == NULL)
		throw OutOfMemory("Could not allocate AVFrame", path);

	// Determine required buffer size and allocate buffer
	*buffer_size = AV_GET_IMAGE_SIZE(pix_fmt, width, height);

	// Create buffer (if not provided)
	if (!new_buffer) {
		// New Buffer
		new_buffer = (uint8_t *) av_malloc(*buffer_size * sizeof(uint8_t));
		// Attach buffer to AVFrame
		AV_COPY_PICTURE_DATA(new_av_frame, new_buffer, pix_fmt, width, height);
		new_av_frame->width = width;
		new_av_frame->height = height;
		new_av_frame->format = pix_fmt;
	}

	// return AVFrame
	return new_av_frame;
}

// process video frame
void FFmpegWriter::process_video_packet(std::shared_ptr<Frame> frame) {
	// Source dimensions (RGBA)
	int src_w = frame->GetWidth();
	int src_h = frame->GetHeight();

	// Skip empty frames (1×1)
	if (src_w == 1 && src_h == 1)
		return;

	// Point persistent_src_frame->data to RGBA pixels
	const uchar* pixels = frame->GetPixels();
	if (!persistent_src_frame) {
		persistent_src_frame = av_frame_alloc();
		if (!persistent_src_frame)
			throw OutOfMemory("Could not allocate persistent_src_frame", path);
		persistent_src_frame->format      = AV_PIX_FMT_RGBA;
		persistent_src_frame->width       = src_w;
		persistent_src_frame->height      = src_h;
		persistent_src_frame->linesize[0] = src_w * 4;
	}
	persistent_src_frame->data[0] = const_cast<uint8_t*>(
		reinterpret_cast<const uint8_t*>(pixels)
	);

	// Prepare persistent_dst_frame + buffer on first use
	if (!persistent_dst_frame) {
		persistent_dst_frame = av_frame_alloc();
		if (!persistent_dst_frame)
			throw OutOfMemory("Could not allocate persistent_dst_frame", path);

		// Decide destination pixel format: NV12 if HW accel is on, else encoder’s pix_fmt
		AVPixelFormat dst_fmt = video_codec_ctx->pix_fmt;
#if USE_HW_ACCEL
		if (hw_en_on && hw_en_supported) {
			dst_fmt = AV_PIX_FMT_NV12;
		}
#endif
		persistent_dst_frame->format = dst_fmt;
		persistent_dst_frame->width  = info.width;
		persistent_dst_frame->height = info.height;

		persistent_dst_size = av_image_get_buffer_size(
			dst_fmt, info.width, info.height, 1
		);
		if (persistent_dst_size < 0)
			throw ErrorEncodingVideo("Invalid destination image size", -1);

		persistent_dst_buffer = static_cast<uint8_t*>(
			av_malloc(persistent_dst_size)
		);
		if (!persistent_dst_buffer)
			throw OutOfMemory("Could not allocate persistent_dst_buffer", path);

		av_image_fill_arrays(
			persistent_dst_frame->data,
			persistent_dst_frame->linesize,
			persistent_dst_buffer,
			dst_fmt,
			info.width,
			info.height,
			1
		);
	}

	// Initialize SwsContext (RGBA → dst_fmt) on first use
	if (!img_convert_ctx) {
		int flags = SWS_FAST_BILINEAR;
		if (openshot::Settings::Instance()->HIGH_QUALITY_SCALING) {
			flags = SWS_BICUBIC;
		}
		AVPixelFormat dst_fmt = video_codec_ctx->pix_fmt;
#if USE_HW_ACCEL
		if (hw_en_on && hw_en_supported) {
			dst_fmt = AV_PIX_FMT_NV12;
		}
#endif
		img_convert_ctx = sws_getContext(
			src_w, src_h, AV_PIX_FMT_RGBA,
			info.width, info.height, dst_fmt,
			flags, NULL, NULL, NULL
		);
		if (!img_convert_ctx)
			throw ErrorEncodingVideo("Could not initialize sws context", -1);
	}

	// Scale RGBA → dst_fmt into persistent_dst_buffer
	sws_scale(
		img_convert_ctx,
		persistent_src_frame->data,
		persistent_src_frame->linesize,
		0, src_h,
		persistent_dst_frame->data,
		persistent_dst_frame->linesize
	);

	// Allocate a new AVFrame + buffer, then copy scaled data into it
	int bytes_final = 0;
	AVPixelFormat dst_fmt = video_codec_ctx->pix_fmt;
#if USE_HW_ACCEL
	if (hw_en_on && hw_en_supported) {
		dst_fmt = AV_PIX_FMT_NV12;
	}
#endif

	AVFrame* new_frame = allocate_avframe(
		dst_fmt,
		info.width,
		info.height,
		&bytes_final,
		nullptr
	);
	if (!new_frame)
		throw OutOfMemory("Could not allocate new_frame via allocate_avframe", path);

	// Copy persistent_dst_buffer → new_frame buffer
	memcpy(
		new_frame->data[0],
		persistent_dst_buffer,
		static_cast<size_t>(bytes_final)
	);

	// Queue the deep‐copied frame for encoding
	add_avframe(frame, new_frame);
}

// write video frame
bool FFmpegWriter::write_video_packet(std::shared_ptr<Frame> frame, AVFrame *frame_final) {
#if (LIBAVFORMAT_VERSION_MAJOR >= 58)
	// FFmpeg 4.0+
	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::write_video_packet",
		"frame->number", frame->number,
		"oc->oformat->flags", oc->oformat->flags);

	if (AV_GET_CODEC_TYPE(video_st) == AVMEDIA_TYPE_VIDEO && AV_FIND_DECODER_CODEC_ID(video_st) == AV_CODEC_ID_RAWVIDEO) {
#else
	// TODO: Should we have moved away from oc->oformat->flags / AVFMT_RAWPICTURE
	// on ffmpeg < 4.0 as well?
	// Does AV_CODEC_ID_RAWVIDEO not work in ffmpeg 3.x?
	ZmqLogger::Instance()->AppendDebugMethod(
		"FFmpegWriter::write_video_packet",
		"frame->number", frame->number,
		"oc->oformat->flags & AVFMT_RAWPICTURE", oc->oformat->flags & AVFMT_RAWPICTURE);

	if (oc->oformat->flags & AVFMT_RAWPICTURE) {
#endif
		// Raw video case.
#if IS_FFMPEG_3_2
		AVPacket* pkt = av_packet_alloc();
#else
		AVPacket* pkt;
		av_init_packet(pkt);
#endif

	av_packet_from_data(
			pkt, frame_final->data[0],
			frame_final->linesize[0] * frame_final->height);

		pkt->flags |= AV_PKT_FLAG_KEY;
		pkt->stream_index = video_st->index;

		// Set PTS (in frames and scaled to the codec's timebase)
		pkt->pts = video_timestamp;

		/* write the compressed frame in the media file */
		int error_code = av_interleaved_write_frame(oc, pkt);
		if (error_code < 0) {
			ZmqLogger::Instance()->AppendDebugMethod(
				"FFmpegWriter::write_video_packet ERROR ["
					+ av_err2string(error_code) + "]",
				"error_code", error_code);
			return false;
		}

		// Deallocate packet
		AV_FREE_PACKET(pkt);

	} else
	{

#if IS_FFMPEG_3_2
		AVPacket* pkt = av_packet_alloc();
#else
		AVPacket* pkt;
		av_init_packet(pkt);
#endif
		pkt->data = NULL;
		pkt->size = 0;
		pkt->pts = pkt->dts = AV_NOPTS_VALUE;

		// Assign the initial AVFrame PTS from the frame counter
		frame_final->pts = video_timestamp;
#if USE_HW_ACCEL
		if (hw_en_on && hw_en_supported) {
			if (!(hw_frame = av_frame_alloc())) {
				std::clog << "Error code: av_hwframe_alloc\n";
			}
			if (av_hwframe_get_buffer(video_codec_ctx->hw_frames_ctx, hw_frame, 0) < 0) {
				std::clog << "Error code: av_hwframe_get_buffer\n";
			}
			if (!hw_frame->hw_frames_ctx) {
				std::clog << "Error hw_frames_ctx.\n";
			}
			hw_frame->format = AV_PIX_FMT_NV12;
			if ( av_hwframe_transfer_data(hw_frame, frame_final, 0) < 0) {
				std::clog << "Error while transferring frame data to surface.\n";
			}
			av_frame_copy_props(hw_frame, frame_final);
		}
#endif // USE_HW_ACCEL
		/* encode the image */
		int got_packet_ptr = 0;
		int error_code = 0;
#if IS_FFMPEG_3_2
		// Write video packet
		int ret;

	#if USE_HW_ACCEL
		if (hw_en_on && hw_en_supported) {
			ret = avcodec_send_frame(video_codec_ctx, hw_frame);  //hw_frame!!!
		} else
	#endif // USE_HW_ACCEL
		{
			ret = avcodec_send_frame(video_codec_ctx, frame_final);
		}
		error_code = ret;
		if (ret < 0 ) {
			ZmqLogger::Instance()->AppendDebugMethod(
				"FFmpegWriter::write_video_packet (Frame not sent)");
			if (ret == AVERROR(EAGAIN) ) {
				std::clog << "Frame EAGAIN\n";
			}
			if (ret == AVERROR_EOF ) {
				std::clog << "Frame AVERROR_EOF\n";
			}
			avcodec_send_frame(video_codec_ctx, NULL);
		}
		else {
			while (ret >= 0) {
				ret = avcodec_receive_packet(video_codec_ctx, pkt);

				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					got_packet_ptr = 0;
					break;
				}
				if (ret == 0) {
					got_packet_ptr = 1;
					break;
				}
			}
		}
#else
		// Write video packet (older than FFmpeg 3.2)
		error_code = avcodec_encode_video2(video_codec_ctx, pkt, frame_final, &got_packet_ptr);
		if (error_code != 0) {
			ZmqLogger::Instance()->AppendDebugMethod(
				"FFmpegWriter::write_video_packet ERROR ["
					+ av_err2string(error_code) + "]",
				"error_code", error_code);
		}
		if (got_packet_ptr == 0) {
			ZmqLogger::Instance()->AppendDebugMethod(
				"FFmpegWriter::write_video_packet (Frame gotpacket error)");
		}
#endif // IS_FFMPEG_3_2

		/* if zero size, it means the image was buffered */
		if (error_code == 0 && got_packet_ptr) {
			// set the timestamp
			av_packet_rescale_ts(pkt, video_codec_ctx->time_base, video_st->time_base);
			pkt->stream_index = video_st->index;

			/* write the compressed frame in the media file */
			int result = av_interleaved_write_frame(oc, pkt);
			if (result < 0) {
				ZmqLogger::Instance()->AppendDebugMethod(
					"FFmpegWriter::write_video_packet ERROR ["
						+ av_err2string(result) + "]",
					"result", result);
				return false;
			}
		}

		// Deallocate packet
		AV_FREE_PACKET(pkt);
#if USE_HW_ACCEL
		if (hw_en_on && hw_en_supported) {
			if (hw_frame) {
				av_frame_free(&hw_frame);
				hw_frame = NULL;
			}
		}
#endif // USE_HW_ACCEL
	}

	// Increment PTS (in frames and scaled to the codec's timebase)
	video_timestamp += av_rescale_q(1, av_make_q(info.fps.den, info.fps.num), video_codec_ctx->time_base);

	// Success
	return true;
}

// Output the ffmpeg info about this format, streams, and codecs (i.e. dump format)
void FFmpegWriter::OutputStreamInfo() {
	// output debug info
	av_dump_format(oc, 0, path.c_str(), 1);
}

// Set audio resample options
void FFmpegWriter::ResampleAudio(int sample_rate, int channels) {
	original_sample_rate = sample_rate;
	original_channels = channels;
}

// In FFmpegWriter.cpp
void FFmpegWriter::AddSphericalMetadata(const std::string& projection, float yaw_deg, float pitch_deg, float roll_deg) {
	if (!oc) return;
	if (!info.has_video || !video_st) return;

	// Allow movenc.c to write out the sv3d atom
	oc->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 0, 0)
	// Map the projection name to the enum (defaults to equirectangular)
	int proj = av_spherical_from_name(projection.c_str());
	if (proj < 0)
		proj = AV_SPHERICAL_EQUIRECTANGULAR;

	// Allocate the side‐data structure
	size_t sd_size = 0;
	AVSphericalMapping* map = av_spherical_alloc(&sd_size);
	if (!map) return;

	// Populate it
	map->projection = static_cast<AVSphericalProjection>(proj);
	// yaw/pitch/roll are 16.16 fixed point
	map->yaw   = static_cast<int32_t>(yaw_deg   * (1 << 16));
	map->pitch = static_cast<int32_t>(pitch_deg * (1 << 16));
	map->roll  = static_cast<int32_t>(roll_deg  * (1 << 16));

	av_stream_add_side_data(video_st, AV_PKT_DATA_SPHERICAL, reinterpret_cast<uint8_t*>(map), sd_size);
#endif
}
