/**
 * @file
 * @brief Source file for FFmpegReader class
 * @author Jonathan Thomas <jonathan@openshot.org>, Fabrice Bellard
 *
 * This file is originally based on the Libavformat API example, and then modified
 * by the libopenshot project.
 *
 * @ref License
 */

// Copyright (c) 2008-2024 OpenShot Studios, LLC, Fabrice Bellard
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <thread>	// for std::this_thread::sleep_for
#include <chrono>	// for std::chrono::milliseconds
#include <unistd.h>

#include "FFmpegUtilities.h"

#include "FFmpegReader.h"
#include "Exceptions.h"
#include "Timeline.h"
#include "ZmqLogger.h"

#define ENABLE_VAAPI 0

#if USE_HW_ACCEL
#define MAX_SUPPORTED_WIDTH 1950
#define MAX_SUPPORTED_HEIGHT 1100

#if ENABLE_VAAPI
#include "libavutil/hwcontext_vaapi.h"

typedef struct VAAPIDecodeContext {
	 VAProfile va_profile;
	 VAEntrypoint va_entrypoint;
	 VAConfigID va_config;
	 VAContextID va_context;

#if FF_API_STRUCT_VAAPI_CONTEXT
	// FF_DISABLE_DEPRECATION_WARNINGS
	int have_old_context;
	struct vaapi_context *old_context;
	AVBufferRef *device_ref;
	// FF_ENABLE_DEPRECATION_WARNINGS
#endif

	 AVHWDeviceContext *device;
	 AVVAAPIDeviceContext *hwctx;

	 AVHWFramesContext *frames;
	 AVVAAPIFramesContext *hwfc;

	 enum AVPixelFormat surface_format;
	 int surface_count;
 } VAAPIDecodeContext;
#endif // ENABLE_VAAPI
#endif // USE_HW_ACCEL


using namespace openshot;

int hw_de_on = 0;
#if USE_HW_ACCEL
	AVPixelFormat hw_de_av_pix_fmt_global = AV_PIX_FMT_NONE;
	AVHWDeviceType hw_de_av_device_type_global = AV_HWDEVICE_TYPE_NONE;
#endif

FFmpegReader::FFmpegReader(const std::string &path, bool inspect_reader)
		: last_frame(0), is_seeking(0), seeking_pts(0), seeking_frame(0), seek_count(0), NO_PTS_OFFSET(-99999),
		  path(path), is_video_seek(true), check_interlace(false), check_fps(false), enable_seek(true), is_open(false),
		  seek_audio_frame_found(0), seek_video_frame_found(0),is_duration_known(false), largest_frame_processed(0),
		  current_video_frame(0), packet(NULL), max_concurrent_frames(OPEN_MP_NUM_PROCESSORS), audio_pts(0),
		  video_pts(0), pFormatCtx(NULL), videoStream(-1), audioStream(-1), pCodecCtx(NULL), aCodecCtx(NULL),
		pStream(NULL), aStream(NULL), pFrame(NULL), previous_packet_location{-1,0},
		hold_packet(false) {

	// Initialize FFMpeg, and register all formats and codecs
	AV_REGISTER_ALL
	AVCODEC_REGISTER_ALL

	// Init timestamp offsets
	pts_offset_seconds = NO_PTS_OFFSET;
	video_pts_seconds = NO_PTS_OFFSET;
	audio_pts_seconds = NO_PTS_OFFSET;

	// Init cache
	working_cache.SetMaxBytesFromInfo(max_concurrent_frames * info.fps.ToDouble() * 2, info.width, info.height, info.sample_rate, info.channels);
	final_cache.SetMaxBytesFromInfo(max_concurrent_frames * 2, info.width, info.height, info.sample_rate, info.channels);

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

#if USE_HW_ACCEL

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
#endif // USE_HW_ACCEL

void FFmpegReader::Open() {
	// Open reader if not already open
	if (!is_open) {
		// Prevent async calls to the following code
		const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

		// Initialize format context
		pFormatCtx = NULL;
		{
			hw_de_on = (openshot::Settings::Instance()->HARDWARE_DECODER == 0 ? 0 : 1);
			ZmqLogger::Instance()->AppendDebugMethod("Decode hardware acceleration settings", "hw_de_on", hw_de_on, "HARDWARE_DECODER", openshot::Settings::Instance()->HARDWARE_DECODER);
		}

		// Open video file
		if (avformat_open_input(&pFormatCtx, path.c_str(), NULL, NULL) != 0)
			throw InvalidFile("File could not be opened.", path);

		// Retrieve stream information
		if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
			throw NoStreamsFound("No streams found in file.", path);

		videoStream = -1;
		audioStream = -1;

		// Init end-of-file detection variables
		packet_status.reset(true);

		// Loop through each stream, and identify the video and audio stream index
		for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
			// Is this a video stream?
			if (AV_GET_CODEC_TYPE(pFormatCtx->streams[i]) == AVMEDIA_TYPE_VIDEO && videoStream < 0) {
				videoStream = i;
				packet_status.video_eof = false;
				packet_status.packets_eof = false;
				packet_status.end_of_file = false;
			}
			// Is this an audio stream?
			if (AV_GET_CODEC_TYPE(pFormatCtx->streams[i]) == AVMEDIA_TYPE_AUDIO && audioStream < 0) {
				audioStream = i;
				packet_status.audio_eof = false;
				packet_status.packets_eof = false;
				packet_status.end_of_file = false;
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
			const AVCodecID codecId = AV_FIND_DECODER_CODEC_ID(pStream);

			// Get codec and codec context from stream
			const AVCodec *pCodec = avcodec_find_decoder(codecId);
			AVDictionary *opts = NULL;
			int retry_decode_open = 2;
			// If hw accel is selected but hardware cannot handle repeat with software decoding
			do {
				pCodecCtx = AV_GET_CODEC_CONTEXT(pStream, pCodec);
#if USE_HW_ACCEL
				if (hw_de_on && (retry_decode_open==2)) {
					// Up to here no decision is made if hardware or software decode
					hw_de_supported = IsHardwareDecodeSupported(pCodecCtx->codec_id);
				}
#endif
				retry_decode_open = 0;

				// Set number of threads equal to number of processors (not to exceed 16)
				pCodecCtx->thread_count = std::min(FF_VIDEO_NUM_PROCESSORS, 16);

				if (pCodec == NULL) {
					throw InvalidCodec("A valid video codec could not be found for this file.", path);
				}

				// Init options
				av_dict_set(&opts, "strict", "experimental", 0);
#if USE_HW_ACCEL
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
#endif // USE_HW_ACCEL

				// Disable per-frame threading for album arts
				// Using FF_THREAD_FRAME adds one frame decoding delay per thread,
				// but there's only one frame in this case.
				if (HasAlbumArt())
				{
					pCodecCtx->thread_type &= ~FF_THREAD_FRAME;
				}

				// Open video codec
				int avcodec_return = avcodec_open2(pCodecCtx, pCodec, &opts);
				if (avcodec_return < 0) {
					std::stringstream avcodec_error_msg;
					avcodec_error_msg << "A video codec was found, but could not be opened. Error: " << av_err2string(avcodec_return);
					throw InvalidCodec(avcodec_error_msg.str(), path);
				}

#if USE_HW_ACCEL
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
#endif // USE_HW_ACCEL
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
			const AVCodec *aCodec = avcodec_find_decoder(codecId);
			aCodecCtx = AV_GET_CODEC_CONTEXT(aStream, aCodec);

			// Audio encoding does not typically use more than 2 threads (most codecs use 1 thread)
			aCodecCtx->thread_count = std::min(FF_AUDIO_NUM_PROCESSORS, 2);

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

		// Process video stream side data (rotation, spherical metadata, etc)
		for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
			AVStream* st = pFormatCtx->streams[i];
			if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
				// Only inspect the first video stream
				for (int j = 0; j < st->nb_side_data; j++) {
					AVPacketSideData *sd = &st->side_data[j];

					// Handle rotation metadata (unchanged)
					if (sd->type == AV_PKT_DATA_DISPLAYMATRIX &&
						sd->size >= 9 * sizeof(int32_t) &&
						!info.metadata.count("rotate"))
					{
						double rotation = -av_display_rotation_get(
							reinterpret_cast<int32_t *>(sd->data));
						if (std::isnan(rotation)) rotation = 0;
						info.metadata["rotate"] = std::to_string(rotation);
					}
					// Handle spherical video metadata
					else if (sd->type == AV_PKT_DATA_SPHERICAL) {
						// Always mark as spherical
						info.metadata["spherical"] = "1";

						// Cast the raw bytes to an AVSphericalMapping
						const AVSphericalMapping* map =
							reinterpret_cast<const AVSphericalMapping*>(sd->data);

						// Projection enum → string
						const char* proj_name = av_spherical_projection_name(map->projection);
						info.metadata["spherical_projection"] = proj_name
							? proj_name
							: "unknown";

						// Convert 16.16 fixed-point to float degrees
						auto to_deg = [](int32_t v){
							return (double)v / 65536.0;
						};
						info.metadata["spherical_yaw"]   = std::to_string(to_deg(map->yaw));
						info.metadata["spherical_pitch"] = std::to_string(to_deg(map->pitch));
						info.metadata["spherical_roll"]  = std::to_string(to_deg(map->roll));
					}
				}
				break;
			}
		}

		// Init previous audio location to zero
		previous_packet_location.frame = -1;
		previous_packet_location.sample_start = 0;

		// Adjust cache size based on size of frame and audio
		working_cache.SetMaxBytesFromInfo(max_concurrent_frames * info.fps.ToDouble() * 2, info.width, info.height, info.sample_rate, info.channels);
		final_cache.SetMaxBytesFromInfo(max_concurrent_frames * 2, info.width, info.height, info.sample_rate, info.channels);

		// Scan PTS for any offsets (i.e. non-zero starting streams). At least 1 stream must start at zero timestamp.
		// This method allows us to shift timestamps to ensure at least 1 stream is starting at zero.
		UpdatePTSOffset();

		// Override an invalid framerate
		if (info.fps.ToFloat() > 240.0f || (info.fps.num <= 0 || info.fps.den <= 0) || info.video_length <= 0) {
			// Calculate FPS, duration, video bit rate, and video length manually
			// by scanning through all the video stream packets
			CheckFPS();
		}

		// Mark as "open"
		is_open = true;

		// Seek back to beginning of file (if not already seeking)
		if (!is_seeking) {
			Seek(1);
		}
	}
}

void FFmpegReader::Close() {
	// Close all objects, if reader is 'open'
	if (is_open) {
		// Prevent async calls to the following code
		const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

		// Mark as "closed"
		is_open = false;

		// Keep track of most recent packet
		AVPacket *recent_packet = packet;

		// Drain any packets from the decoder
		packet = NULL;
		int attempts = 0;
		int max_attempts = 128;
		while (packet_status.packets_decoded() < packet_status.packets_read() && attempts < max_attempts) {
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::Close (Drain decoder loop)",
													 "packets_read", packet_status.packets_read(),
													 "packets_decoded", packet_status.packets_decoded(),
													 "attempts", attempts);
			if (packet_status.video_decoded < packet_status.video_read) {
				ProcessVideoPacket(info.video_length);
			}
			if (packet_status.audio_decoded < packet_status.audio_read) {
				ProcessAudioPacket(info.video_length);
			}
			attempts++;
		}

		// Remove packet
		if (recent_packet) {
			RemoveAVPacket(recent_packet);
		}

		// Close the video codec
		if (info.has_video) {
			if(avcodec_is_open(pCodecCtx)) {
				avcodec_flush_buffers(pCodecCtx);
			}
			AV_FREE_CONTEXT(pCodecCtx);
#if USE_HW_ACCEL
			if (hw_de_on) {
				if (hw_device_ctx) {
					av_buffer_unref(&hw_device_ctx);
					hw_device_ctx = NULL;
				}
			}
#endif // USE_HW_ACCEL
			if (img_convert_ctx) {
				sws_freeContext(img_convert_ctx);
				img_convert_ctx = nullptr;
			}
			if (pFrameRGB_cached) {
				AV_FREE_FRAME(&pFrameRGB_cached);
			}
		}

		// Close the audio codec
		if (info.has_audio) {
			if(avcodec_is_open(aCodecCtx)) {
				avcodec_flush_buffers(aCodecCtx);
			}
			AV_FREE_CONTEXT(aCodecCtx);
			if (avr_ctx) {
				SWR_CLOSE(avr_ctx);
				SWR_FREE(&avr_ctx);
				avr_ctx = nullptr;
			}
		}

		// Clear final cache
		final_cache.Clear();
		working_cache.Clear();

		// Close the video file
		avformat_close_input(&pFormatCtx);
		av_freep(&pFormatCtx);

		// Reset some variables
		last_frame = 0;
		hold_packet = false;
		largest_frame_processed = 0;
		seek_audio_frame_found = 0;
		seek_video_frame_found = 0;
		current_video_frame = 0;
		last_video_frame.reset();
	}
}

