/**
 * @file
 * @brief Source file for FFmpegReader class
 * @author Jonathan Thomas <jonathan@openshot.org>, Fabrice Bellard
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC, Fabrice Bellard
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * This file is originally based on the Libavformat API example, and then modified
 * by the libopenshot project.
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

#include "../include/FFmpegReader.h"

#define ENABLE_VAAPI 0

#if HAVE_HW_ACCEL
#pragma message "You are compiling with experimental hardware decode"
#else
#pragma message "You are compiling only with software decode"
#endif

#if HAVE_HW_ACCEL
#define MAX_SUPPORTED_WIDTH 1950
#define MAX_SUPPORTED_HEIGHT 1100

#if ENABLE_VAAPI
#include "libavutil/hwcontext_vaapi.h"

typedef struct VAAPIDecodeContext {
	 VAProfile             va_profile;
	 VAEntrypoint          va_entrypoint;
	 VAConfigID            va_config;
	 VAContextID           va_context;

#if FF_API_STRUCT_VAAPI_CONTEXT
	 // FF_DISABLE_DEPRECATION_WARNINGS
		 int                   have_old_context;
		 struct vaapi_context *old_context;
		 AVBufferRef          *device_ref;
	 // FF_ENABLE_DEPRECATION_WARNINGS
#endif

	 AVHWDeviceContext    *device;
	 AVVAAPIDeviceContext *hwctx;

	 AVHWFramesContext    *frames;
	 AVVAAPIFramesContext *hwfc;

	 enum AVPixelFormat    surface_format;
	 int                   surface_count;
 } VAAPIDecodeContext;
#endif // ENABLE_VAAPI
#endif // HAVE_HW_ACCEL


using namespace openshot;

int hw_de_on = 0;
#if HAVE_HW_ACCEL
	AVPixelFormat hw_de_av_pix_fmt_global = AV_PIX_FMT_NONE;
	AVHWDeviceType hw_de_av_device_type_global = AV_HWDEVICE_TYPE_NONE;
#endif

FFmpegReader::FFmpegReader(std::string path)
		: last_frame(0), is_seeking(0), seeking_pts(0), seeking_frame(0), seek_count(0),
		  audio_pts_offset(99999), video_pts_offset(99999), path(path), is_video_seek(true), check_interlace(false),
		  check_fps(false), enable_seek(true), is_open(false), seek_audio_frame_found(0), seek_video_frame_found(0),
		  prev_samples(0), prev_pts(0), pts_total(0), pts_counter(0), is_duration_known(false), largest_frame_processed(0),
		  current_video_frame(0), has_missing_frames(false), num_packets_since_video_frame(0), num_checks_since_final(0),
		  packet(NULL) {

	// Initialize FFMpeg, and register all formats and codecs
	AV_REGISTER_ALL
	AVCODEC_REGISTER_ALL

	// Init cache
	working_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * info.fps.ToDouble() * 2, info.width, info.height, info.sample_rate, info.channels);
	missing_frames.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);
	final_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);

	// Open and Close the reader, to populate its attributes (such as height, width, etc...)
	Open();
	Close();
}

FFmpegReader::FFmpegReader(std::string path, bool inspect_reader)
		: last_frame(0), is_seeking(0), seeking_pts(0), seeking_frame(0), seek_count(0),
		  audio_pts_offset(99999), video_pts_offset(99999), path(path), is_video_seek(true), check_interlace(false),
		  check_fps(false), enable_seek(true), is_open(false), seek_audio_frame_found(0), seek_video_frame_found(0),
		  prev_samples(0), prev_pts(0), pts_total(0), pts_counter(0), is_duration_known(false), largest_frame_processed(0),
		  current_video_frame(0), has_missing_frames(false), num_packets_since_video_frame(0), num_checks_since_final(0),
		  packet(NULL) {

	// Initialize FFMpeg, and register all formats and codecs
	AV_REGISTER_ALL
	AVCODEC_REGISTER_ALL

	// Init cache
	working_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * info.fps.ToDouble() * 2, info.width, info.height, info.sample_rate, info.channels);
	missing_frames.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);
	final_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);

	// Open and Close the reader, to populate its attributes (such as height, width, etc...)
	if (inspect_reader) {
		Open();
		Close();
	}
}

FFmpegReader::~FFmpegReader() {
	if (is_open)
		// Auto close reader if not already done
		Close();
}

// This struct holds the associated video frame and starting sample # for an audio packet.
bool AudioLocation::is_near(AudioLocation location, int samples_per_frame, int64_t amount) {
	// Is frame even close to this one?
	if (abs(location.frame - frame) >= 2)
		// This is too far away to be considered
		return false;

	// Note that samples_per_frame can vary slightly frame to frame when the
	// audio sampling rate is not an integer multiple of the video fps.
	int64_t diff = samples_per_frame * (location.frame - frame) + location.sample_start - sample_start;
	if (abs(diff) <= amount)
		// close
		return true;

	// not close
	return false;
}

#if HAVE_HW_ACCEL

// Get hardware pix format
static enum AVPixelFormat get_hw_dec_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
	const enum AVPixelFormat *p;

	for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
		switch (*p) {
#if defined(__linux__)
			// Linux pix formats
			case AV_PIX_FMT_VAAPI:
				hw_de_av_pix_fmt_global = AV_PIX_FMT_VAAPI;
				hw_de_av_device_type_global = AV_HWDEVICE_TYPE_VAAPI;
				return *p;
				break;
			case AV_PIX_FMT_VDPAU:
				hw_de_av_pix_fmt_global = AV_PIX_FMT_VDPAU;
				hw_de_av_device_type_global = AV_HWDEVICE_TYPE_VDPAU;
				return *p;
				break;
#endif
#if defined(_WIN32)
			// Windows pix formats
			case AV_PIX_FMT_DXVA2_VLD:
				hw_de_av_pix_fmt_global = AV_PIX_FMT_DXVA2_VLD;
				hw_de_av_device_type_global = AV_HWDEVICE_TYPE_DXVA2;
				return *p;
				break;
			case AV_PIX_FMT_D3D11:
				hw_de_av_pix_fmt_global = AV_PIX_FMT_D3D11;
				hw_de_av_device_type_global = AV_HWDEVICE_TYPE_D3D11VA;
				return *p;
				break;
#endif
#if defined(__APPLE__)
			// Apple pix formats
			case AV_PIX_FMT_VIDEOTOOLBOX:
				hw_de_av_pix_fmt_global = AV_PIX_FMT_VIDEOTOOLBOX;
				hw_de_av_device_type_global = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
				return *p;
				break;
#endif
				// Cross-platform pix formats
			case AV_PIX_FMT_CUDA:
				hw_de_av_pix_fmt_global = AV_PIX_FMT_CUDA;
				hw_de_av_device_type_global = AV_HWDEVICE_TYPE_CUDA;
				return *p;
				break;
			case AV_PIX_FMT_QSV:
				hw_de_av_pix_fmt_global = AV_PIX_FMT_QSV;
				hw_de_av_device_type_global = AV_HWDEVICE_TYPE_QSV;
				return *p;
				break;
			default:
				// This is only here to silence unused-enum warnings
				break;
		}
	}
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::get_hw_dec_format (Unable to decode this file using hardware decode)");
	return AV_PIX_FMT_NONE;
}

int FFmpegReader::IsHardwareDecodeSupported(int codecid)
{
	int ret;
	switch (codecid) {
		case AV_CODEC_ID_H264:
		case AV_CODEC_ID_MPEG2VIDEO:
		case AV_CODEC_ID_VC1:
		case AV_CODEC_ID_WMV1:
		case AV_CODEC_ID_WMV2:
		case AV_CODEC_ID_WMV3:
			ret = 1;
			break;
		default :
			ret = 0;
			break;
	}
	return ret;
}
#endif // HAVE_HW_ACCEL

void FFmpegReader::Open() {
	// Open reader if not already open
	if (!is_open) {
		// Initialize format context
		pFormatCtx = NULL;
		{
			hw_de_on = (openshot::Settings::Instance()->HARDWARE_DECODER == 0 ? 0 : 1);
		}

		// Open video file
		if (avformat_open_input(&pFormatCtx, path.c_str(), NULL, NULL) != 0)
			throw InvalidFile("File could not be opened.", path);

		// Retrieve stream information
		if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
			throw NoStreamsFound("No streams found in file.", path);

		videoStream = -1;
		audioStream = -1;
		// Loop through each stream, and identify the video and audio stream index
		for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
			// Is this a video stream?
			if (AV_GET_CODEC_TYPE(pFormatCtx->streams[i]) == AVMEDIA_TYPE_VIDEO && videoStream < 0) {
				videoStream = i;
			}
			// Is this an audio stream?
			if (AV_GET_CODEC_TYPE(pFormatCtx->streams[i]) == AVMEDIA_TYPE_AUDIO && audioStream < 0) {
				audioStream = i;
			}
		}
		if (videoStream == -1 && audioStream == -1)
			throw NoStreamsFound("No video or audio streams found in this file.", path);

		// Is there a video stream?
		if (videoStream != -1) {
			// Set the stream index
			info.video_stream_index = videoStream;

			// Set the codec and codec context pointers
			pStream = pFormatCtx->streams[videoStream];

			// Find the codec ID from stream
			AVCodecID codecId = AV_FIND_DECODER_CODEC_ID(pStream);

			// Get codec and codec context from stream
			AVCodec *pCodec = avcodec_find_decoder(codecId);
			AVDictionary *opts = NULL;
			int retry_decode_open = 2;
			// If hw accel is selected but hardware cannot handle repeat with software decoding
			do {
				pCodecCtx = AV_GET_CODEC_CONTEXT(pStream, pCodec);
#if HAVE_HW_ACCEL
				if (hw_de_on && (retry_decode_open==2)) {
					// Up to here no decision is made if hardware or software decode
					hw_de_supported = IsHardwareDecodeSupported(pCodecCtx->codec_id);
				}
#endif
				retry_decode_open = 0;

				// Set number of threads equal to number of processors (not to exceed 16)
				pCodecCtx->thread_count = std::min(FF_NUM_PROCESSORS, 16);

				if (pCodec == NULL) {
					throw InvalidCodec("A valid video codec could not be found for this file.", path);
				}

				// Init options
				av_dict_set(&opts, "strict", "experimental", 0);
#if HAVE_HW_ACCEL
				if (hw_de_on && hw_de_supported) {
					// Open Hardware Acceleration
					int i_decoder_hw = 0;
					char adapter[256];
					char *adapter_ptr = NULL;
					int adapter_num;
					adapter_num = openshot::Settings::Instance()->HW_DE_DEVICE_SET;
					fprintf(stderr, "Hardware decoding device number: %d\n", adapter_num);

					// Set hardware pix format (callback)
					pCodecCtx->get_format = get_hw_dec_format;

					if (adapter_num < 3 && adapter_num >=0) {
#if defined(__linux__)
						snprintf(adapter,sizeof(adapter),"/dev/dri/renderD%d", adapter_num+128);
						adapter_ptr = adapter;
						i_decoder_hw = openshot::Settings::Instance()->HARDWARE_DECODER;
						switch (i_decoder_hw) {
								case 1:
									hw_de_av_device_type = AV_HWDEVICE_TYPE_VAAPI;
									break;
								case 2:
									hw_de_av_device_type = AV_HWDEVICE_TYPE_CUDA;
									break;
								case 6:
									hw_de_av_device_type = AV_HWDEVICE_TYPE_VDPAU;
									break;
								case 7:
									hw_de_av_device_type = AV_HWDEVICE_TYPE_QSV;
									break;
								default:
									hw_de_av_device_type = AV_HWDEVICE_TYPE_VAAPI;
									break;
							}

#elif defined(_WIN32)
						adapter_ptr = NULL;
						i_decoder_hw = openshot::Settings::Instance()->HARDWARE_DECODER;
						switch (i_decoder_hw) {
							case 2:
								hw_de_av_device_type = AV_HWDEVICE_TYPE_CUDA;
								break;
							case 3:
								hw_de_av_device_type = AV_HWDEVICE_TYPE_DXVA2;
								break;
							case 4:
								hw_de_av_device_type = AV_HWDEVICE_TYPE_D3D11VA;
								break;
							case 7:
								hw_de_av_device_type = AV_HWDEVICE_TYPE_QSV;
								break;
							default:
								hw_de_av_device_type = AV_HWDEVICE_TYPE_DXVA2;
								break;
						}
#elif defined(__APPLE__)
						adapter_ptr = NULL;
						i_decoder_hw = openshot::Settings::Instance()->HARDWARE_DECODER;
						switch (i_decoder_hw) {
							case 5:
								hw_de_av_device_type =  AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
								break;
							case 7:
								hw_de_av_device_type = AV_HWDEVICE_TYPE_QSV;
								break;
							default:
								hw_de_av_device_type = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
								break;
						}
#endif

					} else {
						adapter_ptr = NULL; // Just to be sure
					}

					// Check if it is there and writable
#if defined(__linux__)
					if( adapter_ptr != NULL && access( adapter_ptr, W_OK ) == 0 ) {
#elif defined(_WIN32)
					if( adapter_ptr != NULL ) {
#elif defined(__APPLE__)
					if( adapter_ptr != NULL ) {
#endif
						ZmqLogger::Instance()->AppendDebugMethod("Decode Device present using device");
					}
					else {
						adapter_ptr = NULL;  // use default
						ZmqLogger::Instance()->AppendDebugMethod("Decode Device not present using default");
					}

					hw_device_ctx = NULL;
					// Here the first hardware initialisations are made
					if (av_hwdevice_ctx_create(&hw_device_ctx, hw_de_av_device_type, adapter_ptr, NULL, 0) >= 0) {
						if (!(pCodecCtx->hw_device_ctx = av_buffer_ref(hw_device_ctx))) {
							throw InvalidCodec("Hardware device reference create failed.", path);
						}

						/*
						av_buffer_unref(&ist->hw_frames_ctx);
						ist->hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx);
						if (!ist->hw_frames_ctx) {
							av_log(avctx, AV_LOG_ERROR, "Error creating a CUDA frames context\n");
							return AVERROR(ENOMEM);
						}

						frames_ctx = (AVHWFramesContext*)ist->hw_frames_ctx->data;

						frames_ctx->format = AV_PIX_FMT_CUDA;
						frames_ctx->sw_format = avctx->sw_pix_fmt;
						frames_ctx->width = avctx->width;
						frames_ctx->height = avctx->height;

						av_log(avctx, AV_LOG_DEBUG, "Initializing CUDA frames context: sw_format = %s, width = %d, height = %d\n",
								av_get_pix_fmt_name(frames_ctx->sw_format), frames_ctx->width, frames_ctx->height);


						ret = av_hwframe_ctx_init(pCodecCtx->hw_device_ctx);
						ret = av_hwframe_ctx_init(ist->hw_frames_ctx);
						if (ret < 0) {
						  av_log(avctx, AV_LOG_ERROR, "Error initializing a CUDA frame pool\n");
						  return ret;
						}
						*/
					}
					else {
						  throw InvalidCodec("Hardware device create failed.", path);
					}
				}
