/**
 * @file
 * @brief Header file for FFmpegWriter class
 * @author Jonathan Thomas <jonathan@openshot.org>, Fabrice Bellard
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC, Fabrice Bellard
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * This file is originally based on the Libavformat API example, and then modified
 * by the libopenshot project.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * * OpenShot Library (libopenshot) is free software: you can redistribute it
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


#ifndef OPENSHOT_FFMPEG_WRITER_H
#define OPENSHOT_FFMPEG_WRITER_H

#include "ReaderBase.h"
#include "WriterBase.h"

// Include FFmpeg headers and macros
#include "FFmpegUtilities.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include "CacheMemory.h"
#include "Exceptions.h"
#include "OpenMPUtilities.h"
#include "ZmqLogger.h"


using namespace std;

namespace openshot
{

	/// This enumeration designates the type of stream when encoding (video or audio)
	enum StreamType
	{
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
	 * FFmpegReader r("MyAwesomeVideo.webm");
	 * r.Open(); // Open thetarget_ reader
	 *
	 * // Create a writer (which will create a WebM video)
	 * FFmpegWriter w("/home/jonathan/NewVideo.webm");
	 *
	 * // Set options
	 * w.SetAudioOptions(true, "libvorbis", 44100, 2, 128000); // Sample Rate: 44100, Channels: 2, Bitrate: 128000
	 * w.SetVideoOptions(true, "libvpx", openshot::Fraction(24,1), 720, 480, openshot::Fraction(1,1), false, false, 300000); // FPS: 24, Size: 720x480, Pixel Ratio: 1/1, Bitrate: 300000
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
	 * FFmpegReader r("MyAwesomeVideo.webm");
	 * r.Open(); // Open the reader
	 *
	 * // Create a writer (which will create a WebM video)
	 * FFmpegWriter w("/home/jonathan/NewVideo.webm");
	 *
	 * // Set options
	 * w.SetAudioOptions(true, "libvorbis", 44100, 2, 128000); // Sample Rate: 44100, Channels: 2, Bitrate: 128000
	 * w.SetVideoOptions(true, "libvpx", openshot::Fraction(24,1), 720, 480, openshot::Fraction(1,1), false, false, 300000); // FPS: 24, Size: 720x480, Pixel Ratio: 1/1, Bitrate: 300000
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
	class FFmpegWriter : public WriterBase
	{
	private:
		string path;
		int cache_size;
		bool is_writing;
		bool is_open;
		int64 write_video_count;
		int64 write_audio_count;

		bool prepare_streams;
		bool write_header;
		bool write_trailer;

	    AVOutputFormat *fmt;
	    AVFormatContext *oc;
	    AVStream *audio_st, *video_st;
	    AVCodecContext *video_codec;
	    AVCodecContext *audio_codec;
	    SwsContext *img_convert_ctx;
	    double audio_pts, video_pts;
	    int16_t *samples;
	    uint8_t *audio_outbuf;
	    uint8_t *audio_encoder_buffer;

	    int num_of_rescalers;
		int rescaler_position;
		vector<SwsContext*> image_rescalers;

	    int audio_outbuf_size;
	    int audio_input_frame_size;
	    int initial_audio_input_frame_size;
	    int audio_input_position;
	    int audio_encoder_buffer_size;
	    AVAudioResampleContext *avr;
	    AVAudioResampleContext *avr_planar;

	    /* Resample options */
	    int original_sample_rate;
	    int original_channels;

	    std::shared_ptr<Frame> last_frame;
	    deque<std::shared_ptr<Frame> > spooled_audio_frames;
	    deque<std::shared_ptr<Frame> > spooled_video_frames;

	    deque<std::shared_ptr<Frame> > queued_audio_frames;
	    deque<std::shared_ptr<Frame> > queued_video_frames;

	    deque<std::shared_ptr<Frame> > processed_frames;
	    deque<std::shared_ptr<Frame> > deallocate_frames;

	    map<std::shared_ptr<Frame>, AVFrame*> av_frames;

	    /// Add an AVFrame to the cache
	    void add_avframe(std::shared_ptr<Frame> frame, AVFrame* av_frame);

		/// Add an audio output stream
		AVStream* add_audio_stream();

		/// Add a video output stream
		AVStream* add_video_stream();

		/// Allocate an AVFrame object
		AVFrame* allocate_avframe(PixelFormat pix_fmt, int width, int height, int *buffer_size, uint8_t *new_buffer);

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

		/// @brief Init a collection of software rescalers (thread safe)
		/// @param source_width The source width of the image scalers (used to cache a bunch of scalers)
		/// @param source_height The source height of the image scalers (used to cache a bunch of scalers)
		void InitScalers(int source_width, int source_height);

		/// open audio codec
		void open_audio(AVFormatContext *oc, AVStream *st);

		/// open video codec
		void open_video(AVFormatContext *oc, AVStream *st);

		/// process video frame
		void process_video_packet(std::shared_ptr<Frame> frame);

		/// write all queued frames' audio to the video file
		void write_audio_packets(bool final);

		/// write video frame
		bool write_video_packet(std::shared_ptr<Frame> frame, AVFrame* frame_final);

		/// write all queued frames
		void write_queued_frames() throw (ErrorEncodingVideo);

	public:

		/// @brief Constructor for FFmpegWriter. Throws one of the following exceptions.
		/// @param path The file path of the video file you want to open and read
		FFmpegWriter(string path) throw(InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory);

		/// Close the writer
		void Close();

		/// Get the cache size (number of frames to queue before writing)
		int GetCacheSize() { return cache_size; };

		/// Determine if writer is open or closed
		bool IsOpen() { return is_open; };

		/// Open writer
		void Open() throw(InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory, InvalidChannels, InvalidSampleRate);

		/// Output the ffmpeg info about this format, streams, and codecs (i.e. dump format)
		void OutputStreamInfo();

		/// @brief Prepare & initialize streams and open codecs. This method is called automatically
		/// by the Open() method if this method has not yet been called.
		void PrepareStreams();

		/// Remove & deallocate all software scalers
		void RemoveScalers();

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
		void SetAudioOptions(bool has_audio, string codec, int sample_rate, int channels, ChannelLayout channel_layout, int bit_rate)
				throw(InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory, InvalidChannels);

		/// @brief Set the cache size
		/// @param new_size The number of frames to queue before writing to the file
		void SetCacheSize(int new_size) { cache_size = new_size; };

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
		void SetVideoOptions(bool has_video, string codec, Fraction fps, int width, int height,Fraction pixel_ratio, bool interlaced, bool top_field_first, int bit_rate)
				throw(InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory, InvalidChannels);

		/// @brief Set custom options (some codecs accept additional params). This must be called after the
		/// PrepareStreams() method, otherwise the streams have not been initialized yet.
		/// @param stream The stream (openshot::StreamType) this option should apply to
		/// @param name The name of the option you want to set (i.e. qmin, qmax, etc...)
		/// @param value The new value of this option
		void SetOption(StreamType stream, string name, string value) throw(NoStreamsFound, InvalidOptions);

		/// @brief Write the file header (after the options are set). This method is called automatically
		/// by the Open() method if this method has not yet been called.
		void WriteHeader();

		/// @brief Add a frame to the stack waiting to be encoded.
		/// @param frame The openshot::Frame object to write to this image
		void WriteFrame(std::shared_ptr<Frame> frame) throw(ErrorEncodingVideo, WriterClosed);

		/// @brief Write a block of frames from a reader
		/// @param reader A openshot::ReaderBase object which will provide frames to be written
		/// @param start The starting frame number of the reader
		/// @param length The number of frames to write
		void WriteFrame(ReaderBase* reader, long int start, long int length) throw(ErrorEncodingVideo, WriterClosed);

		/// @brief Write the file trailer (after all frames are written). This is called automatically
		/// by the Close() method if this method has not yet been called.
		void WriteTrailer();

	};

}

#endif