bool FFmpegReader::HasAlbumArt() {
	// Check if the video stream we use is an attached picture
	// This won't return true if the file has a cover image as a secondary stream
	// like an MKV file with an attached image file
	return pFormatCtx && videoStream >= 0 && pFormatCtx->streams[videoStream]
		&& (pFormatCtx->streams[videoStream]->disposition & AV_DISPOSITION_ATTACHED_PIC);
}

void FFmpegReader::UpdateAudioInfo() {
	// Set default audio channel layout (if needed)
#if HAVE_CH_LAYOUT
	if (!av_channel_layout_check(&(AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->ch_layout)))
		AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->ch_layout = (AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO;
#else
	if (AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channel_layout == 0)
		AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channel_layout = av_get_default_channel_layout(AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channels);
#endif

	if (info.sample_rate > 0) {
		// Skip init - if info struct already populated
		return;
	}

	// Set values of FileInfo struct
	info.has_audio = true;
	info.file_size = pFormatCtx->pb ? avio_size(pFormatCtx->pb) : -1;
	info.acodec = aCodecCtx->codec->name;
#if HAVE_CH_LAYOUT
	info.channels = AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->ch_layout.nb_channels;
	info.channel_layout = (ChannelLayout) AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->ch_layout.u.mask;
#else
	info.channels = AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channels;
	info.channel_layout = (ChannelLayout) AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channel_layout;
#endif

	// If channel layout is not set, guess based on the number of channels
	if (info.channel_layout == 0) {
		if (info.channels == 1) {
			info.channel_layout = openshot::LAYOUT_MONO;
		} else if (info.channels == 2) {
			info.channel_layout = openshot::LAYOUT_STEREO;
		}
	}

	info.sample_rate = AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->sample_rate;
	info.audio_bit_rate = AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->bit_rate;
	if (info.audio_bit_rate <= 0) {
		// Get bitrate from format
		info.audio_bit_rate = pFormatCtx->bit_rate;
	}

	// Set audio timebase
	info.audio_timebase.num = aStream->time_base.num;
	info.audio_timebase.den = aStream->time_base.den;

	// Get timebase of audio stream (if valid) and greater than the current duration
	if (aStream->duration > 0 && aStream->duration > info.duration) {
		// Get duration from audio stream
		info.duration = aStream->duration * info.audio_timebase.ToDouble();
	} else if (pFormatCtx->duration > 0 && info.duration <= 0.0f) {
		// Use the format's duration
		info.duration = float(pFormatCtx->duration) / AV_TIME_BASE;
	}

	// Calculate duration from filesize and bitrate (if any)
	if (info.duration <= 0.0f && info.video_bit_rate > 0 && info.file_size > 0) {
		// Estimate from bitrate, total bytes, and framerate
		info.duration = float(info.file_size) / info.video_bit_rate;
	}

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

		// Use timeline to set correct width & height (if any)
		Clip *parent = static_cast<Clip *>(ParentClip());
		if (parent) {
			if (parent->ParentTimeline()) {
				// Set max width/height based on parent clip's timeline (if attached to a timeline)
				info.width = parent->ParentTimeline()->preview_width;
				info.height = parent->ParentTimeline()->preview_height;
			}
		}
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
	if (info.vcodec.length() > 0) {
		// Skip init - if info struct already populated
		return;
	}

	// Set values of FileInfo struct
	info.has_video = true;
	info.file_size = pFormatCtx->pb ? avio_size(pFormatCtx->pb) : -1;
	info.height = AV_GET_CODEC_ATTRIBUTES(pStream, pCodecCtx)->height;
	info.width = AV_GET_CODEC_ATTRIBUTES(pStream, pCodecCtx)->width;
	info.vcodec = pCodecCtx->codec->name;
	info.video_bit_rate = (pFormatCtx->bit_rate / 8);

	// Frame rate from the container and codec
	AVRational framerate = av_guess_frame_rate(pFormatCtx, pStream, NULL);
	if (!check_fps) {
		info.fps.num = framerate.num;
		info.fps.den = framerate.den;
	}

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
	if (info.duration <= 0.0f && pFormatCtx->duration >= 0) {
		// Use the format's duration
		info.duration = float(pFormatCtx->duration) / AV_TIME_BASE;
	}

	// Calculate duration from filesize and bitrate (if any)
	if (info.duration <= 0.0f && info.video_bit_rate > 0 && info.file_size > 0) {
		// Estimate from bitrate, total bytes, and framerate
		info.duration = float(info.file_size) / info.video_bit_rate;
	}

	// Certain "image" formats do not have a valid duration
	if (info.duration <= 0.0f && pStream->duration == AV_NOPTS_VALUE && pFormatCtx->duration == AV_NOPTS_VALUE) {
		// Force an "image" duration
		info.duration = 60 * 60 * 1;  // 1 hour duration
		info.video_length = 1;
		info.has_single_image = true;
	}

	// Get the # of video frames (if found in stream)
	// Only set this 1 time (this method can be called multiple times)
	if (pStream->nb_frames > 0 && info.video_length <= 0)
	{
		info.video_length = pStream->nb_frames;

		// If the file format is animated GIF, override the video_length to be (duration * fps) rounded.
		if (pFormatCtx && pFormatCtx->iformat && strcmp(pFormatCtx->iformat->name, "gif") == 0)
		{
			if (pStream->nb_frames > 1) {
				// Animated gif (nb_frames does not take into delays and gaps)
				info.video_length = round(info.duration * info.fps.ToDouble());
			} else {
				// Static non-animated gif (set a default duration)
				info.duration = 10.0;
			}
		}
	}

	// No duration found in stream of file
	if (info.duration <= 0.0f) {
		// No duration is found in the video stream
		info.duration = -1;
		info.video_length = -1;
		is_duration_known = false;
	} else {
		// Yes, a duration was found
		is_duration_known = true;

		// Calculate number of frames (if not already found in metadata)
		// Only set this 1 time (this method can be called multiple times)
		if (info.video_length <= 0) {
			info.video_length = round(info.duration * info.fps.ToDouble());
		}
	}

	// Add video metadata (if any)
	AVDictionaryEntry *tag = NULL;
	while ((tag = av_dict_get(pStream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
		QString str_key = tag->key;
		QString str_value = tag->value;
		info.metadata[str_key.toStdString()] = str_value.trimmed().toStdString();
	}
}

bool FFmpegReader::GetIsDurationKnown() {
	return this->is_duration_known;
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

		// Prevent async calls to the remainder of this code
		const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

		// Check the cache a 2nd time (due to the potential previous lock)
		frame = final_cache.GetFrame(requested_frame);
		if (frame) {
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetFrame", "returned cached frame on 2nd look", requested_frame);

		} else {
			// Frame is not in cache
			// Reset seek count
			seek_count = 0;

			// Are we within X frames of the requested frame?
			int64_t diff = requested_frame - last_frame;
			if (diff >= 1 && diff <= 20) {
				// Continue walking the stream
				frame = ReadStream(requested_frame);
			} else {
				// Greater than 30 frames away, or backwards, we need to seek to the nearest key frame
				if (enable_seek) {
					// Only seek if enabled
					Seek(requested_frame);

				} else if (!enable_seek && diff < 0) {
					// Start over, since we can't seek, and the requested frame is smaller than our position
					// Since we are seeking to frame 1, this actually just closes/re-opens the reader
					Seek(1);
				}

				// Then continue walking the stream
				frame = ReadStream(requested_frame);
			}
		}
		return frame;
	}
}

// Read the stream until we find the requested Frame
std::shared_ptr<Frame> FFmpegReader::ReadStream(int64_t requested_frame) {
	// Allocate video frame
	bool check_seek = false;
	int packet_error = -1;

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ReadStream", "requested_frame", requested_frame, "max_concurrent_frames", max_concurrent_frames);

	// Loop through the stream until the correct frame is found
	while (true) {
		// Check if working frames are 'finished'
		if (!is_seeking) {
			// Check for final frames
			CheckWorkingFrames(requested_frame);
		}

		// Check if requested 'final' frame is available (and break out of loop if found)
		bool is_cache_found = (final_cache.GetFrame(requested_frame) != NULL);
		if (is_cache_found) {
			break;
		}

		if (!hold_packet || !packet) {
			// Get the next packet
			packet_error = GetNextPacket();
			if (packet_error < 0 && !packet) {
				// No more packets to be found
				packet_status.packets_eof = true;
			}
		}

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ReadStream (GetNextPacket)", "requested_frame", requested_frame,"packets_read", packet_status.packets_read(), "packets_decoded", packet_status.packets_decoded(), "is_seeking", is_seeking);

		// Check the status of a seek (if any)
		if (is_seeking) {
			check_seek = CheckSeek(false);
		} else {
			check_seek = false;
		}

		if (check_seek) {
			// Packet may become NULL on Close inside Seek if CheckSeek returns false
			// Jump to the next iteration of this loop
			continue;
		}

		// Video packet
		if ((info.has_video && packet && packet->stream_index == videoStream) ||
			(info.has_video && packet_status.video_decoded < packet_status.video_read) ||
			(info.has_video && !packet && !packet_status.video_eof)) {
			// Process Video Packet
			ProcessVideoPacket(requested_frame);
		}
		// Audio packet
		if ((info.has_audio && packet && packet->stream_index == audioStream) ||
			(info.has_audio && !packet && packet_status.audio_decoded < packet_status.audio_read) ||
			(info.has_audio && !packet && !packet_status.audio_eof)) {
			// Process Audio Packet
			ProcessAudioPacket(requested_frame);
		}

		// Remove unused packets (sometimes we purposely ignore video or audio packets,
		// if the has_video or has_audio properties are manually overridden)
		if ((!info.has_video && packet && packet->stream_index == videoStream) ||
			(!info.has_audio && packet && packet->stream_index == audioStream)) {
			// Keep track of deleted packet counts
			if (packet->stream_index == videoStream) {
				packet_status.video_decoded++;
			} else if (packet->stream_index == audioStream) {
				packet_status.audio_decoded++;
			}

			// Remove unused packets (sometimes we purposely ignore video or audio packets,
			// if the has_video or has_audio properties are manually overridden)
			RemoveAVPacket(packet);
			packet = NULL;
		}

		// Determine end-of-stream (waiting until final decoder threads finish)
		// Force end-of-stream in some situations
		packet_status.end_of_file = packet_status.packets_eof && packet_status.video_eof && packet_status.audio_eof;
		if ((packet_status.packets_eof && packet_status.packets_read() == packet_status.packets_decoded()) || packet_status.end_of_file) {
			// Force EOF (end of file) variables to true, if decoder does not support EOF detection.
			// If we have no more packets, and all known packets have been decoded
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ReadStream (force EOF)", "packets_read", packet_status.packets_read(), "packets_decoded", packet_status.packets_decoded(), "packets_eof", packet_status.packets_eof, "video_eof", packet_status.video_eof, "audio_eof", packet_status.audio_eof, "end_of_file", packet_status.end_of_file);
			if (!packet_status.video_eof) {
				packet_status.video_eof = true;
			}
			if (!packet_status.audio_eof) {
				packet_status.audio_eof = true;
			}
			packet_status.end_of_file = true;
			break;
		}
	} // end while

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ReadStream (Completed)",
										  "packets_read", packet_status.packets_read(),
										  "packets_decoded", packet_status.packets_decoded(),
										  "end_of_file", packet_status.end_of_file,
										  "largest_frame_processed", largest_frame_processed,
										  "Working Cache Count", working_cache.Count());

	// Have we reached end-of-stream (or the final frame)?
	if (!packet_status.end_of_file && requested_frame >= info.video_length) {
		// Force end-of-stream
		packet_status.end_of_file = true;
	}
	if (packet_status.end_of_file) {
		// Mark any other working frames as 'finished'
		CheckWorkingFrames(requested_frame);
	}

	// Return requested frame (if found)
	std::shared_ptr<Frame> frame = final_cache.GetFrame(requested_frame);
	if (frame)
		// Return prepared frame
		return frame;
	else {

		// Check if largest frame is still cached
		frame = final_cache.GetFrame(largest_frame_processed);
		int samples_in_frame = Frame::GetSamplesPerFrame(requested_frame, info.fps,
														 info.sample_rate, info.channels);
		if (frame) {
			// Copy and return the largest processed frame (assuming it was the last in the video file)
			std::shared_ptr<Frame> f = CreateFrame(largest_frame_processed);

			// Use solid color (if no image data found)
			if (!frame->has_image_data) {
				// Use solid black frame if no image data available
				f->AddColor(info.width, info.height, "#000");
			}
			// Silence audio data (if any), since we are repeating the last frame
			frame->AddAudioSilence(samples_in_frame);

			return frame;
		} else {
			// The largest processed frame is no longer in cache, return a blank frame
			std::shared_ptr<Frame> f = CreateFrame(largest_frame_processed);
			f->AddColor(info.width, info.height, "#000");
			f->AddAudioSilence(samples_in_frame);
			return f;
		}
	}

}

// Get the next packet (if any)
int FFmpegReader::GetNextPacket() {
	int found_packet = 0;
	AVPacket *next_packet;
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

		// Keep track of packet stats
		if (packet->stream_index == videoStream) {
			packet_status.video_read++;
		} else if (packet->stream_index == audioStream) {
			packet_status.audio_read++;
		}
	} else {
		// No more packets found
		delete next_packet;
		packet = NULL;
	}
	// Return if packet was found (or error number)
	return found_packet;
}