#endif // HAVE_HW_ACCEL

				// Open video codec
				if (avcodec_open2(pCodecCtx, pCodec, &opts) < 0)
					throw InvalidCodec("A video codec was found, but could not be opened.", path);

#if HAVE_HW_ACCEL
				if (hw_de_on && hw_de_supported) {
					AVHWFramesConstraints *constraints = NULL;
					void *hwconfig = NULL;
					hwconfig = av_hwdevice_hwconfig_alloc(hw_device_ctx);

// TODO: needs va_config!
#if ENABLE_VAAPI
					((AVVAAPIHWConfig *)hwconfig)->config_id = ((VAAPIDecodeContext *)(pCodecCtx->priv_data))->va_config;
					constraints = av_hwdevice_get_hwframe_constraints(hw_device_ctx,hwconfig);
#endif // ENABLE_VAAPI
					if (constraints) {
						if (pCodecCtx->coded_width < constraints->min_width  	||
								pCodecCtx->coded_height < constraints->min_height ||
								pCodecCtx->coded_width > constraints->max_width  	||
								pCodecCtx->coded_height > constraints->max_height) {
							ZmqLogger::Instance()->AppendDebugMethod("DIMENSIONS ARE TOO LARGE for hardware acceleration\n");
							hw_de_supported = 0;
							retry_decode_open = 1;
							AV_FREE_CONTEXT(pCodecCtx);
							if (hw_device_ctx) {
								av_buffer_unref(&hw_device_ctx);
								hw_device_ctx = NULL;
							}
						}
						else {
							// All is just peachy
							ZmqLogger::Instance()->AppendDebugMethod("\nDecode hardware acceleration is used\n", "Min width :", constraints->min_width, "Min Height :", constraints->min_height, "MaxWidth :", constraints->max_width, "MaxHeight :", constraints->max_height, "Frame width :", pCodecCtx->coded_width, "Frame height :", pCodecCtx->coded_height);
							retry_decode_open = 0;
						}
						av_hwframe_constraints_free(&constraints);
						if (hwconfig) {
							av_freep(&hwconfig);
						}
					}
					else {
						int max_h, max_w;
						//max_h = ((getenv( "LIMIT_HEIGHT_MAX" )==NULL) ? MAX_SUPPORTED_HEIGHT : atoi(getenv( "LIMIT_HEIGHT_MAX" )));
						max_h = openshot::Settings::Instance()->DE_LIMIT_HEIGHT_MAX;
						//max_w = ((getenv( "LIMIT_WIDTH_MAX" )==NULL) ? MAX_SUPPORTED_WIDTH : atoi(getenv( "LIMIT_WIDTH_MAX" )));
						max_w = openshot::Settings::Instance()->DE_LIMIT_WIDTH_MAX;
						ZmqLogger::Instance()->AppendDebugMethod("Constraints could not be found using default limit\n");
						//cerr << "Constraints could not be found using default limit\n";
						if (pCodecCtx->coded_width < 0  	||
								pCodecCtx->coded_height < 0 	||
								pCodecCtx->coded_width > max_w ||
								pCodecCtx->coded_height > max_h ) {
							ZmqLogger::Instance()->AppendDebugMethod("DIMENSIONS ARE TOO LARGE for hardware acceleration\n", "Max Width :", max_w, "Max Height :", max_h, "Frame width :", pCodecCtx->coded_width, "Frame height :", pCodecCtx->coded_height);
							hw_de_supported = 0;
							retry_decode_open = 1;
							AV_FREE_CONTEXT(pCodecCtx);
							if (hw_device_ctx) {
								av_buffer_unref(&hw_device_ctx);
								hw_device_ctx = NULL;
							}
						}
						else {
							ZmqLogger::Instance()->AppendDebugMethod("\nDecode hardware acceleration is used\n", "Max Width :", max_w, "Max Height :", max_h, "Frame width :", pCodecCtx->coded_width, "Frame height :", pCodecCtx->coded_height);
							retry_decode_open = 0;
						}
					}
				} // if hw_de_on && hw_de_supported
				else {
					ZmqLogger::Instance()->AppendDebugMethod("\nDecode in software is used\n");
				}
#else
				retry_decode_open = 0;
#endif // HAVE_HW_ACCEL
			} while (retry_decode_open); // retry_decode_open
			// Free options
			av_dict_free(&opts);

			// Update the File Info struct with video details (if a video stream is found)
			UpdateVideoInfo();
		}

		// Is there an audio stream?
		if (audioStream != -1) {
			// Set the stream index
			info.audio_stream_index = audioStream;

			// Get a pointer to the codec context for the audio stream
			aStream = pFormatCtx->streams[audioStream];

			// Find the codec ID from stream
			AVCodecID codecId = AV_FIND_DECODER_CODEC_ID(aStream);

			// Get codec and codec context from stream
			AVCodec *aCodec = avcodec_find_decoder(codecId);
			aCodecCtx = AV_GET_CODEC_CONTEXT(aStream, aCodec);

			// Set number of threads equal to number of processors (not to exceed 16)
			aCodecCtx->thread_count = std::min(FF_NUM_PROCESSORS, 16);

			if (aCodec == NULL) {
				throw InvalidCodec("A valid audio codec could not be found for this file.", path);
			}

			// Init options
			AVDictionary *opts = NULL;
			av_dict_set(&opts, "strict", "experimental", 0);

			// Open audio codec
			if (avcodec_open2(aCodecCtx, aCodec, &opts) < 0)
				throw InvalidCodec("An audio codec was found, but could not be opened.", path);

			// Free options
			av_dict_free(&opts);

			// Update the File Info struct with audio details (if an audio stream is found)
			UpdateAudioInfo();
		}

		// Add format metadata (if any)
		AVDictionaryEntry *tag = NULL;
		while ((tag = av_dict_get(pFormatCtx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
			QString str_key = tag->key;
			QString str_value = tag->value;
			info.metadata[str_key.toStdString()] = str_value.trimmed().toStdString();
		}

		// Init previous audio location to zero
		previous_packet_location.frame = -1;
		previous_packet_location.sample_start = 0;

		// Adjust cache size based on size of frame and audio
		working_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * info.fps.ToDouble() * 2, info.width, info.height, info.sample_rate, info.channels);
		missing_frames.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);
		final_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);

		// Mark as "open"
		is_open = true;
	}
}

