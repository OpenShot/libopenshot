/**
 * @file
 * @brief Header file for FFmpegWriter class
 * @author Jonathan Thomas <jonathan@openshot.org>, Fabrice Bellard
 *
 * This file is originally based on the Libavformat API example, and then modified
 * by the libopenshot project.
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC, Fabrice Bellard
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_FFMPEG_WRITER_H
#define OPENSHOT_FFMPEG_WRITER_H

#include "ReaderBase.h"
#include "WriterBase.h"

// Include FFmpeg headers and macros
#include "FFmpegUtilities.h"

namespace openshot {

	/// This enumeration designates the type of stream when encoding (video or audio)
	enum StreamType {
		VIDEO_STREAM,	///< A video stream (used to determine which type of stream)
		AUDIO_STREAM	///< An audio stream (used to determine which type of stream)
	};

	/**
	 * @brief This class uses the FFmpeg libraries, to write and encode video files and audio files.
	 *
	 * All FFmpeg options can be set using the SetOption() method, and any Reader may be used
	 * to generate openshot::Frame objects needed for writing. Be sure to use valid bit rates, frame
	 * rates, and sample rates (each format / codec has a limited # of valid options).
	 *
	 * @code SIMPLE EXAMPLE
	 *
	 * // Create a reader for a video
	 * openshot::FFmpegReader r("MyAwesomeVideo.webm");
	 * r.Open(); // Open the target reader
	 *
	 * // Create a writer (which will create a WebM video)
	 * openshot::FFmpegWriter w("/home/jonathan/NewVideo.webm");
	 *
	 * // Set options
	 *
	 * // Sample Rate: 44100, Channels: 2, Bitrate: 128000
	 * w.SetAudioOptions(true, "libvorbis", 44100, 2, openshot::ChannelLayout::LAYOUT_STEREO, 128000);
	 *
	 * // FPS: 24, Size: 720x480, Pixel Ratio: 1/1, Bitrate: 300000
	 * w.SetVideoOptions(true, "libvpx", openshot::Fraction(24,1), 720, 480, openshot::Fraction(1,1), false, false, 300000);
	 *
	 * // Open the writer
	 * w.Open();
	 *
	 * // Write all frames from the reader
	 * w.WriteFrame(&r, 1, r.info.video_length);
	 *
	 * // Close the reader & writer
	 * w.Close();
	 * r.Close();
	 * @endcode
	 *
	 * Here is a more advanced example, which sets some additional (and optional) encoding
	 * options.
	 *
	 * @code ADVANCED WRITER EXAMPLE
	 *
	 * // Create a reader for a video
	 * openshot::FFmpegReader r("MyAwesomeVideo.webm");
	 * r.Open(); // Open the reader
	 *
	 * // Create a writer (which will create a WebM video)
	 * openshot::FFmpegWriter w("/home/jonathan/NewVideo.webm");
	 *
	 * // Set options
	 *
	 * // Sample Rate: 44100, Channels: 2, Bitrate: 128000
	 * w.SetAudioOptions(true, "libvorbis", 44100, 2, openshot::ChannelLayout::LAYOUT_STEREO, 128000);
	 *
	 * // FPS: 24, Size: 720x480, Pixel Ratio: 1/1, Bitrate: 300000
	 * w.SetVideoOptions(true, "libvpx", openshot::Fraction(24,1), 720, 480, openshot::Fraction(1,1), false, false, 300000);
	 *
	 * // Prepare Streams (Optional method that must be called before any SetOption calls)
	 * w.PrepareStreams();
	 *
	 * // Set some specific encoding options (Optional methods)
	 * w.SetOption(VIDEO_STREAM, "qmin", "2" );
	 * w.SetOption(VIDEO_STREAM, "qmax", "30" );
	 * w.SetOption(VIDEO_STREAM, "crf", "10" );
	 * w.SetOption(VIDEO_STREAM, "rc_min_rate", "2000000" );
	 * w.SetOption(VIDEO_STREAM, "rc_max_rate", "4000000" );
	 * w.SetOption(VIDEO_STREAM, "max_b_frames", "10" );
	 *
	 * // Write the header of the video file
	 * w.WriteHeader();
	 *
	 * // Open the writer
	 * w.Open();
	 *
	 * // Write all frames from the reader
	 * w.WriteFrame(&r, 1, r.info.video_length);
	 *
	 * // Write the trailer of the video file
	 * w.WriteTrailer();
	 *
	 * // Close the reader & writer
	 * w.Close();
	 * r.Close();
	 * @endcode
	 */
	class FFmpegWriter : public WriterBase {
	private:
		std::string path;
		bool is_writing;
		bool is_open;
		int64_t video_timestamp;
		int64_t audio_timestamp;

		bool prepare_streams;
		bool write_header;
		bool write_trailer;

		AVFormatContext* oc;
		AVStream *audio_st, *video_st;
		AVCodecContext *video_codec_ctx;
		AVCodecContext *audio_codec_ctx;
		SwsContext *img_convert_ctx;
		int16_t *samples;
		uint8_t *audio_outbuf;
		uint8_t *audio_encoder_buffer;

		AVFrame *persistent_src_frame = nullptr;
		AVFrame *persistent_dst_frame = nullptr;
		uint8_t *persistent_dst_buffer = nullptr;
		int persistent_dst_size = 0;

		int audio_outbuf_size;
		int audio_input_frame_size;
		int initial_audio_input_frame_size;
		int audio_input_position;
		int audio_encoder_buffer_size;
		SWRCONTEXT *avr;
		SWRCONTEXT *avr_planar;

		/* Resample options */
		int original_sample_rate;
		int original_channels;

		std::shared_ptr<openshot::Frame> last_frame;
		std::map<std::shared_ptr<openshot::Frame>, AVFrame *> av_frames;

		/// Add an AVFrame to the cache
		void add_avframe(std::shared_ptr<openshot::Frame> frame, AVFrame *av_frame);

		/// Add an audio output stream
		AVStream *add_audio_stream();

		/// Add a video output stream
		AVStream *add_video_stream();

		/// Allocate an AVFrame object
		AVFrame *allocate_avframe(PixelFormat pix_fmt, int width, int height, int *buffer_size, uint8_t *new_buffer);

		/// Auto detect format (from path)
		void auto_detect_format();

		/// Close the audio codec
		void close_audio(AVFormatContext *oc, AVStream *st);

		/// Close the video codec
		void close_video(AVFormatContext *oc, AVStream *st);

		/// Flush encoders
		void flush_encoders();

		/// initialize streams
		void initialize_streams();

		/// open audio codec
		void open_audio(AVFormatContext *oc, AVStream *st);

		/// open video codec
		void open_video(AVFormatContext *oc, AVStream *st);

		/// process video frame
		void process_video_packet(std::shared_ptr<openshot::Frame> frame);

		/// write all queued frames' audio to the video file
		void write_audio_packets(bool is_final, std::shared_ptr<openshot::Frame> frame);

		/// write video frame
		bool write_video_packet(std::shared_ptr<openshot::Frame> frame, AVFrame *frame_final);

		/// write all queued frames
		void write_frame(std::shared_ptr<Frame> frame);

	public:

		/// @brief Constructor for FFmpegWriter.
		/// Throws an exception on failure to open path.
		///
		/// @param path The file path of the video file you want to open and read
		FFmpegWriter(const std::string& path);

		/// Close the writer
		void Close();

		/// Determine if writer is open or closed
		bool IsOpen() { return is_open; };

		/// Determine if codec name is valid
		static bool IsValidCodec(std::string codec_name);

		/// Open writer
		void Open();

		/// Output the ffmpeg info about this format, streams, and codecs (i.e. dump format)
		void OutputStreamInfo();

		/// @brief Prepare & initialize streams and open codecs. This method is called automatically
		/// by the Open() method if this method has not yet been called.
		void PrepareStreams();

		/// @brief Set audio resample options
		/// @param sample_rate The number of samples per second of the audio
		/// @param channels The number of audio channels
		void ResampleAudio(int sample_rate, int channels);

		/// @brief Set audio export options
		/// @param has_audio Does this file need an audio stream?
		/// @param codec The codec used to encode the audio for this file
		/// @param sample_rate The number of audio samples needed in this file
		/// @param channels The number of audio channels needed in this file
		/// @param channel_layout The 'layout' of audio channels (i.e. mono, stereo, surround, etc...)
		/// @param bit_rate The audio bit rate used during encoding
		///
		/// \note This is an overloaded function.
		void SetAudioOptions(bool has_audio, std::string codec, int sample_rate, int channels, openshot::ChannelLayout channel_layout, int bit_rate);

		/// @brief Set audio export options.
		///
		/// Enables the stream and configures a default 2-channel stereo layout.
		///
		/// @param codec The codec used to encode the audio for this file
		/// @param sample_rate The number of audio samples needed in this file
		/// @param bit_rate The audio bit rate used during encoding
		///
		/// \note This is an overloaded function.
		void SetAudioOptions(std::string codec, int sample_rate, int bit_rate);

		/// @brief Set video export options
		/// @param has_video Does this file need a video stream
		/// @param codec The codec used to encode the images in this video
		/// @param fps The number of frames per second
		/// @param width The width in pixels of this video
		/// @param height The height in pixels of this video
		/// @param pixel_ratio The shape of the pixels represented as a openshot::Fraction (1x1 is most common / square pixels)
		/// @param interlaced Does this video need to be interlaced?
		/// @param top_field_first Which frame should be used as the top field?
		/// @param bit_rate The video bit rate used during encoding
		///
		/// \note This is an overloaded function.
		void SetVideoOptions(bool has_video, std::string codec, openshot::Fraction fps, int width, int height, openshot::Fraction pixel_ratio, bool interlaced, bool top_field_first, int bit_rate);

		/// @brief Set video export options.
		///
		/// Enables the stream and configures non-interlaced video with a 1:1 pixel aspect ratio.
		///
		/// @param codec The codec used to encode the images in this video
		/// @param width The width in pixels of this video
		/// @param height The height in pixels of this video
		/// @param fps The number of frames per second
		/// @param bit_rate The video bit rate used during encoding
		///
		/// \note This is an overloaded function.
		/// \warning Observe the argument order, which is consistent with the openshot::Timeline constructor, but differs from the other signature.
		void SetVideoOptions(std::string codec, int width, int height,  openshot::Fraction fps, int bit_rate);

		/// @brief Set custom options (some codecs accept additional params). This must be called after the
		/// PrepareStreams() method, otherwise the streams have not been initialized yet.
		///
		/// @param stream The stream (openshot::StreamType) this option should apply to
		/// @param name The name of the option you want to set (i.e. qmin, qmax, etc...)
		/// @param value The new value of this option
		void SetOption(openshot::StreamType stream, std::string name, std::string value);

		/// @brief Write the file header (after the options are set). This method is called automatically
		/// by the Open() method if this method has not yet been called.
		void WriteHeader();

		/// @brief Add a frame to the stack waiting to be encoded.
		/// @param frame The openshot::Frame object to write to this image
		///
		/// \note This is an overloaded function.
		void WriteFrame(std::shared_ptr<openshot::Frame> frame);

		/// @brief Write a block of frames from a reader
		/// @param reader A openshot::ReaderBase object which will provide frames to be written
		/// @param start The starting frame number of the reader
		/// @param length The number of frames to write
		///
		/// \note This is an overloaded function.
		void WriteFrame(openshot::ReaderBase *reader, int64_t start, int64_t length);

		/// @brief Write the file trailer (after all frames are written). This is called automatically
		/// by the Close() method if this method has not yet been called.
		void WriteTrailer();

		/// @brief Add spherical (360°) video metadata to the video stream
		/// @param projection The projection type (e.g., "equirectangular", "cubemap")
		/// @param yaw_deg The yaw angle in degrees (horizontal orientation, default 0)
		/// @param pitch_deg The pitch angle in degrees (vertical orientation, default 0)
		/// @param roll_deg The roll angle in degrees (tilt orientation, default 0)
		void AddSphericalMetadata(const std::string& projection="equirectangular", float yaw_deg=0.0f, float pitch_deg=0.0f, float roll_deg=0.0f);

	};

} // namespace openshot

#endif