// Get an AVFrame (if any)
bool FFmpegReader::GetAVFrame() {
	int frameFinished = 0;

	// Decode video frame
	AVFrame *next_frame = AV_ALLOCATE_FRAME();

#if IS_FFMPEG_3_2
	int send_packet_err = 0;
	int64_t send_packet_pts = 0;
	if ((packet && packet->stream_index == videoStream) || !packet) {
		send_packet_err = avcodec_send_packet(pCodecCtx, packet);

		if (packet && send_packet_err >= 0) {
			send_packet_pts = GetPacketPTS();
			hold_packet = false;
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (send packet succeeded)", "send_packet_err", send_packet_err, "send_packet_pts", send_packet_pts);
		}
	}

	#if USE_HW_ACCEL
		// Get the format from the variables set in get_hw_dec_format
		hw_de_av_pix_fmt = hw_de_av_pix_fmt_global;
		hw_de_av_device_type = hw_de_av_device_type_global;
	#endif // USE_HW_ACCEL
		if (send_packet_err < 0 && send_packet_err != AVERROR_EOF) {
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (send packet: Not sent [" + av_err2string(send_packet_err) + "])", "send_packet_err", send_packet_err, "send_packet_pts", send_packet_pts);
			if (send_packet_err == AVERROR(EAGAIN)) {
				hold_packet = true;
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (send packet: AVERROR(EAGAIN): user must read output with avcodec_receive_frame()", "send_packet_pts", send_packet_pts);
			}
			if (send_packet_err == AVERROR(EINVAL)) {
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (send packet: AVERROR(EINVAL): codec not opened, it is an encoder, or requires flush", "send_packet_pts", send_packet_pts);
			}
			if (send_packet_err == AVERROR(ENOMEM)) {
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (send packet: AVERROR(ENOMEM): failed to add packet to internal queue, or legitimate decoding errors", "send_packet_pts", send_packet_pts);
			}
		}

		// Always try and receive a packet, if not EOF.
		// Even if the above avcodec_send_packet failed to send,
		// we might still need to receive a packet.
		int receive_frame_err = 0;
		AVFrame *next_frame2;
#if USE_HW_ACCEL
		if (hw_de_on && hw_de_supported) {
			next_frame2 = AV_ALLOCATE_FRAME();
		}
		else
#endif // USE_HW_ACCEL
		{
			next_frame2 = next_frame;
		}
		pFrame = AV_ALLOCATE_FRAME();
		while (receive_frame_err >= 0) {
			receive_frame_err = avcodec_receive_frame(pCodecCtx, next_frame2);

			if (receive_frame_err != 0) {
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (receive frame: frame not ready yet from decoder [\" + av_err2string(receive_frame_err) + \"])", "receive_frame_err", receive_frame_err, "send_packet_pts", send_packet_pts);

				if (receive_frame_err == AVERROR_EOF) {
					ZmqLogger::Instance()->AppendDebugMethod(
							"FFmpegReader::GetAVFrame (receive frame: AVERROR_EOF: EOF detected from decoder, flushing buffers)", "send_packet_pts", send_packet_pts);
					avcodec_flush_buffers(pCodecCtx);
					packet_status.video_eof = true;
				}
				if (receive_frame_err == AVERROR(EINVAL)) {
					ZmqLogger::Instance()->AppendDebugMethod(
							"FFmpegReader::GetAVFrame (receive frame: AVERROR(EINVAL): invalid frame received, flushing buffers)", "send_packet_pts", send_packet_pts);
					avcodec_flush_buffers(pCodecCtx);
				}
				if (receive_frame_err == AVERROR(EAGAIN)) {
					ZmqLogger::Instance()->AppendDebugMethod(
							"FFmpegReader::GetAVFrame (receive frame: AVERROR(EAGAIN): output is not available in this state - user must try to send new input)", "send_packet_pts", send_packet_pts);
				}
				if (receive_frame_err == AVERROR_INPUT_CHANGED) {
					ZmqLogger::Instance()->AppendDebugMethod(
							"FFmpegReader::GetAVFrame (receive frame: AVERROR_INPUT_CHANGED: current decoded frame has changed parameters with respect to first decoded frame)", "send_packet_pts", send_packet_pts);
				}

				// Break out of decoding loop
				// Nothing ready for decoding yet
				break;
			}

#if USE_HW_ACCEL
			if (hw_de_on && hw_de_supported) {
				int err;
				if (next_frame2->format == hw_de_av_pix_fmt) {
					next_frame->format = AV_PIX_FMT_YUV420P;
					if ((err = av_hwframe_transfer_data(next_frame,next_frame2,0)) < 0) {
						ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (Failed to transfer data to output frame)", "hw_de_on", hw_de_on);
					}
					if ((err = av_frame_copy_props(next_frame,next_frame2)) < 0) {
						ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::GetAVFrame (Failed to copy props to output frame)", "hw_de_on", hw_de_on);
					}
				}
			}
			else
#endif // USE_HW_ACCEL
			{	// No hardware acceleration used -> no copy from GPU memory needed
				next_frame = next_frame2;
			}

			// TODO also handle possible further frames
			// Use only the first frame like avcodec_decode_video2
			frameFinished = 1;
			packet_status.video_decoded++;

			// Allocate image (align 32 for simd)
			if (AV_ALLOCATE_IMAGE(pFrame, (AVPixelFormat)(pStream->codecpar->format), info.width, info.height) <= 0) {
				throw OutOfMemory("Failed to allocate image buffer", path);
			}
			av_image_copy(pFrame->data, pFrame->linesize, (const uint8_t**)next_frame->data, next_frame->linesize,
										(AVPixelFormat)(pStream->codecpar->format), info.width, info.height);

			// Get display PTS from video frame, often different than packet->pts.
			// Sending packets to the decoder (i.e. packet->pts) is async,
			// and retrieving packets from the decoder (frame->pts) is async. In most decoders
			// sending and retrieving are separated by multiple calls to this method.
			if (next_frame->pts != AV_NOPTS_VALUE) {
				// This is the current decoded frame (and should be the pts used) for
				// processing this data
				video_pts = next_frame->pts;
			} else if (next_frame->pkt_dts != AV_NOPTS_VALUE) {
				// Some videos only set this timestamp (fallback)
				video_pts = next_frame->pkt_dts;
			}

			ZmqLogger::Instance()->AppendDebugMethod(
					"FFmpegReader::GetAVFrame (Successful frame received)", "video_pts", video_pts, "send_packet_pts", send_packet_pts);

			// break out of loop after each successful image returned
			break;
		}
#if USE_HW_ACCEL
		if (hw_de_on && hw_de_supported) {
			AV_FREE_FRAME(&next_frame2);
		}
	#endif // USE_HW_ACCEL
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
		int64_t max_seeked_frame = std::max(seek_audio_frame_found, seek_video_frame_found);

		// determine if we are "before" the requested frame
		if (max_seeked_frame >= seeking_frame) {
			// SEEKED TOO FAR
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckSeek (Too far, seek again)",
											"is_video_seek", is_video_seek,
											"max_seeked_frame", max_seeked_frame,
											"seeking_frame", seeking_frame,
											"seeking_pts", seeking_pts,
											"seek_video_frame_found", seek_video_frame_found,
											"seek_audio_frame_found", seek_audio_frame_found);

			// Seek again... to the nearest Keyframe
			Seek(seeking_frame - (10 * seek_count * seek_count));
		} else {
			// SEEK WORKED
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckSeek (Successful)",
											"is_video_seek", is_video_seek,
											"packet->pts", GetPacketPTS(),
											"seeking_pts", seeking_pts,
											"seeking_frame", seeking_frame,
											"seek_video_frame_found", seek_video_frame_found,
											"seek_audio_frame_found", seek_audio_frame_found);

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
	// Get the AVFrame from the current packet
	// This sets the video_pts to the correct timestamp
	int frame_finished = GetAVFrame();

	// Check if the AVFrame is finished and set it
	if (!frame_finished) {
		// No AVFrame decoded yet, bail out
		if (pFrame) {
			RemoveAVFrame(pFrame);
		}
		return;
	}

	// Calculate current frame #
	int64_t current_frame = ConvertVideoPTStoFrame(video_pts);

	// Track 1st video packet after a successful seek
	if (!seek_video_frame_found && is_seeking)
		seek_video_frame_found = current_frame;

	// Create or get the existing frame object. Requested frame needs to be created
	// in working_cache at least once. Seek can clear the working_cache, so we must
	// add the requested frame back to the working_cache here. If it already exists,
	// it will be moved to the top of the working_cache.
	working_cache.Add(CreateFrame(requested_frame));

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessVideoPacket (Before)", "requested_frame", requested_frame, "current_frame", current_frame);

	// Init some things local (for OpenMP)
	PixelFormat pix_fmt = AV_GET_CODEC_PIXEL_FORMAT(pStream, pCodecCtx);
	int height = info.height;
	int width = info.width;
	int64_t video_length = info.video_length;

	// Create or reuse a RGB Frame (since most videos are not in RGB, we must convert it)
	AVFrame *pFrameRGB = pFrameRGB_cached;
	if (!pFrameRGB) {
		pFrameRGB = AV_ALLOCATE_FRAME();
		if (pFrameRGB == nullptr)
			throw OutOfMemory("Failed to allocate frame buffer", path);
		pFrameRGB_cached = pFrameRGB;
	}
	AV_RESET_FRAME(pFrameRGB);
	uint8_t *buffer = nullptr;

	// Determine the max size of this source image (based on the timeline's size, the scaling mode,
	// and the scaling keyframes). This is a performance improvement, to keep the images as small as possible,
	// without losing quality. NOTE: We cannot go smaller than the timeline itself, or the add_layer timeline
	// method will scale it back to timeline size before scaling it smaller again. This needs to be fixed in
	// the future.
	int max_width = info.width;
	int max_height = info.height;

	Clip *parent = static_cast<Clip *>(ParentClip());
	if (parent) {
		if (parent->ParentTimeline()) {
			// Set max width/height based on parent clip's timeline (if attached to a timeline)
			max_width = parent->ParentTimeline()->preview_width;
			max_height = parent->ParentTimeline()->preview_height;
		}
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
			// Scale video to equivalent unscaled size
			// Since the preview window can change sizes, we want to always
			// scale against the ratio of original video size to timeline size
			float preview_ratio = 1.0;
			if (parent->ParentTimeline()) {
				Timeline *t = (Timeline *) parent->ParentTimeline();
				preview_ratio = t->preview_width / float(t->info.width);
			}
			float max_scale_x = parent->scale_x.GetMaxPoint().co.Y;
			float max_scale_y = parent->scale_y.GetMaxPoint().co.Y;
			max_width = info.width * max_scale_x * preview_ratio;
			max_height = info.height * max_scale_y * preview_ratio;
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
	const int bytes_per_pixel = 4;
	int raw_buffer_size = (width * height * bytes_per_pixel) + 128;

	// Aligned memory allocation (for speed)
	constexpr size_t ALIGNMENT = 32;  // AVX2
	int buffer_size = ((raw_buffer_size + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;
	buffer = (unsigned char*) aligned_malloc(buffer_size, ALIGNMENT);

	// Copy picture data from one AVFrame (or AVPicture) to another one.
	AV_COPY_PICTURE_DATA(pFrameRGB, buffer, PIX_FMT_RGBA, width, height);

	int scale_mode = SWS_FAST_BILINEAR;
	if (openshot::Settings::Instance()->HIGH_QUALITY_SCALING) {
		scale_mode = SWS_BICUBIC;
	}
	img_convert_ctx = sws_getCachedContext(img_convert_ctx, info.width, info.height, AV_GET_CODEC_PIXEL_FORMAT(pStream, pCodecCtx), width, height, PIX_FMT_RGBA, scale_mode, NULL, NULL, NULL);
	if (!img_convert_ctx)
		throw OutOfMemory("Failed to initialize sws context", path);

	// Resize / Convert to RGB
	sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0,
			  original_height, pFrameRGB->data, pFrameRGB->linesize);

	// Create or get the existing frame object
	std::shared_ptr<Frame> f = CreateFrame(current_frame);

	// Add Image data to frame
	if (!ffmpeg_has_alpha(AV_GET_CODEC_PIXEL_FORMAT(pStream, pCodecCtx))) {
		// Add image with no alpha channel, Speed optimization
		f->AddImage(width, height, bytes_per_pixel, QImage::Format_RGBA8888_Premultiplied, buffer);
	} else {
		// Add image with alpha channel (this will be converted to premultipled when needed, but is slower)
		f->AddImage(width, height, bytes_per_pixel, QImage::Format_RGBA8888, buffer);
	}

	// Update working cache
	working_cache.Add(f);

	// Keep track of last last_video_frame
	last_video_frame = f;

	// Free the RGB image
	AV_RESET_FRAME(pFrameRGB);

    // Remove frame and packet
    RemoveAVFrame(pFrame);

	// Get video PTS in seconds
	video_pts_seconds = (double(video_pts) * info.video_timebase.ToDouble()) + pts_offset_seconds;

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessVideoPacket (After)", "requested_frame", requested_frame, "current_frame", current_frame, "f->number", f->number, "video_pts_seconds", video_pts_seconds);
}

// Process an audio packet
void FFmpegReader::ProcessAudioPacket(int64_t requested_frame) {
	AudioLocation location;
	// Calculate location of current audio packet
	if (packet && packet->pts != AV_NOPTS_VALUE) {
		// Determine related video frame and starting sample # from audio PTS
		location = GetAudioPTSLocation(packet->pts);

		// Track 1st audio packet after a successful seek
		if (!seek_audio_frame_found && is_seeking)
			seek_audio_frame_found = location.frame;
	}

	// Create or get the existing frame object. Requested frame needs to be created
	// in working_cache at least once. Seek can clear the working_cache, so we must
	// add the requested frame back to the working_cache here. If it already exists,
	// it will be moved to the top of the working_cache.
	working_cache.Add(CreateFrame(requested_frame));

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (Before)",
										  "requested_frame", requested_frame,
										  "target_frame", location.frame,
										  "starting_sample", location.sample_start);

	// Init an AVFrame to hold the decoded audio samples
	int frame_finished = 0;
	AVFrame *audio_frame = AV_ALLOCATE_FRAME();
	AV_RESET_FRAME(audio_frame);

	int packet_samples = 0;
	int data_size = 0;

#if IS_FFMPEG_3_2
		int send_packet_err =  avcodec_send_packet(aCodecCtx, packet);
		if (send_packet_err < 0 && send_packet_err != AVERROR_EOF) {
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (Packet not sent)");
		}
		else {
			int receive_frame_err = avcodec_receive_frame(aCodecCtx, audio_frame);
			if (receive_frame_err >= 0) {
				frame_finished = 1;
			}
			if (receive_frame_err == AVERROR_EOF) {
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (EOF detected from decoder)");
				packet_status.audio_eof = true;
			}
			if (receive_frame_err == AVERROR(EINVAL) || receive_frame_err == AVERROR_EOF) {
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (invalid frame received or EOF from decoder)");
				avcodec_flush_buffers(aCodecCtx);
			}
			if (receive_frame_err != 0) {
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (frame not ready yet from decoder)");
			}
		}
#else
		int used = avcodec_decode_audio4(aCodecCtx, audio_frame, &frame_finished, packet);
#endif

	if (frame_finished) {
		packet_status.audio_decoded++;

		// This can be different than the current packet, so we need to look
		// at the current AVFrame from the audio decoder. This timestamp should
		// be used for the remainder of this function
		audio_pts = audio_frame->pts;

		// Determine related video frame and starting sample # from audio PTS
		location = GetAudioPTSLocation(audio_pts);

		// determine how many samples were decoded
		int plane_size = -1;
#if HAVE_CH_LAYOUT
		int nb_channels = AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->ch_layout.nb_channels;
#else
		int nb_channels = AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channels;
#endif
		data_size = av_samples_get_buffer_size(&plane_size, nb_channels,
											   audio_frame->nb_samples, (AVSampleFormat) (AV_GET_SAMPLE_FORMAT(aStream, aCodecCtx)), 1);

		// Calculate total number of samples
		packet_samples = audio_frame->nb_samples * nb_channels;
	} else {
		if (audio_frame) {
			// Free audio frame
			AV_FREE_FRAME(&audio_frame);
		}
	}

	// Estimate the # of samples and the end of this packet's location (to prevent GAPS for the next timestamp)
	int pts_remaining_samples = packet_samples / info.channels; // Adjust for zero based array

	// Bail if no samples found
	if (pts_remaining_samples == 0) {
		ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (No samples, bailing)",
										   "packet_samples", packet_samples,
										   "info.channels", info.channels,
										   "pts_remaining_samples", pts_remaining_samples);
		return;
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
		} else {
			// Increment sample start
			previous_packet_location.sample_start += samples;
		}
	}

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (ReSample)",
										  "packet_samples", packet_samples,
										  "info.channels", info.channels,
										  "info.sample_rate", info.sample_rate,
										  "aCodecCtx->sample_fmt", AV_GET_SAMPLE_FORMAT(aStream, aCodecCtx));

	// Create output frame
	AVFrame *audio_converted = AV_ALLOCATE_FRAME();
	AV_RESET_FRAME(audio_converted);
	audio_converted->nb_samples = audio_frame->nb_samples;
	av_samples_alloc(audio_converted->data, audio_converted->linesize, info.channels, audio_frame->nb_samples, AV_SAMPLE_FMT_FLTP, 0);

	SWRCONTEXT *avr = avr_ctx;
	// setup resample context if needed
	if (!avr) {
		avr = SWR_ALLOC();
#if HAVE_CH_LAYOUT
	av_opt_set_chlayout(avr, "in_chlayout", &AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->ch_layout, 0);
	av_opt_set_chlayout(avr, "out_chlayout", &AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->ch_layout, 0);
#else
	av_opt_set_int(avr, "in_channel_layout", AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channel_layout, 0);
	av_opt_set_int(avr, "out_channel_layout", AV_GET_CODEC_ATTRIBUTES(aStream, aCodecCtx)->channel_layout, 0);
	av_opt_set_int(avr, "in_channels", info.channels, 0);
	av_opt_set_int(avr, "out_channels", info.channels, 0);
#endif
	av_opt_set_int(avr, "in_sample_fmt", AV_GET_SAMPLE_FORMAT(aStream, aCodecCtx), 0);
	av_opt_set_int(avr, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
	av_opt_set_int(avr, "in_sample_rate", info.sample_rate, 0);
	av_opt_set_int(avr, "out_sample_rate", info.sample_rate, 0);
	SWR_INIT(avr);
	avr_ctx = avr;
	}

	// Convert audio samples
	int nb_samples = SWR_CONVERT(avr,	// audio resample context
							 audio_converted->data,		  // output data pointers
							 audio_converted->linesize[0],   // output plane size, in bytes. (0 if unknown)
							 audio_converted->nb_samples,	// maximum number of samples that the output buffer can hold
							 audio_frame->data,			  // input data pointers
							 audio_frame->linesize[0],	   // input plane size, in bytes (0 if unknown)
							 audio_frame->nb_samples);	   // number of input samples to convert


	int64_t starting_frame_number = -1;
	for (int channel_filter = 0; channel_filter < info.channels; channel_filter++) {
		// Array of floats (to hold samples for each channel)
		starting_frame_number = location.frame;
		int channel_buffer_size = nb_samples;
		auto *channel_buffer = (float *) (audio_converted->data[channel_filter]);

		// Loop through samples, and add them to the correct frames
		int start = location.sample_start;
		int remaining_samples = channel_buffer_size;
		while (remaining_samples > 0) {
			// Get Samples per frame (for this frame number)
			int samples_per_frame = Frame::GetSamplesPerFrame(starting_frame_number, info.fps, info.sample_rate, info.channels);

			// Calculate # of samples to add to this frame
			int samples = std::fmin(samples_per_frame - start, remaining_samples);

			// Create or get the existing frame object
			std::shared_ptr<Frame> f = CreateFrame(starting_frame_number);

			// Add samples for current channel to the frame.
			f->AddAudio(true, channel_filter, start, channel_buffer, samples, 1.0f);

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (f->AddAudio)",
											"frame", starting_frame_number,
											"start", start,
											"samples", samples,
											"channel", channel_filter,
											"samples_per_frame", samples_per_frame);

			// Add or update cache
			working_cache.Add(f);

			// Decrement remaining samples
			remaining_samples -= samples;

			// Increment buffer (to next set of samples)
			if (remaining_samples > 0)
				channel_buffer += samples;

			// Increment frame number
			starting_frame_number++;

			// Reset starting sample #
			start = 0;
		}
	}

	// Free AVFrames
	av_free(audio_converted->data[0]);
	AV_FREE_FRAME(&audio_converted);
	AV_FREE_FRAME(&audio_frame);

	// Get audio PTS in seconds
	audio_pts_seconds = (double(audio_pts) * info.audio_timebase.ToDouble()) + pts_offset_seconds;

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::ProcessAudioPacket (After)",
										  "requested_frame", requested_frame,
										  "starting_frame", location.frame,
										  "end_frame", starting_frame_number - 1,
										  "audio_pts_seconds", audio_pts_seconds);

}