void FFmpegReader::Close() {
	// Close all objects, if reader is 'open'
	if (is_open) {
		// Mark as "closed"
		is_open = false;

		ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::Close");

		if (packet) {
			// Remove previous packet before getting next one
			RemoveAVPacket(packet);
			packet = NULL;
		}

		// Close the codec
		if (info.has_video) {
			avcodec_flush_buffers(pCodecCtx);
			AV_FREE_CONTEXT(pCodecCtx);
#if HAVE_HW_ACCEL
			if (hw_de_on) {
				if (hw_device_ctx) {
					av_buffer_unref(&hw_device_ctx);
					hw_device_ctx = NULL;
				}
			}
#endif // HAVE_HW_ACCEL
		}
		if (info.has_audio) {
			avcodec_flush_buffers(aCodecCtx);
			AV_FREE_CONTEXT(aCodecCtx);
		}

		// Clear final cache
		final_cache.Clear();
		working_cache.Clear();
		missing_frames.Clear();

		// Clear processed lists
		{
			const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
			processed_video_frames.clear();
			processed_audio_frames.clear();
			processing_video_frames.clear();
			processing_audio_frames.clear();
			missing_audio_frames.clear();
			missing_video_frames.clear();
			missing_audio_frames_source.clear();
			missing_video_frames_source.clear();
			checked_frames.clear();
		}

		// Close the video file
		avformat_close_input(&pFormatCtx);
		av_freep(&pFormatCtx);

		// Reset some variables
		last_frame = 0;
		largest_frame_processed = 0;
		seek_audio_frame_found = 0;
		seek_video_frame_found = 0;
		current_video_frame = 0;
		has_missing_frames = false;

		last_video_frame.reset();
	}
}

void FFmpegReader::UpdateAudioInfo() {
	// Set values of FileInfo struct
	info.has_audio = true;
	info.file_size = pFormatCtx->pb ? avio_size(pFormatCtx->pb) : -1;
	info.acodec = aCodecCtx->codec->name;
	info.channels = AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channels;
	if (AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channel_layout == 0)
		AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channel_layout = av_get_default_channel_layout(AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channels);
	info.channel_layout = (ChannelLayout) AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channel_layout;
	info.sample_rate = AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->sample_rate;
	info.audio_bit_rate = AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->bit_rate;

	// Set audio timebase
	info.audio_timebase.num = aStream->time_base.num;
	info.audio_timebase.den = aStream->time_base.den;

	// Get timebase of audio stream (if valid) and greater than the current duration
	if (aStream->duration > 0.0f && aStream->duration > info.duration)
		info.duration = aStream->duration * info.audio_timebase.ToDouble();

	// Check for an invalid video length
	if (info.has_video && info.video_length <= 0) {
		// Calculate the video length from the audio duration
		info.video_length = info.duration * info.fps.ToDouble();
	}

	// Set video timebase (if no video stream was found)
	if (!info.has_video) {
		// Set a few important default video settings (so audio can be divided into frames)
		info.fps.num = 24;
		info.fps.den = 1;
		info.video_timebase.num = 1;
		info.video_timebase.den = 24;
		info.video_length = info.duration * info.fps.ToDouble();
		info.width = 720;
		info.height = 480;
	}

	// Fix invalid video lengths for certain types of files (MP3 for example)
	if (info.has_video && ((info.duration * info.fps.ToDouble()) - info.video_length > 60)) {
		info.video_length = info.duration * info.fps.ToDouble();
	}

	// Add audio metadata (if any found)
	AVDictionaryEntry *tag = NULL;
	while ((tag = av_dict_get(aStream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
		QString str_key = tag->key;
		QString str_value = tag->value;
		info.metadata[str_key.toStdString()] = str_value.trimmed().toStdString();
	}
}

void FFmpegReader::UpdateVideoInfo() {
	if (check_fps)
		// Already initialized all the video metadata, no reason to do it again
		return;

	// Set values of FileInfo struct
	info.has_video = true;
	info.file_size = pFormatCtx->pb ? avio_size(pFormatCtx->pb) : -1;
	info.height = AV_GET_CODEC_ATTRIBUTES(pStream, pCodecCtx)->height;
	info.width = AV_GET_CODEC_ATTRIBUTES(pStream, pCodecCtx)->width;
	info.vcodec = pCodecCtx->codec->name;
	info.video_bit_rate = (pFormatCtx->bit_rate / 8);

	// Frame rate from the container and codec
	AVRational framerate = av_guess_frame_rate(pFormatCtx, pStream, NULL);
	info.fps.num = framerate.num;
	info.fps.den = framerate.den;

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::UpdateVideoInfo", "info.fps.num", info.fps.num, "info.fps.den", info.fps.den);

	// TODO: remove excessive debug info in the next releases
	// The debug info below is just for comparison and troubleshooting on users side during the transition period
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::UpdateVideoInfo (pStream->avg_frame_rate)", "num", pStream->avg_frame_rate.num, "den", pStream->avg_frame_rate.den);

	if (pStream->sample_aspect_ratio.num != 0) {
		info.pixel_ratio.num = pStream->sample_aspect_ratio.num;
		info.pixel_ratio.den = pStream->sample_aspect_ratio.den;
	} else if (AV_GET_CODEC_ATTRIBUTES(pStream, pCodecCtx)->sample_aspect_ratio.num != 0) {
		info.pixel_ratio.num = AV_GET_CODEC_ATTRIBUTES(pStream, pCodecCtx)->sample_aspect_ratio.num;
		info.pixel_ratio.den = AV_GET_CODEC_ATTRIBUTES(pStream, pCodecCtx)->sample_aspect_ratio.den;
	} else {
		info.pixel_ratio.num = 1;
		info.pixel_ratio.den = 1;
	}
	info.pixel_format = AV_GET_CODEC_PIXEL_FORMAT(pStream, pCodecCtx);

	// Calculate the DAR (display aspect ratio)
	Fraction size(info.width * info.pixel_ratio.num, info.height * info.pixel_ratio.den);

	// Reduce size fraction
	size.Reduce();

	// Set the ratio based on the reduced fraction
	info.display_ratio.num = size.num;
	info.display_ratio.den = size.den;

	// Get scan type and order from codec context/params
	if (!check_interlace) {
		check_interlace = true;
		AVFieldOrder field_order = AV_GET_CODEC_ATTRIBUTES(pStream, pCodecCtx)->field_order;
		switch(field_order) {
			case AV_FIELD_PROGRESSIVE:
				info.interlaced_frame = false;
				break;
			case AV_FIELD_TT:
			case AV_FIELD_TB:
				info.interlaced_frame = true;
				info.top_field_first = true;
				break;
			case AV_FIELD_BT:
			case AV_FIELD_BB:
				info.interlaced_frame = true;
				info.top_field_first = false;
				break;
			case AV_FIELD_UNKNOWN:
				// Check again later?
				check_interlace = false;
				break;
		}
		// check_interlace will prevent these checks being repeated,
		// unless it was cleared because we got an AV_FIELD_UNKNOWN response.
	}

	// Set the video timebase
	info.video_timebase.num = pStream->time_base.num;
	info.video_timebase.den = pStream->time_base.den;

	// Set the duration in seconds, and video length (# of frames)
	info.duration = pStream->duration * info.video_timebase.ToDouble();

	// Check for valid duration (if found)
	if (info.duration <= 0.0f && pFormatCtx->duration >= 0)
		// Use the format's duration
		info.duration = pFormatCtx->duration / AV_TIME_BASE;

	// Calculate duration from filesize and bitrate (if any)
	if (info.duration <= 0.0f && info.video_bit_rate > 0 && info.file_size > 0)
		// Estimate from bitrate, total bytes, and framerate
		info.duration = (info.file_size / info.video_bit_rate);

	// No duration found in stream of file
	if (info.duration <= 0.0f) {
		// No duration is found in the video stream
		info.duration = -1;
		info.video_length = -1;
		is_duration_known = false;
	} else {
		// Yes, a duration was found
		is_duration_known = true;

		// Calculate number of frames
		info.video_length = round(info.duration * info.fps.ToDouble());
	}

	// Override an invalid framerate
	if (info.fps.ToFloat() > 240.0f || (info.fps.num <= 0 || info.fps.den <= 0) || info.video_length <= 0) {
		// Calculate FPS, duration, video bit rate, and video length manually
		// by scanning through all the video stream packets
		CheckFPS();
	}

	// Add video metadata (if any)
	AVDictionaryEntry *tag = NULL;
	while ((tag = av_dict_get(pStream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
		QString str_key = tag->key;
		QString str_value = tag->value;
		info.metadata[str_key.toStdString()] = str_value.trimmed().toStdString();
	}
}


std::shared_ptr<Frame> FFmpegReader::GetFrame(int64_t requested_frame) {
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The FFmpegReader is closed.  Call Open() before calling this method.", path);

	// Adjust for a requested frame that is too small or too large
	if (requested_frame < 1)
		requested_frame = 1;
	if (requested_frame > info.video_length && is_duration_known)
		requested_frame = info.video_length;
	if (info.has_video && info.video_length == 0)
		// Invalid duration of video file
		throw InvalidFile("Could not detect the duration of the video or audio stream.", path);

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetFrame", "requested_frame", requested_frame, "last_frame", last_frame);

	// Check the cache for this frame
	std::shared_ptr<Frame> frame = final_cache.GetFrame(requested_frame);
	if (frame) {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetFrame", "returned cached frame", requested_frame);

		// Return the cached frame
		return frame;
	} else {
#pragma omp critical (ReadStream)
		{
			// Check the cache a 2nd time (due to a potential previous lock)
			frame = final_cache.GetFrame(requested_frame);
			if (frame) {
				// Debug output
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetFrame", "returned cached frame on 2nd look", requested_frame);

				// Return the cached frame
			} else {
				// Frame is not in cache
				// Reset seek count
				seek_count = 0;

				// Check for first frame (always need to get frame 1 before other frames, to correctly calculate offsets)
				if (last_frame == 0 && requested_frame != 1)
					// Get first frame
					ReadStream(1);

				// Are we within X frames of the requested frame?
				int64_t diff = requested_frame - last_frame;
				if (diff >= 1 && diff <= 20) {
					// Continue walking the stream
					frame = ReadStream(requested_frame);
				} else {
					// Greater than 30 frames away, or backwards, we need to seek to the nearest key frame
					if (enable_seek)
						// Only seek if enabled
						Seek(requested_frame);

					else if (!enable_seek && diff < 0) {
						// Start over, since we can't seek, and the requested frame is smaller than our position
						Close();
						Open();
					}

					// Then continue walking the stream
					frame = ReadStream(requested_frame);
				}
			}
		} //omp critical
		return frame;
	}
}

// Read the stream until we find the requested Frame
std::shared_ptr<Frame> FFmpegReader::ReadStream(int64_t requested_frame) {
	// Allocate video frame
	bool end_of_stream = false;
	bool check_seek = false;
	bool frame_finished = false;
	int packet_error = -1;

	// Minimum number of packets to process (for performance reasons)
	int packets_processed = 0;
	int minimum_packets = OPEN_MP_NUM_PROCESSORS;
	int max_packets = 4096;

	// Set the number of threads in OpenMP
	omp_set_num_threads(OPEN_MP_NUM_PROCESSORS);
	// Allow nested OpenMP sections
	omp_set_nested(true);

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ReadStream", "requested_frame", requested_frame, "OPEN_MP_NUM_PROCESSORS", OPEN_MP_NUM_PROCESSORS);

#pragma omp parallel
	{
#pragma omp single
		{
			// Loop through the stream until the correct frame is found
			while (true) {
				// Get the next packet into a local variable called packet
				packet_error = GetNextPacket();

				int processing_video_frames_size = 0;
				int processing_audio_frames_size = 0;
				{
					const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
					processing_video_frames_size = processing_video_frames.size();
					processing_audio_frames_size = processing_audio_frames.size();
				}

				// Wait if too many frames are being processed
				while (processing_video_frames_size + processing_audio_frames_size >= minimum_packets) {
					usleep(2500);
					const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
					processing_video_frames_size = processing_video_frames.size();
					processing_audio_frames_size = processing_audio_frames.size();
				}

				// Get the next packet (if any)
				if (packet_error < 0) {
					// Break loop when no more packets found
					end_of_stream = true;
					break;
				}

				// Debug output
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ReadStream (GetNextPacket)", "requested_frame", requested_frame, "processing_video_frames_size", processing_video_frames_size, "processing_audio_frames_size", processing_audio_frames_size, "minimum_packets", minimum_packets, "packets_processed", packets_processed, "is_seeking", is_seeking);

				// Video packet
				if (info.has_video && packet->stream_index == videoStream) {
					// Reset this counter, since we have a video packet
					num_packets_since_video_frame = 0;

					// Check the status of a seek (if any)
					if (is_seeking)
#pragma omp critical (openshot_seek)
						check_seek = CheckSeek(true);
					else
						check_seek = false;

					if (check_seek) {
						// Jump to the next iteration of this loop
						continue;
					}

					// Packet may become NULL on Close inside Seek if CheckSeek returns false
					if (!packet)
						// Jump to the next iteration of this loop
						continue;

					// Get the AVFrame from the current packet
					frame_finished = GetAVFrame();

					// Check if the AVFrame is finished and set it
					if (frame_finished) {
						// Update PTS / Frame Offset (if any)
						UpdatePTSOffset(true);

						// Process Video Packet
						ProcessVideoPacket(requested_frame);

						if (openshot::Settings::Instance()->WAIT_FOR_VIDEO_PROCESSING_TASK) {
							// Wait on each OMP task to complete before moving on to the next one. This slows
							// down processing considerably, but might be more stable on some systems.
#pragma omp taskwait
						}
					}

				}
				// Audio packet
				else if (info.has_audio && packet->stream_index == audioStream) {
					// Increment this (to track # of packets since the last video packet)
					num_packets_since_video_frame++;

					// Check the status of a seek (if any)
					if (is_seeking)
#pragma omp critical (openshot_seek)
						check_seek = CheckSeek(false);
					else
						check_seek = false;

					if (check_seek) {
						// Jump to the next iteration of this loop
						continue;
					}

					// Packet may become NULL on Close inside Seek if CheckSeek returns false
					if (!packet)
						// Jump to the next iteration of this loop
						continue;

					// Update PTS / Frame Offset (if any)
					UpdatePTSOffset(false);

					// Determine related video frame and starting sample # from audio PTS
					AudioLocation location = GetAudioPTSLocation(packet->pts);

					// Process Audio Packet
					ProcessAudioPacket(requested_frame, location.frame, location.sample_start);
				}

				// Check if working frames are 'finished'
				if (!is_seeking) {
					// Check for final frames
					CheckWorkingFrames(false, requested_frame);
				}

				// Check if requested 'final' frame is available
				bool is_cache_found = (final_cache.GetFrame(requested_frame) != NULL);

				// Increment frames processed
				packets_processed++;

				// Break once the frame is found
				if ((is_cache_found && packets_processed >= minimum_packets) || packets_processed > max_packets)
					break;

			} // end while

		} // end omp single

	} // end omp parallel

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ReadStream (Completed)", "packets_processed", packets_processed, "end_of_stream", end_of_stream, "largest_frame_processed", largest_frame_processed, "Working Cache Count", working_cache.Count());

	// End of stream?
	if (end_of_stream)
		// Mark the any other working frames as 'finished'
		CheckWorkingFrames(end_of_stream, requested_frame);

	// Return requested frame (if found)
	std::shared_ptr<Frame> frame = final_cache.GetFrame(requested_frame);
	if (frame)
		// Return prepared frame
		return frame;
	else {

		// Check if largest frame is still cached
		frame = final_cache.GetFrame(largest_frame_processed);
		if (frame) {
			// return the largest processed frame (assuming it was the last in the video file)
			return frame;
		} else {
			// The largest processed frame is no longer in cache, return a blank frame
			std::shared_ptr<Frame> f = CreateFrame(largest_frame_processed);
			f->AddColor(info.width, info.height, "#000");
			return f;
		}
	}

}

// Get the next packet (if any)
int FFmpegReader::GetNextPacket() {
	int found_packet = 0;
	AVPacket *next_packet;
#pragma omp critical(getnextpacket)
	{
		next_packet = new AVPacket();
		found_packet = av_read_frame(pFormatCtx, next_packet);


		if (packet) {
			// Remove previous packet before getting next one
			RemoveAVPacket(packet);
			packet = NULL;
		}

		if (found_packet >= 0) {
			// Update current packet pointer
			packet = next_packet;
		}
        else
            delete next_packet;
	}
	// Return if packet was found (or error number)
	return found_packet;
}

// Get an AVFrame (if any)
bool FFmpegReader::GetAVFrame() {
	int frameFinished = -1;
	int ret = 0;

	// Decode video frame
	AVFrame *next_frame = AV_ALLOCATE_FRAME();
#pragma omp critical (packet_cache)
	{
#if IS_FFMPEG_3_2
		frameFinished = 0;

		ret = avcodec_send_packet(pCodecCtx, packet);

	#if HAVE_HW_ACCEL
		// Get the format from the variables set in get_hw_dec_format
		hw_de_av_pix_fmt = hw_de_av_pix_fmt_global;
		hw_de_av_device_type = hw_de_av_device_type_global;
	#endif // HAVE_HW_ACCEL
		if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (Packet not sent)");
		}
		else {
			AVFrame *next_frame2;
	#if HAVE_HW_ACCEL
			if (hw_de_on && hw_de_supported) {
				next_frame2 = AV_ALLOCATE_FRAME();
			}
			else
	#endif // HAVE_HW_ACCEL
			{
				next_frame2 = next_frame;
			}
			pFrame = AV_ALLOCATE_FRAME();
			while (ret >= 0) {
				ret =  avcodec_receive_frame(pCodecCtx, next_frame2);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				}
				if (ret != 0) {
					ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (invalid return frame received)");
				}
	#if HAVE_HW_ACCEL
				if (hw_de_on && hw_de_supported) {
					int err;
					if (next_frame2->format == hw_de_av_pix_fmt) {
						next_frame->format = AV_PIX_FMT_YUV420P;
						if ((err = av_hwframe_transfer_data(next_frame,next_frame2,0)) < 0) {
							ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (Failed to transfer data to output frame)");
						}
						if ((err = av_frame_copy_props(next_frame,next_frame2)) < 0) {
							ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (Failed to copy props to output frame)");
						}
					}
				}
				else
	#endif // HAVE_HW_ACCEL
				{	// No hardware acceleration used -> no copy from GPU memory needed
					next_frame = next_frame2;
				}

				// TODO also handle possible further frames
				// Use only the first frame like avcodec_decode_video2
				if (frameFinished == 0 ) {
					frameFinished = 1;
					av_image_alloc(pFrame->data, pFrame->linesize, info.width, info.height, (AVPixelFormat)(pStream->codecpar->format), 1);
					av_image_copy(pFrame->data, pFrame->linesize, (const uint8_t**)next_frame->data, next_frame->linesize,
												(AVPixelFormat)(pStream->codecpar->format), info.width, info.height);
				}
			}
	#if HAVE_HW_ACCEL
			if (hw_de_on && hw_de_supported) {
				AV_FREE_FRAME(&next_frame2);
			}
	#endif // HAVE_HW_ACCEL
		}
#else
		avcodec_decode_video2(pCodecCtx, next_frame, &frameFinished, packet);

		// always allocate pFrame (because we do that in the ffmpeg >= 3.2 as well); it will always be freed later
		pFrame = AV_ALLOCATE_FRAME();

		// is frame finished
		if (frameFinished) {
			// AVFrames are clobbered on the each call to avcodec_decode_video, so we
			// must make a copy of the image data before this method is called again.
			avpicture_alloc((AVPicture *) pFrame, pCodecCtx->pix_fmt, info.width, info.height);
			av_picture_copy((AVPicture *) pFrame, (AVPicture *) next_frame, pCodecCtx->pix_fmt, info.width,
							info.height);
		}
#endif // IS_FFMPEG_3_2
	}

	// deallocate the frame
	AV_FREE_FRAME(&next_frame);

	// Did we get a video frame?
	return frameFinished;
}

// Check the current seek position and determine if we need to seek again
bool FFmpegReader::CheckSeek(bool is_video) {
	// Are we seeking for a specific frame?
	if (is_seeking) {
		// Determine if both an audio and video packet have been decoded since the seek happened.
		// If not, allow the ReadStream method to keep looping
		if ((is_video_seek && !seek_video_frame_found) || (!is_video_seek && !seek_audio_frame_found))
			return false;

		// Check for both streams
		if ((info.has_video && !seek_video_frame_found) || (info.has_audio && !seek_audio_frame_found))
			return false;

		// Determine max seeked frame
		int64_t max_seeked_frame = seek_audio_frame_found; // determine max seeked frame
		if (seek_video_frame_found > max_seeked_frame)
			max_seeked_frame = seek_video_frame_found;

		// determine if we are "before" the requested frame
		if (max_seeked_frame >= seeking_frame) {
			// SEEKED TOO FAR
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckSeek (Too far, seek again)", "is_video_seek", is_video_seek, "max_seeked_frame", max_seeked_frame, "seeking_frame", seeking_frame, "seeking_pts", seeking_pts, "seek_video_frame_found", seek_video_frame_found, "seek_audio_frame_found", seek_audio_frame_found);

			// Seek again... to the nearest Keyframe
			Seek(seeking_frame - (10 * seek_count * seek_count));
		} else {
			// SEEK WORKED
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckSeek (Successful)", "is_video_seek", is_video_seek, "current_pts", packet->pts, "seeking_pts", seeking_pts, "seeking_frame", seeking_frame, "seek_video_frame_found", seek_video_frame_found, "seek_audio_frame_found", seek_audio_frame_found);

			// Seek worked, and we are "before" the requested frame
			is_seeking = false;
			seeking_frame = 0;
			seeking_pts = -1;
		}
	}

	// return the pts to seek to (if any)
	return is_seeking;
}

// Process a video packet
void FFmpegReader::ProcessVideoPacket(int64_t requested_frame) {
	// Calculate current frame #
	int64_t current_frame = ConvertVideoPTStoFrame(GetVideoPTS());

	// Track 1st video packet after a successful seek
	if (!seek_video_frame_found && is_seeking)
		seek_video_frame_found = current_frame;

	// Are we close enough to decode the frame? and is this frame # valid?
	if ((current_frame < (requested_frame - 20)) or (current_frame == -1)) {
		// Remove frame and packet
		RemoveAVFrame(pFrame);

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessVideoPacket (Skipped)", "requested_frame", requested_frame, "current_frame", current_frame);

		// Skip to next frame without decoding or caching
		return;
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessVideoPacket (Before)", "requested_frame", requested_frame, "current_frame", current_frame);

	// Init some things local (for OpenMP)
	PixelFormat pix_fmt = AV_GET_CODEC_PIXEL_FORMAT(pStream, pCodecCtx);
	int height = info.height;
	int width = info.width;
	int64_t video_length = info.video_length;
	AVFrame *my_frame = pFrame;
	pFrame = NULL;

	// Add video frame to list of processing video frames
	const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
	processing_video_frames[current_frame] = current_frame;

#pragma omp task firstprivate(current_frame, my_frame, height, width, video_length, pix_fmt)
	{
		// Create variables for a RGB Frame (since most videos are not in RGB, we must convert it)
		AVFrame *pFrameRGB = NULL;
		int numBytes;
		uint8_t *buffer = NULL;

		// Allocate an AVFrame structure
		pFrameRGB = AV_ALLOCATE_FRAME();
		if (pFrameRGB == NULL)
			throw OutOfBoundsFrame("Convert Image Broke!", current_frame, video_length);

		// Determine the max size of this source image (based on the timeline's size, the scaling mode,
		// and the scaling keyframes). This is a performance improvement, to keep the images as small as possible,
		// without losing quality. NOTE: We cannot go smaller than the timeline itself, or the add_layer timeline
		// method will scale it back to timeline size before scaling it smaller again. This needs to be fixed in
		// the future.
		int max_width = openshot::Settings::Instance()->MAX_WIDTH;
		if (max_width <= 0)
			max_width = info.width;
		int max_height = openshot::Settings::Instance()->MAX_HEIGHT;
		if (max_height <= 0)
			max_height = info.height;

		Clip *parent = (Clip *) GetClip();
		if (parent) {
			if (parent->scale == SCALE_FIT || parent->scale == SCALE_STRETCH) {
				// Best fit or Stretch scaling (based on max timeline size * scaling keyframes)
				float max_scale_x = parent->scale_x.GetMaxPoint().co.Y;
				float max_scale_y = parent->scale_y.GetMaxPoint().co.Y;
				max_width = std::max(float(max_width), max_width * max_scale_x);
				max_height = std::max(float(max_height), max_height * max_scale_y);

			} else if (parent->scale == SCALE_CROP) {
				// Cropping scale mode (based on max timeline size * cropped size * scaling keyframes)
				float max_scale_x = parent->scale_x.GetMaxPoint().co.Y;
				float max_scale_y = parent->scale_y.GetMaxPoint().co.Y;
				QSize width_size(max_width * max_scale_x,
								 round(max_width / (float(info.width) / float(info.height))));
				QSize height_size(round(max_height / (float(info.height) / float(info.width))),
								  max_height * max_scale_y);
				// respect aspect ratio
				if (width_size.width() >= max_width && width_size.height() >= max_height) {
					max_width = std::max(max_width, width_size.width());
					max_height = std::max(max_height, width_size.height());
				} else {
					max_width = std::max(max_width, height_size.width());
					max_height = std::max(max_height, height_size.height());
				}

			} else {
				// No scaling, use original image size (slower)
				max_width = info.width;
				max_height = info.height;
			}
		}

		// Determine if image needs to be scaled (for performance reasons)
		int original_height = height;
		if (max_width != 0 && max_height != 0 && max_width < width && max_height < height) {
			// Override width and height (but maintain aspect ratio)
			float ratio = float(width) / float(height);
			int possible_width = round(max_height * ratio);
			int possible_height = round(max_width / ratio);

			if (possible_width <= max_width) {
				// use calculated width, and max_height
				width = possible_width;
				height = max_height;
			} else {
				// use max_width, and calculated height
				width = max_width;
				height = possible_height;
			}
		}

		// Determine required buffer size and allocate buffer
		numBytes = AV_GET_IMAGE_SIZE(PIX_FMT_RGBA, width, height);

#pragma omp critical (video_buffer)
		buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

		// Copy picture data from one AVFrame (or AVPicture) to another one.
		AV_COPY_PICTURE_DATA(pFrameRGB, buffer, PIX_FMT_RGBA, width, height);

		int scale_mode = SWS_FAST_BILINEAR;
		if (openshot::Settings::Instance()->HIGH_QUALITY_SCALING) {
			scale_mode = SWS_BICUBIC;
		}
		SwsContext *img_convert_ctx = sws_getContext(info.width, info.height, AV_GET_CODEC_PIXEL_FORMAT(pStream, pCodecCtx), width,
															  height, PIX_FMT_RGBA, scale_mode, NULL, NULL, NULL);

		// Resize / Convert to RGB
		sws_scale(img_convert_ctx, my_frame->data, my_frame->linesize, 0,
				  original_height, pFrameRGB->data, pFrameRGB->linesize);

		// Create or get the existing frame object
		std::shared_ptr<Frame> f = CreateFrame(current_frame);

		// Add Image data to frame
		f->AddImage(width, height, 4, QImage::Format_RGBA8888, buffer);

		// Update working cache
		working_cache.Add(f);

		// Keep track of last last_video_frame
#pragma omp critical (video_buffer)
		last_video_frame = f;

		// Free the RGB image
		av_free(buffer);
		AV_FREE_FRAME(&pFrameRGB);

		// Remove frame and packet
		RemoveAVFrame(my_frame);
		sws_freeContext(img_convert_ctx);

		// Remove video frame from list of processing video frames
		{
			const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
			processing_video_frames.erase(current_frame);
			processed_video_frames[current_frame] = current_frame;
		}

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessVideoPacket (After)", "requested_frame", requested_frame, "current_frame", current_frame, "f->number", f->number);

	} // end omp task

}

// Process an audio packet
void FFmpegReader::ProcessAudioPacket(int64_t requested_frame, int64_t target_frame, int starting_sample) {
	// Track 1st audio packet after a successful seek
	if (!seek_audio_frame_found && is_seeking)
		seek_audio_frame_found = target_frame;

	// Are we close enough to decode the frame's audio?
	if (target_frame < (requested_frame - 20)) {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (Skipped)", "requested_frame", requested_frame, "target_frame", target_frame, "starting_sample", starting_sample);

		// Skip to next frame without decoding or caching
		return;
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (Before)", "requested_frame", requested_frame, "target_frame", target_frame, "starting_sample", starting_sample);

	// Init an AVFrame to hold the decoded audio samples
	int frame_finished = 0;
	AVFrame *audio_frame = AV_ALLOCATE_FRAME();
	AV_RESET_FRAME(audio_frame);

	int packet_samples = 0;
	int data_size = 0;

#pragma omp critical (ProcessAudioPacket)
	{
#if IS_FFMPEG_3_2
		int ret = 0;
		frame_finished = 1;
		while((packet->size > 0 || (!packet->data && frame_finished)) && ret >= 0) {
			frame_finished = 0;
			ret =  avcodec_send_packet(aCodecCtx, packet);
			if (ret < 0 && ret !=  AVERROR(EINVAL) && ret != AVERROR_EOF) {
				avcodec_send_packet(aCodecCtx, NULL);
				break;
			}
			if (ret >= 0)
				packet->size = 0;
			ret =  avcodec_receive_frame(aCodecCtx, audio_frame);
			if (ret >= 0)
				frame_finished = 1;
			if(ret == AVERROR(EINVAL) || ret == AVERROR_EOF) {
				avcodec_flush_buffers(aCodecCtx);
				ret = 0;
			}
			if (ret >= 0) {
				ret = frame_finished;
			}
		}
		if (!packet->data && !frame_finished)
		{
			ret = -1;
		}
#else
		int used = avcodec_decode_audio4(aCodecCtx, audio_frame, &frame_finished, packet);
#endif
	}

	if (frame_finished) {

		// determine how many samples were decoded
		int plane_size = -1;
		data_size = av_samples_get_buffer_size(&plane_size,
											   AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channels,
											   audio_frame->nb_samples,
											   (AVSampleFormat) (AV_GET_SAMPLE_FORMAT(aStream, aCodecCtx)), 1);

		// Calculate total number of samples
		packet_samples = audio_frame->nb_samples * AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channels;
	}

	// Estimate the # of samples and the end of this packet's location (to prevent GAPS for the next timestamp)
	int pts_remaining_samples = packet_samples / info.channels; // Adjust for zero based array

	// DEBUG (FOR AUDIO ISSUES) - Get the audio packet start time (in seconds)
	int64_t adjusted_pts = packet->pts + audio_pts_offset;
	double audio_seconds = double(adjusted_pts) * info.audio_timebase.ToDouble();
	double sample_seconds = double(pts_total) / info.sample_rate;

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (Decode Info A)", "pts_counter", pts_counter, "PTS", adjusted_pts, "Offset", audio_pts_offset, "PTS Diff", adjusted_pts - prev_pts, "Samples", pts_remaining_samples, "Sample PTS ratio", float(adjusted_pts - prev_pts) / pts_remaining_samples);
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (Decode Info B)", "Sample Diff", pts_remaining_samples - prev_samples - prev_pts, "Total", pts_total, "PTS Seconds", audio_seconds, "Sample Seconds", sample_seconds, "Seconds Diff", audio_seconds - sample_seconds, "raw samples", packet_samples);

	// DEBUG (FOR AUDIO ISSUES)
	prev_pts = adjusted_pts;
	pts_total += pts_remaining_samples;
	pts_counter++;
	prev_samples = pts_remaining_samples;

	// Add audio frame to list of processing audio frames
	{
		const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
		processing_audio_frames.insert(std::pair<int, int>(previous_packet_location.frame, previous_packet_location.frame));
	}

	while (pts_remaining_samples) {
		// Get Samples per frame (for this frame number)
		int samples_per_frame = Frame::GetSamplesPerFrame(previous_packet_location.frame, info.fps, info.sample_rate, info.channels);

		// Calculate # of samples to add to this frame
		int samples = samples_per_frame - previous_packet_location.sample_start;
		if (samples > pts_remaining_samples)
			samples = pts_remaining_samples;

		// Decrement remaining samples
		pts_remaining_samples -= samples;

		if (pts_remaining_samples > 0) {
			// next frame
			previous_packet_location.frame++;
			previous_packet_location.sample_start = 0;

			// Add audio frame to list of processing audio frames
			{
				const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
				processing_audio_frames.insert(std::pair<int, int>(previous_packet_location.frame, previous_packet_location.frame));
			}

		} else {
			// Increment sample start
			previous_packet_location.sample_start += samples;
		}
	}


	// Allocate audio buffer
	int16_t *audio_buf = new int16_t[AVCODEC_MAX_AUDIO_FRAME_SIZE + MY_INPUT_BUFFER_PADDING_SIZE];

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (ReSample)", "packet_samples", packet_samples, "info.channels", info.channels, "info.sample_rate", info.sample_rate, "aCodecCtx->sample_fmt", AV_GET_SAMPLE_FORMAT(aStream, aCodecCtx), "AV_SAMPLE_FMT_S16", AV_SAMPLE_FMT_S16);

	// Create output frame
	AVFrame *audio_converted = AV_ALLOCATE_FRAME();
	AV_RESET_FRAME(audio_converted);
	audio_converted->nb_samples = audio_frame->nb_samples;
	av_samples_alloc(audio_converted->data, audio_converted->linesize, info.channels, audio_frame->nb_samples, AV_SAMPLE_FMT_S16, 0);

	SWRCONTEXT *avr = NULL;
	int nb_samples = 0;

	// setup resample context
	avr = SWR_ALLOC();
	av_opt_set_int(avr, "in_channel_layout", AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channel_layout, 0);
	av_opt_set_int(avr, "out_channel_layout", AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channel_layout, 0);
	av_opt_set_int(avr, "in_sample_fmt", AV_GET_SAMPLE_FORMAT(aStream, aCodecCtx), 0);
	av_opt_set_int(avr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int(avr, "in_sample_rate", info.sample_rate, 0);
	av_opt_set_int(avr, "out_sample_rate", info.sample_rate, 0);
	av_opt_set_int(avr, "in_channels", info.channels, 0);
	av_opt_set_int(avr, "out_channels", info.channels, 0);
	SWR_INIT(avr);

	// Convert audio samples
	nb_samples = SWR_CONVERT(avr,    // audio resample context
							 audio_converted->data,          // output data pointers
							 audio_converted->linesize[0],   // output plane size, in bytes. (0 if unknown)
							 audio_converted->nb_samples,    // maximum number of samples that the output buffer can hold
							 audio_frame->data,              // input data pointers
							 audio_frame->linesize[0],       // input plane size, in bytes (0 if unknown)
							 audio_frame->nb_samples);       // number of input samples to convert

	// Copy audio samples over original samples
	memcpy(audio_buf, audio_converted->data[0], audio_converted->nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * info.channels);

	// Deallocate resample buffer
	SWR_CLOSE(avr);
	SWR_FREE(&avr);
	avr = NULL;

	// Free AVFrames
	av_free(audio_converted->data[0]);
	AV_FREE_FRAME(&audio_converted);

	int64_t starting_frame_number = -1;
	bool partial_frame = true;
	for (int channel_filter = 0; channel_filter < info.channels; channel_filter++) {
		// Array of floats (to hold samples for each channel)
		starting_frame_number = target_frame;
		int channel_buffer_size = packet_samples / info.channels;
		float *channel_buffer = new float[channel_buffer_size];

		// Init buffer array
		for (int z = 0; z < channel_buffer_size; z++)
			channel_buffer[z] = 0.0f;

		// Loop through all samples and add them to our Frame based on channel.
		// Toggle through each channel number, since channel data is stored like (left right left right)
		int channel = 0;
		int position = 0;
		for (int sample = 0; sample < packet_samples; sample++) {
			// Only add samples for current channel
			if (channel_filter == channel) {
				// Add sample (convert from (-32768 to 32768)  to (-1.0 to 1.0))
				channel_buffer[position] = audio_buf[sample] * (1.0f / (1 << 15));

				// Increment audio position
				position++;
			}

			// increment channel (if needed)
			if ((channel + 1) < info.channels)
				// move to next channel
				channel++;
			else
				// reset channel
				channel = 0;
		}

		// Loop through samples, and add them to the correct frames
		int start = starting_sample;
		int remaining_samples = channel_buffer_size;
		float *iterate_channel_buffer = channel_buffer;    // pointer to channel buffer
		while (remaining_samples > 0) {
			// Get Samples per frame (for this frame number)
			int samples_per_frame = Frame::GetSamplesPerFrame(starting_frame_number, info.fps, info.sample_rate, info.channels);

			// Calculate # of samples to add to this frame
			int samples = samples_per_frame - start;
			if (samples > remaining_samples)
				samples = remaining_samples;

			// Create or get the existing frame object
			std::shared_ptr<Frame> f = CreateFrame(starting_frame_number);

			// Determine if this frame was "partially" filled in
			if (samples_per_frame == start + samples)
				partial_frame = false;
			else
				partial_frame = true;

			// Add samples for current channel to the frame.
			f->AddAudio(true, channel_filter, start, iterate_channel_buffer, samples, 1.0f);

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (f->AddAudio)", "frame", starting_frame_number, "start", start, "samples", samples, "channel", channel_filter, "partial_frame", partial_frame, "samples_per_frame", samples_per_frame);

			// Add or update cache
			working_cache.Add(f);

			// Decrement remaining samples
			remaining_samples -= samples;

			// Increment buffer (to next set of samples)
			if (remaining_samples > 0)
				iterate_channel_buffer += samples;

			// Increment frame number
			starting_frame_number++;

			// Reset starting sample #
			start = 0;
		}

		// clear channel buffer
		delete[] channel_buffer;
		channel_buffer = NULL;
		iterate_channel_buffer = NULL;
	}

	// Clean up some arrays
	delete[] audio_buf;
	audio_buf = NULL;

	// Remove audio frame from list of processing audio frames
	{
		const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
		// Update all frames as completed
		for (int64_t f = target_frame; f < starting_frame_number; f++) {
			// Remove the frame # from the processing list. NOTE: If more than one thread is
			// processing this frame, the frame # will be in this list multiple times. We are only
			// removing a single instance of it here.
			processing_audio_frames.erase(processing_audio_frames.find(f));

			// Check and see if this frame is also being processed by another thread
			if (processing_audio_frames.count(f) == 0)
				// No other thread is processing it. Mark the audio as processed (final)
				processed_audio_frames[f] = f;
		}

		if (target_frame == starting_frame_number) {
			// This typically never happens, but just in case, remove the currently processing number
			processing_audio_frames.erase(processing_audio_frames.find(target_frame));
		}
	}

	// Free audio frame
	AV_FREE_FRAME(&audio_frame);

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (After)", "requested_frame", requested_frame, "starting_frame", target_frame, "end_frame", starting_frame_number - 1);

}


// Seek to a specific frame.  This is not always frame accurate, it's more of an estimation on many codecs.
void FFmpegReader::Seek(int64_t requested_frame) {
	// Adjust for a requested frame that is too small or too large
	if (requested_frame < 1)
		requested_frame = 1;
	if (requested_frame > info.video_length)
		requested_frame = info.video_length;

	int processing_video_frames_size = 0;
	int processing_audio_frames_size = 0;
	{
		const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
		processing_video_frames_size = processing_video_frames.size();
		processing_audio_frames_size = processing_audio_frames.size();
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::Seek", "requested_frame", requested_frame, "seek_count", seek_count, "last_frame", last_frame, "processing_video_frames_size", processing_video_frames_size, "processing_audio_frames_size", processing_audio_frames_size, "video_pts_offset", video_pts_offset);

	// Wait for any processing frames to complete
	while (processing_video_frames_size + processing_audio_frames_size > 0) {
		usleep(2500);
		const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
		processing_video_frames_size = processing_video_frames.size();
		processing_audio_frames_size = processing_audio_frames.size();
	}

	// Clear working cache (since we are seeking to another location in the file)
	working_cache.Clear();
	missing_frames.Clear();

	// Clear processed lists
	{
		const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
		processing_audio_frames.clear();
		processing_video_frames.clear();
		processed_video_frames.clear();
		processed_audio_frames.clear();
		missing_audio_frames.clear();
		missing_video_frames.clear();
		missing_audio_frames_source.clear();
		missing_video_frames_source.clear();
		checked_frames.clear();
	}

	// Reset the last frame variable
	last_frame = 0;
	current_video_frame = 0;
	largest_frame_processed = 0;
	num_checks_since_final = 0;
	num_packets_since_video_frame = 0;
	has_missing_frames = false;
	bool has_audio_override = info.has_audio;
	bool has_video_override = info.has_video;

	// Increment seek count
	seek_count++;

	// If seeking near frame 1, we need to close and re-open the file (this is more reliable than seeking)
	int buffer_amount = std::max(OPEN_MP_NUM_PROCESSORS, 8);
	if (requested_frame - buffer_amount < 20) {
		// Close and re-open file (basically seeking to frame 1)
		Close();
		Open();

		// Update overrides (since closing and re-opening might update these)
		info.has_audio = has_audio_override;
		info.has_video = has_video_override;

		// Not actually seeking, so clear these flags
		is_seeking = false;
		if (seek_count == 1) {
			// Don't redefine this on multiple seek attempts for a specific frame
			seeking_frame = 1;
			seeking_pts = ConvertFrameToVideoPTS(1);
		}
		seek_audio_frame_found = 0; // used to detect which frames to throw away after a seek
		seek_video_frame_found = 0; // used to detect which frames to throw away after a seek

	} else {
		// Seek to nearest key-frame (aka, i-frame)
		bool seek_worked = false;
		int64_t seek_target = 0;

		// Seek video stream (if any)
		if (!seek_worked && info.has_video) {
			seek_target = ConvertFrameToVideoPTS(requested_frame - buffer_amount);
			if (av_seek_frame(pFormatCtx, info.video_stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
				fprintf(stderr, "%s: error while seeking video stream\n", pFormatCtx->AV_FILENAME);
			} else {
				// VIDEO SEEK
				is_video_seek = true;
				seek_worked = true;
			}
		}

		// Seek audio stream (if not already seeked... and if an audio stream is found)
		if (!seek_worked && info.has_audio) {
			seek_target = ConvertFrameToAudioPTS(requested_frame - buffer_amount);
			if (av_seek_frame(pFormatCtx, info.audio_stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
				fprintf(stderr, "%s: error while seeking audio stream\n", pFormatCtx->AV_FILENAME);
			} else {
				// AUDIO SEEK
				is_video_seek = false;
				seek_worked = true;
			}
		}

		// Was the seek successful?
		if (seek_worked) {
			// Flush audio buffer
			if (info.has_audio)
				avcodec_flush_buffers(aCodecCtx);

			// Flush video buffer
			if (info.has_video)
				avcodec_flush_buffers(pCodecCtx);

			// Reset previous audio location to zero
			previous_packet_location.frame = -1;
			previous_packet_location.sample_start = 0;

			// init seek flags
			is_seeking = true;
			if (seek_count == 1) {
				// Don't redefine this on multiple seek attempts for a specific frame
				seeking_pts = seek_target;
				seeking_frame = requested_frame;
			}
			seek_audio_frame_found = 0; // used to detect which frames to throw away after a seek
			seek_video_frame_found = 0; // used to detect which frames to throw away after a seek

		} else {
			// seek failed
			is_seeking = false;
			seeking_pts = 0;
			seeking_frame = 0;

			// dislable seeking for this reader (since it failed)
			// TODO: Find a safer way to do this... not sure how common it is for a seek to fail.
			enable_seek = false;

			// Close and re-open file (basically seeking to frame 1)
			Close();
			Open();

			// Update overrides (since closing and re-opening might update these)
			info.has_audio = has_audio_override;
			info.has_video = has_video_override;
		}
	}
}

// Get the PTS for the current video packet
int64_t FFmpegReader::GetVideoPTS() {
	int64_t current_pts = 0;
	if (packet->dts != AV_NOPTS_VALUE)
		current_pts = packet->dts;

	// Return adjusted PTS
	return current_pts;
}

// Update PTS Offset (if any)
void FFmpegReader::UpdatePTSOffset(bool is_video) {
	// Determine the offset between the PTS and Frame number (only for 1st frame)
	if (is_video) {
		// VIDEO PACKET
		if (video_pts_offset == 99999) // Has the offset been set yet?
		{
			// Find the difference between PTS and frame number (no more than 10 timebase units allowed)
			video_pts_offset = 0 - std::max(GetVideoPTS(), (int64_t) info.video_timebase.ToInt() * 10);

			// debug output
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::UpdatePTSOffset (Video)", "video_pts_offset", video_pts_offset, "is_video", is_video);
		}
	} else {
		// AUDIO PACKET
		if (audio_pts_offset == 99999) // Has the offset been set yet?
		{
			// Find the difference between PTS and frame number (no more than 10 timebase units allowed)
			audio_pts_offset = 0 - std::max(packet->pts, (int64_t) info.audio_timebase.ToInt() * 10);

			// debug output
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::UpdatePTSOffset (Audio)", "audio_pts_offset", audio_pts_offset, "is_video", is_video);
		}
	}
}

// Convert PTS into Frame Number
int64_t FFmpegReader::ConvertVideoPTStoFrame(int64_t pts) {
	// Apply PTS offset
	pts = pts + video_pts_offset;
	int64_t previous_video_frame = current_video_frame;

	// Get the video packet start time (in seconds)
	double video_seconds = double(pts) * info.video_timebase.ToDouble();

	// Divide by the video timebase, to get the video frame number (frame # is decimal at this point)
	int64_t frame = round(video_seconds * info.fps.ToDouble()) + 1;

	// Keep track of the expected video frame #
	if (current_video_frame == 0)
		current_video_frame = frame;
	else {

		// Sometimes frames are duplicated due to identical (or similar) timestamps
		if (frame == previous_video_frame) {
			// return -1 frame number
			frame = -1;
		} else {
			// Increment expected frame
			current_video_frame++;
		}

		if (current_video_frame < frame)
			// has missing frames
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ConvertVideoPTStoFrame (detected missing frame)", "calculated frame", frame, "previous_video_frame", previous_video_frame, "current_video_frame", current_video_frame);

		// Sometimes frames are missing due to varying timestamps, or they were dropped. Determine
		// if we are missing a video frame.
		const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
		while (current_video_frame < frame) {
			if (!missing_video_frames.count(current_video_frame)) {
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ConvertVideoPTStoFrame (tracking missing frame)", "current_video_frame", current_video_frame, "previous_video_frame", previous_video_frame);
				missing_video_frames.insert(std::pair<int64_t, int64_t>(current_video_frame, previous_video_frame));
				missing_video_frames_source.insert(std::pair<int64_t, int64_t>(previous_video_frame, current_video_frame));
			}

			// Mark this reader as containing missing frames
			has_missing_frames = true;

			// Increment current frame
			current_video_frame++;
		}
	}

	// Return frame #
	return frame;
}

// Convert Frame Number into Video PTS
int64_t FFmpegReader::ConvertFrameToVideoPTS(int64_t frame_number) {
	// Get timestamp of this frame (in seconds)
	double seconds = double(frame_number) / info.fps.ToDouble();

	// Calculate the # of video packets in this timestamp
	int64_t video_pts = round(seconds / info.video_timebase.ToDouble());

	// Apply PTS offset (opposite)
	return video_pts - video_pts_offset;
}

// Convert Frame Number into Video PTS
int64_t FFmpegReader::ConvertFrameToAudioPTS(int64_t frame_number) {
	// Get timestamp of this frame (in seconds)
	double seconds = double(frame_number) / info.fps.ToDouble();

	// Calculate the # of audio packets in this timestamp
	int64_t audio_pts = round(seconds / info.audio_timebase.ToDouble());

	// Apply PTS offset (opposite)
	return audio_pts - audio_pts_offset;
}

// Calculate Starting video frame and sample # for an audio PTS
AudioLocation FFmpegReader::GetAudioPTSLocation(int64_t pts) {
	// Apply PTS offset
	pts = pts + audio_pts_offset;

	// Get the audio packet start time (in seconds)
	double audio_seconds = double(pts) * info.audio_timebase.ToDouble();

	// Divide by the video timebase, to get the video frame number (frame # is decimal at this point)
	double frame = (audio_seconds * info.fps.ToDouble()) + 1;

	// Frame # as a whole number (no more decimals)
	int64_t whole_frame = int64_t(frame);

	// Remove the whole number, and only get the decimal of the frame
	double sample_start_percentage = frame - double(whole_frame);

	// Get Samples per frame
	int samples_per_frame = Frame::GetSamplesPerFrame(whole_frame, info.fps, info.sample_rate, info.channels);

	// Calculate the sample # to start on
	int sample_start = round(double(samples_per_frame) * sample_start_percentage);

	// Protect against broken (i.e. negative) timestamps
	if (whole_frame < 1)
		whole_frame = 1;
	if (sample_start < 0)
		sample_start = 0;

	// Prepare final audio packet location
	AudioLocation location = {whole_frame, sample_start};

	// Compare to previous audio packet (and fix small gaps due to varying PTS timestamps)
	if (previous_packet_location.frame != -1) {
		if (location.is_near(previous_packet_location, samples_per_frame, samples_per_frame)) {
			int64_t orig_frame = location.frame;
			int orig_start = location.sample_start;

			// Update sample start, to prevent gaps in audio
			location.sample_start = previous_packet_location.sample_start;
			location.frame = previous_packet_location.frame;

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAudioPTSLocation (Audio Gap Detected)", "Source Frame", orig_frame, "Source Audio Sample", orig_start, "Target Frame", location.frame, "Target Audio Sample", location.sample_start, "pts", pts);

		} else {
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAudioPTSLocation (Audio Gap Ignored - too big)", "Previous location frame", previous_packet_location.frame, "Target Frame", location.frame, "Target Audio Sample", location.sample_start, "pts", pts);

			const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
			for (int64_t audio_frame = previous_packet_location.frame; audio_frame < location.frame; audio_frame++) {
				if (!missing_audio_frames.count(audio_frame)) {
					ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAudioPTSLocation (tracking missing frame)", "missing_audio_frame", audio_frame, "previous_audio_frame", previous_packet_location.frame, "new location frame", location.frame);
					missing_audio_frames.insert(std::pair<int64_t, int64_t>(audio_frame, previous_packet_location.frame - 1));
				}
			}
		}
	}

	// Set previous location
	previous_packet_location = location;

	// Return the associated video frame and starting sample #
	return location;
}

// Create a new Frame (or return an existing one) and add it to the working queue.
std::shared_ptr<Frame> FFmpegReader::CreateFrame(int64_t requested_frame) {
	// Check working cache
	std::shared_ptr<Frame> output = working_cache.GetFrame(requested_frame);

	if (!output) {
		// Lock
		const GenericScopedLock <CriticalSection> lock(processingCriticalSection);

		// (re-)Check working cache
		output = working_cache.GetFrame(requested_frame);
		if(output) return output;

		// Create a new frame on the working cache
		output = std::make_shared<Frame>(requested_frame, info.width, info.height, "#000000", Frame::GetSamplesPerFrame(requested_frame, info.fps, info.sample_rate, info.channels), info.channels);
		output->SetPixelRatio(info.pixel_ratio.num, info.pixel_ratio.den); // update pixel ratio
		output->ChannelsLayout(info.channel_layout); // update audio channel layout from the parent reader
		output->SampleRate(info.sample_rate); // update the frame's sample rate of the parent reader

		working_cache.Add(output);

		// Set the largest processed frame (if this is larger)
		if (requested_frame > largest_frame_processed)
			largest_frame_processed = requested_frame;
	}
	// Return frame
	return output;
}

// Determine if frame is partial due to seek
bool FFmpegReader::IsPartialFrame(int64_t requested_frame) {

	// Sometimes a seek gets partial frames, and we need to remove them
	bool seek_trash = false;
	int64_t max_seeked_frame = seek_audio_frame_found; // determine max seeked frame
	if (seek_video_frame_found > max_seeked_frame) {
		max_seeked_frame = seek_video_frame_found;
	}
	if ((info.has_audio && seek_audio_frame_found && max_seeked_frame >= requested_frame) ||
		(info.has_video && seek_video_frame_found && max_seeked_frame >= requested_frame)) {
		seek_trash = true;
	}

	return seek_trash;
}

// Check if a frame is missing and attempt to replace its frame image (and
bool FFmpegReader::CheckMissingFrame(int64_t requested_frame) {
	// Lock
	const GenericScopedLock <CriticalSection> lock(processingCriticalSection);

	// Increment check count for this frame (or init to 1)
	++checked_frames[requested_frame];

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckMissingFrame", "requested_frame", requested_frame, "has_missing_frames", has_missing_frames, "missing_video_frames.size()", missing_video_frames.size(), "checked_count", checked_frames[requested_frame]);

	// Missing frames (sometimes frame #'s are skipped due to invalid or missing timestamps)
	std::map<int64_t, int64_t>::iterator itr;
	bool found_missing_frame = false;

	// Special MP3 Handling (ignore more than 1 video frame)
	if (info.has_audio and info.has_video) {
		AVCodecID aCodecId = AV_FIND_DECODER_CODEC_ID(aStream);
		AVCodecID vCodecId = AV_FIND_DECODER_CODEC_ID(pStream);
		// If MP3 with single video frame, handle this special case by copying the previously
		// decoded image to the new frame. Otherwise, it will spend a huge amount of
		// CPU time looking for missing images for all the audio-only frames.
		if (checked_frames[requested_frame] > 8 && !missing_video_frames.count(requested_frame) &&
			!processing_audio_frames.count(requested_frame) && processed_audio_frames.count(requested_frame) &&
			last_frame && last_video_frame && last_video_frame->has_image_data && aCodecId == AV_CODEC_ID_MP3 && (vCodecId == AV_CODEC_ID_MJPEGB || vCodecId == AV_CODEC_ID_MJPEG)) {
			missing_video_frames.insert(std::pair<int64_t, int64_t>(requested_frame, last_video_frame->number));
			missing_video_frames_source.insert(std::pair<int64_t, int64_t>(last_video_frame->number, requested_frame));
			missing_frames.Add(last_video_frame);
		}
	}

	// Check if requested video frame is a missing
	if (missing_video_frames.count(requested_frame)) {
		int64_t missing_source_frame = missing_video_frames.find(requested_frame)->second;

		// Increment missing source frame check count (or init to 1)
		++checked_frames[missing_source_frame];

		// Get the previous frame of this missing frame (if it's available in missing cache)
		std::shared_ptr<Frame> parent_frame = missing_frames.GetFrame(missing_source_frame);
		if (parent_frame == NULL) {
			parent_frame = final_cache.GetFrame(missing_source_frame);
			if (parent_frame != NULL) {
				// Add missing final frame to missing cache
				missing_frames.Add(parent_frame);
			}
		}

		// Create blank missing frame
		std::shared_ptr<Frame> missing_frame = CreateFrame(requested_frame);

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckMissingFrame (Is Previous Video Frame Final)", "requested_frame", requested_frame, "missing_frame->number", missing_frame->number, "missing_source_frame", missing_source_frame);

		// If previous frame found, copy image from previous to missing frame (else we'll just wait a bit and try again later)
		if (parent_frame != NULL) {
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckMissingFrame (AddImage from Previous Video Frame)", "requested_frame", requested_frame, "missing_frame->number", missing_frame->number, "missing_source_frame", missing_source_frame);

			// Add this frame to the processed map (since it's already done)
			std::shared_ptr<QImage> parent_image = parent_frame->GetImage();
			if (parent_image) {
				missing_frame->AddImage(std::shared_ptr<QImage>(new QImage(*parent_image)));
				processed_video_frames[missing_frame->number] = missing_frame->number;
			}
		}
	}

	// Check if requested audio frame is a missing
	if (missing_audio_frames.count(requested_frame)) {

		// Create blank missing frame
		std::shared_ptr<Frame> missing_frame = CreateFrame(requested_frame);

		// Get Samples per frame (for this frame number)
		int samples_per_frame = Frame::GetSamplesPerFrame(missing_frame->number, info.fps, info.sample_rate, info.channels);

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckMissingFrame (Add Silence for Missing Audio Frame)", "requested_frame", requested_frame, "missing_frame->number", missing_frame->number, "samples_per_frame", samples_per_frame);

		// Add this frame to the processed map (since it's already done)
		missing_frame->AddAudioSilence(samples_per_frame);
		processed_audio_frames[missing_frame->number] = missing_frame->number;
	}

	return found_missing_frame;
}

// Check the working queue, and move finished frames to the finished queue
void FFmpegReader::CheckWorkingFrames(bool end_of_stream, int64_t requested_frame) {
	// Loop through all working queue frames
	bool checked_count_tripped = false;
	int max_checked_count = 80;

	// Check if requested frame is 'missing'
	CheckMissingFrame(requested_frame);

	while (true) {
		// Get the front frame of working cache
		std::shared_ptr<Frame> f(working_cache.GetSmallestFrame());

		// Was a frame found?
		if (!f)
			// No frames found
			break;

		// Remove frames which are too old
		if (f && f->number < (requested_frame - (OPEN_MP_NUM_PROCESSORS * 2))) {
			working_cache.Remove(f->number);
		}

		// Check if this frame is 'missing'
		CheckMissingFrame(f->number);

		// Init # of times this frame has been checked so far
		int checked_count = 0;
		int checked_frames_size = 0;

		bool is_video_ready = false;
		bool is_audio_ready = false;
		{ // limit scope of next few lines
			const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
			is_video_ready = processed_video_frames.count(f->number);
			is_audio_ready = processed_audio_frames.count(f->number);

			// Get check count for this frame
			checked_frames_size = checked_frames.size();
			if (!checked_count_tripped || f->number >= requested_frame)
				checked_count = checked_frames[f->number];
			else
				// Force checked count over the limit
				checked_count = max_checked_count;
		}

		if (previous_packet_location.frame == f->number && !end_of_stream)
			is_audio_ready = false; // don't finalize the last processed audio frame
		bool is_seek_trash = IsPartialFrame(f->number);

		// Adjust for available streams
		if (!info.has_video) is_video_ready = true;
		if (!info.has_audio) is_audio_ready = true;

		// Make final any frames that get stuck (for whatever reason)
		if (checked_count >= max_checked_count && (!is_video_ready || !is_audio_ready)) {
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckWorkingFrames (exceeded checked_count)", "requested_frame", requested_frame, "frame_number", f->number, "is_video_ready", is_video_ready, "is_audio_ready", is_audio_ready, "checked_count", checked_count, "checked_frames_size", checked_frames_size);

			// Trigger checked count tripped mode (clear out all frames before requested frame)
			checked_count_tripped = true;

			if (info.has_video && !is_video_ready && last_video_frame) {
				// Copy image from last frame
				f->AddImage(std::shared_ptr<QImage>(new QImage(*last_video_frame->GetImage())));
				is_video_ready = true;
			}

			if (info.has_audio && !is_audio_ready) {
				// Mark audio as processed, and indicate the frame has audio data
				is_audio_ready = true;
			}
		}

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckWorkingFrames", "requested_frame", requested_frame, "frame_number", f->number, "is_video_ready", is_video_ready, "is_audio_ready", is_audio_ready, "checked_count", checked_count, "checked_frames_size", checked_frames_size);

		// Check if working frame is final
		if ((!end_of_stream && is_video_ready && is_audio_ready) || end_of_stream || is_seek_trash) {
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckWorkingFrames (mark frame as final)", "requested_frame", requested_frame, "f->number", f->number, "is_seek_trash", is_seek_trash, "Working Cache Count", working_cache.Count(), "Final Cache Count", final_cache.Count(), "end_of_stream", end_of_stream);

			if (!is_seek_trash) {
				// Add missing image (if needed - sometimes end_of_stream causes frames with only audio)
				if (info.has_video && !is_video_ready && last_video_frame)
					// Copy image from last frame
					f->AddImage(std::shared_ptr<QImage>(new QImage(*last_video_frame->GetImage())));

				// Reset counter since last 'final' frame
				num_checks_since_final = 0;

				// Move frame to final cache
				final_cache.Add(f);

				// Add to missing cache (if another frame depends on it)
				{
					const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
					if (missing_video_frames_source.count(f->number)) {
						// Debug output
						ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckWorkingFrames (add frame to missing cache)", "f->number", f->number, "is_seek_trash", is_seek_trash, "Missing Cache Count", missing_frames.Count(), "Working Cache Count", working_cache.Count(), "Final Cache Count", final_cache.Count());
						missing_frames.Add(f);
					}

					// Remove from 'checked' count
					checked_frames.erase(f->number);
				}

				// Remove frame from working cache
				working_cache.Remove(f->number);

				// Update last frame processed
				last_frame = f->number;

			} else {
				// Seek trash, so delete the frame from the working cache, and never add it to the final cache.
				working_cache.Remove(f->number);
			}

		} else {
			// Stop looping
			break;
		}
	}
}

// Check for the correct frames per second (FPS) value by scanning the 1st few seconds of video packets.
void FFmpegReader::CheckFPS() {
	check_fps = true;


	int first_second_counter = 0;
	int second_second_counter = 0;
	int third_second_counter = 0;
	int forth_second_counter = 0;
	int fifth_second_counter = 0;
	int frames_detected = 0;
	int64_t pts = 0;

	// Loop through the stream
	while (true) {
		// Get the next packet (if any)
		if (GetNextPacket() < 0)
			// Break loop when no more packets found
			break;

		// Video packet
		if (packet->stream_index == videoStream) {
			// Check if the AVFrame is finished and set it
			if (GetAVFrame()) {
				// Update PTS / Frame Offset (if any)
				UpdatePTSOffset(true);

				// Get PTS of this packet
				pts = GetVideoPTS();

				// Remove pFrame
				RemoveAVFrame(pFrame);

				// Apply PTS offset
				pts += video_pts_offset;

				// Get the video packet start time (in seconds)
				double video_seconds = double(pts) * info.video_timebase.ToDouble();

				// Increment the correct counter
				if (video_seconds <= 1.0)
					first_second_counter++;
				else if (video_seconds > 1.0 && video_seconds <= 2.0)
					second_second_counter++;
				else if (video_seconds > 2.0 && video_seconds <= 3.0)
					third_second_counter++;
				else if (video_seconds > 3.0 && video_seconds <= 4.0)
					forth_second_counter++;
				else if (video_seconds > 4.0 && video_seconds <= 5.0)
					fifth_second_counter++;

				// Increment counters
				frames_detected++;
			}
		}
	}

	// Double check that all counters have greater than zero (or give up)
	if (second_second_counter != 0 && third_second_counter != 0 && forth_second_counter != 0 && fifth_second_counter != 0) {
		// Calculate average FPS (average of first few seconds)
		int sum_fps = second_second_counter + third_second_counter + forth_second_counter + fifth_second_counter;
		int avg_fps = round(sum_fps / 4.0f);

		// Update FPS
		info.fps = Fraction(avg_fps, 1);

		// Update Duration and Length
		info.video_length = frames_detected;
		info.duration = frames_detected / (sum_fps / 4.0f);

		// Update video bit rate
		info.video_bit_rate = info.file_size / info.duration;
	} else if (second_second_counter != 0 && third_second_counter != 0) {
		// Calculate average FPS (only on second 2)
		int sum_fps = second_second_counter;

		// Update FPS
		info.fps = Fraction(sum_fps, 1);

		// Update Duration and Length
		info.video_length = frames_detected;
		info.duration = frames_detected / float(sum_fps);

		// Update video bit rate
		info.video_bit_rate = info.file_size / info.duration;
	} else {
		// Too short to determine framerate, just default FPS
		// Set a few important default video settings (so audio can be divided into frames)
		info.fps.num = 30;
		info.fps.den = 1;

		// Calculate number of frames
		info.video_length = frames_detected;
		info.duration = frames_detected / info.fps.ToFloat();
	}
}

// Remove AVFrame from cache (and deallocate its memory)
void FFmpegReader::RemoveAVFrame(AVFrame *remove_frame) {
	// Remove pFrame (if exists)
	if (remove_frame) {
		// Free memory
#pragma omp critical (packet_cache)
		{
			av_freep(&remove_frame->data[0]);
#ifndef WIN32
			AV_FREE_FRAME(&remove_frame);
#endif
		}
	}
}

// Remove AVPacket from cache (and deallocate its memory)
void FFmpegReader::RemoveAVPacket(AVPacket *remove_packet) {
	// deallocate memory for packet
	AV_FREE_PACKET(remove_packet);

	// Delete the object
	delete remove_packet;
}

/// Get the smallest video frame that is still being processed
int64_t FFmpegReader::GetSmallestVideoFrame() {
	// Loop through frame numbers
	std::map<int64_t, int64_t>::iterator itr;
	int64_t smallest_frame = -1;
	const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
	for (itr = processing_video_frames.begin(); itr != processing_video_frames.end(); ++itr) {
		if (itr->first < smallest_frame || smallest_frame == -1)
			smallest_frame = itr->first;
	}

	// Return frame number
	return smallest_frame;
}

/// Get the smallest audio frame that is still being processed
int64_t FFmpegReader::GetSmallestAudioFrame() {
	// Loop through frame numbers
	std::map<int64_t, int64_t>::iterator itr;
	int64_t smallest_frame = -1;
	const GenericScopedLock <CriticalSection> lock(processingCriticalSection);
	for (itr = processing_audio_frames.begin(); itr != processing_audio_frames.end(); ++itr) {
		if (itr->first < smallest_frame || smallest_frame == -1)
			smallest_frame = itr->first;
	}

	// Return frame number
	return smallest_frame;
}

// Generate JSON string of this object
std::string FFmpegReader::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value FFmpegReader::JsonValue() const {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "FFmpegReader";
	root["path"] = path;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void FFmpegReader::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try {
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e) {
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void FFmpegReader::SetJsonValue(const Json::Value root) {

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["path"].isNull())
		path = root["path"].asString();

	// Re-Open path, and re-init everything (if needed)
	if (is_open) {
		Close();
		Open();
	}
}