// Seek to a specific frame.  This is not always frame accurate, it's more of an estimation on many codecs.
void FFmpegReader::Seek(int64_t requested_frame) {
	// Adjust for a requested frame that is too small or too large
	if (requested_frame < 1)
		requested_frame = 1;
	if (requested_frame > info.video_length)
		requested_frame = info.video_length;
	if (requested_frame > largest_frame_processed && packet_status.end_of_file) {
		// Not possible to search past largest_frame once EOF is reached (no more packets)
		return;
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::Seek",
										  "requested_frame", requested_frame,
										  "seek_count", seek_count,
										  "last_frame", last_frame);

	// Clear working cache (since we are seeking to another location in the file)
	working_cache.Clear();

	// Reset the last frame variable
	video_pts = 0.0;
	video_pts_seconds = NO_PTS_OFFSET;
	audio_pts = 0.0;
	audio_pts_seconds = NO_PTS_OFFSET;
	hold_packet = false;
	last_frame = 0;
	current_video_frame = 0;
	largest_frame_processed = 0;
	bool has_audio_override = info.has_audio;
	bool has_video_override = info.has_video;

	// Init end-of-file detection variables
	packet_status.reset(false);

	// Increment seek count
	seek_count++;

	// If seeking near frame 1, we need to close and re-open the file (this is more reliable than seeking)
	int buffer_amount = std::max(max_concurrent_frames, 8);
	if (requested_frame - buffer_amount < 20) {
		// prevent Open() from seeking again
		is_seeking = true;

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

		// Seek video stream (if any), except album arts
		if (!seek_worked && info.has_video && !HasAlbumArt()) {
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
			seeking_pts = 0;
			seeking_frame = 0;

			// prevent Open() from seeking again
			is_seeking = true;

			// Close and re-open file (basically seeking to frame 1)
			Close();
			Open();

			// Not actually seeking, so clear these flags
			is_seeking = false;

			// disable seeking for this reader (since it failed)
			enable_seek = false;

			// Update overrides (since closing and re-opening might update these)
			info.has_audio = has_audio_override;
			info.has_video = has_video_override;
		}
	}
}

// Get the PTS for the current video packet
int64_t FFmpegReader::GetPacketPTS() {
	if (packet) {
		int64_t current_pts = packet->pts;
		if (current_pts == AV_NOPTS_VALUE && packet->dts != AV_NOPTS_VALUE)
			current_pts = packet->dts;

		// Return adjusted PTS
		return current_pts;
	} else {
		// No packet, return NO PTS
		return AV_NOPTS_VALUE;
	}
}

// Update PTS Offset (if any)
void FFmpegReader::UpdatePTSOffset() {
	if (pts_offset_seconds != NO_PTS_OFFSET) {
		// Skip this method if we have already set PTS offset
		return;
	}
	pts_offset_seconds = 0.0;
	double video_pts_offset_seconds = 0.0;
	double audio_pts_offset_seconds = 0.0;

	bool has_video_pts = false;
	if (!info.has_video) {
		// Mark as checked
		has_video_pts = true;
	}
	bool has_audio_pts = false;
	if (!info.has_audio) {
		// Mark as checked
		has_audio_pts = true;
	}

	// Loop through the stream (until a packet from all streams is found)
	while (!has_video_pts || !has_audio_pts) {
		// Get the next packet (if any)
		if (GetNextPacket() < 0)
			// Break loop when no more packets found
			break;

		// Get PTS of this packet
		int64_t pts = GetPacketPTS();

		// Video packet
		if (!has_video_pts && packet->stream_index == videoStream) {
			// Get the video packet start time (in seconds)
			video_pts_offset_seconds = 0.0 - (video_pts * info.video_timebase.ToDouble());

			// Is timestamp close to zero (within X seconds)
			// Ignore wildly invalid timestamps (i.e. -234923423423)
			if (std::abs(video_pts_offset_seconds) <= 10.0) {
				has_video_pts = true;
			}
		}
		else if (!has_audio_pts && packet->stream_index == audioStream) {
			// Get the audio packet start time (in seconds)
			audio_pts_offset_seconds = 0.0 - (pts * info.audio_timebase.ToDouble());

			// Is timestamp close to zero (within X seconds)
			// Ignore wildly invalid timestamps (i.e. -234923423423)
			if (std::abs(audio_pts_offset_seconds) <= 10.0) {
				has_audio_pts = true;
			}
		}
	}

	// Do we have all valid timestamps to determine PTS offset?
	if (has_video_pts && has_audio_pts) {
		// Set PTS Offset to the smallest offset
		//	 [  video timestamp  ]
		//		   [  audio timestamp  ]
		//
		//	 ** SHIFT TIMESTAMPS TO ZERO **
		//
		//[  video timestamp  ]
		//	  [  audio timestamp  ]
		//
		// Since all offsets are negative at this point, we want the max value, which
		// represents the closest to zero
		pts_offset_seconds = std::max(video_pts_offset_seconds, audio_pts_offset_seconds);
	}
}

// Convert PTS into Frame Number
int64_t FFmpegReader::ConvertVideoPTStoFrame(int64_t pts) {
	// Apply PTS offset
	int64_t previous_video_frame = current_video_frame;

	// Get the video packet start time (in seconds)
	double video_seconds = (double(pts) * info.video_timebase.ToDouble()) + pts_offset_seconds;

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
	}

	// Return frame #
	return frame;
}

// Convert Frame Number into Video PTS
int64_t FFmpegReader::ConvertFrameToVideoPTS(int64_t frame_number) {
	// Get timestamp of this frame (in seconds)
	double seconds = (double(frame_number - 1) / info.fps.ToDouble()) + pts_offset_seconds;

	// Calculate the # of video packets in this timestamp
	int64_t video_pts = round(seconds / info.video_timebase.ToDouble());

	// Apply PTS offset (opposite)
	return video_pts;
}

// Convert Frame Number into Video PTS
int64_t FFmpegReader::ConvertFrameToAudioPTS(int64_t frame_number) {
	// Get timestamp of this frame (in seconds)
	double seconds = (double(frame_number - 1) / info.fps.ToDouble()) + pts_offset_seconds;

	// Calculate the # of audio packets in this timestamp
	int64_t audio_pts = round(seconds / info.audio_timebase.ToDouble());

	// Apply PTS offset (opposite)
	return audio_pts;
}

// Calculate Starting video frame and sample # for an audio PTS
AudioLocation FFmpegReader::GetAudioPTSLocation(int64_t pts) {
	// Get the audio packet start time (in seconds)
	double audio_seconds = (double(pts) * info.audio_timebase.ToDouble()) + pts_offset_seconds;

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

// Check the working queue, and move finished frames to the finished queue
void FFmpegReader::CheckWorkingFrames(int64_t requested_frame) {

	// Prevent async calls to the following code
	const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

	// Get a list of current working queue frames in the cache (in-progress frames)
	std::vector<std::shared_ptr<openshot::Frame>> working_frames = working_cache.GetFrames();
	std::vector<std::shared_ptr<openshot::Frame>>::iterator working_itr;

	// Loop through all working queue frames (sorted by frame #)
	for(working_itr = working_frames.begin(); working_itr != working_frames.end(); ++working_itr)
	{
		// Get working frame
		std::shared_ptr<Frame> f = *working_itr;

		// Was a frame found? Is frame requested yet?
		if (!f || f->number > requested_frame) {
			// If not, skip to next one
			continue;
		}

		// Calculate PTS in seconds (of working frame), and the most recent processed pts value
		double frame_pts_seconds = (double(f->number - 1) / info.fps.ToDouble()) + pts_offset_seconds;
		double recent_pts_seconds = std::max(video_pts_seconds, audio_pts_seconds);

		// Determine if video and audio are ready (based on timestamps)
		bool is_video_ready = false;
		bool is_audio_ready = false;
		double recent_pts_diff = recent_pts_seconds - frame_pts_seconds;
		if ((frame_pts_seconds <= video_pts_seconds)
			|| (recent_pts_diff > 1.5)
			|| packet_status.video_eof || packet_status.end_of_file) {
			// Video stream is past this frame (so it must be done)
			// OR video stream is too far behind, missing, or end-of-file
			is_video_ready = true;
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckWorkingFrames (video ready)",
											"frame_number", f->number, 
											"frame_pts_seconds", frame_pts_seconds, 
											"video_pts_seconds", video_pts_seconds, 
											"recent_pts_diff", recent_pts_diff);
			if (info.has_video && !f->has_image_data) {
				// Frame has no image data (copy from previous frame)
				// Loop backwards through final frames (looking for the nearest, previous frame image)
				for (int64_t previous_frame = requested_frame - 1; previous_frame > 0; previous_frame--) {
					std::shared_ptr<Frame> previous_frame_instance = final_cache.GetFrame(previous_frame);
					if (previous_frame_instance && previous_frame_instance->has_image_data) {
						// Copy image from last decoded frame
						f->AddImage(std::make_shared<QImage>(previous_frame_instance->GetImage()->copy()));
						break;
					}
				}
				
				if (last_video_frame && !f->has_image_data) {
					// Copy image from last decoded frame
					f->AddImage(std::make_shared<QImage>(last_video_frame->GetImage()->copy()));
				} else if (!f->has_image_data) {
					f->AddColor("#000000");
				}
			}
		}

		double audio_pts_diff = audio_pts_seconds - frame_pts_seconds;
		if ((frame_pts_seconds < audio_pts_seconds && audio_pts_diff > 1.0)
		   || (recent_pts_diff > 1.5)
		   || packet_status.audio_eof || packet_status.end_of_file) {
			// Audio stream is past this frame (so it must be done)
			// OR audio stream is too far behind, missing, or end-of-file
			// Adding a bit of margin here, to allow for partial audio packets
			is_audio_ready = true;
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckWorkingFrames (audio ready)",
											"frame_number", f->number, 
											"frame_pts_seconds", frame_pts_seconds, 
											"audio_pts_seconds", audio_pts_seconds, 
											"audio_pts_diff", audio_pts_diff, 
											"recent_pts_diff", recent_pts_diff);
		}
		bool is_seek_trash = IsPartialFrame(f->number);

		// Adjust for available streams
		if (!info.has_video) is_video_ready = true;
		if (!info.has_audio) is_audio_ready = true;

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckWorkingFrames",
										   "frame_number", f->number, 
										   "is_video_ready", is_video_ready, 
										   "is_audio_ready", is_audio_ready, 
										   "video_eof", packet_status.video_eof,
										   "audio_eof", packet_status.audio_eof,
										   "end_of_file", packet_status.end_of_file);

		// Check if working frame is final
		if ((!packet_status.end_of_file && is_video_ready && is_audio_ready) || packet_status.end_of_file || is_seek_trash) {
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegReader::CheckWorkingFrames (mark frame as final)", 
											"requested_frame", requested_frame, 
											"f->number", f->number, 
											"is_seek_trash", is_seek_trash, 
											"Working Cache Count", working_cache.Count(), 
											"Final Cache Count", final_cache.Count(), 
											"end_of_file", packet_status.end_of_file);

			if (!is_seek_trash) {
				// Move frame to final cache
				final_cache.Add(f);

				// Remove frame from working cache
				working_cache.Remove(f->number);

				// Update last frame processed
				last_frame = f->number;
			} else {
				// Seek trash, so delete the frame from the working cache, and never add it to the final cache.
				working_cache.Remove(f->number);
			}

		}
	}

	// Clear vector of frames
	working_frames.clear();
	working_frames.shrink_to_fit();
}

// Check for the correct frames per second (FPS) value by scanning the 1st few seconds of video packets.
void FFmpegReader::CheckFPS() {
	if (check_fps) {
		// Do not check FPS more than 1 time
		return;
	} else {
		check_fps = true;
	}

	int frames_per_second[3] = {0,0,0};
	int max_fps_index = sizeof(frames_per_second) / sizeof(frames_per_second[0]);
	int fps_index = 0;

	int all_frames_detected = 0;
	int starting_frames_detected = 0;

	// Loop through the stream
	while (true) {
		// Get the next packet (if any)
		if (GetNextPacket() < 0)
			// Break loop when no more packets found
			break;

		// Video packet
		if (packet->stream_index == videoStream) {
			// Get the video packet start time (in seconds)
			double video_seconds = (double(GetPacketPTS()) * info.video_timebase.ToDouble()) + pts_offset_seconds;
			fps_index = int(video_seconds); // truncate float timestamp to int (second 1, second 2, second 3)

			// Is this video packet from the first few seconds?
			if (fps_index >= 0 && fps_index < max_fps_index) {
				// Yes, keep track of how many frames per second (over the first few seconds)
				starting_frames_detected++;
				frames_per_second[fps_index]++;
			}

			// Track all video packets detected
			all_frames_detected++;
		}
	}

	// Calculate FPS (based on the first few seconds of video packets)
	float avg_fps = 30.0;
	if (starting_frames_detected > 0 && fps_index > 0) {
		avg_fps = float(starting_frames_detected) / std::min(fps_index, max_fps_index);
	}

	// Verify average FPS is a reasonable value
	if (avg_fps < 8.0) {
		// Invalid FPS assumed, so switching to a sane default FPS instead
		avg_fps = 30.0;
	}

	// Update FPS (truncate average FPS to Integer)
	info.fps = Fraction(int(avg_fps), 1);

	// Update Duration and Length
	if (all_frames_detected > 0) {
		// Use all video frames detected to calculate # of frames
		info.video_length = all_frames_detected;
		info.duration = all_frames_detected / avg_fps;
	} else {
		// Use previous duration to calculate # of frames
		info.video_length = info.duration * avg_fps;
	}

	// Update video bit rate
	info.video_bit_rate = info.file_size / info.duration;
}

// Remove AVFrame from cache (and deallocate its memory)
void FFmpegReader::RemoveAVFrame(AVFrame *remove_frame) {
	// Remove pFrame (if exists)
	if (remove_frame) {
		// Free memory
		av_freep(&remove_frame->data[0]);
#ifndef WIN32
		AV_FREE_FRAME(&remove_frame);
#endif
	}
}

// Remove AVPacket from cache (and deallocate its memory)
void FFmpegReader::RemoveAVPacket(AVPacket *remove_packet) {
	// deallocate memory for packet
	AV_FREE_PACKET(remove_packet);

	// Delete the object
	delete remove_packet;
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
