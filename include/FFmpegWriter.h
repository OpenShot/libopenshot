#ifndef OPENSHOT_FFMPEG_WRITER_H
#define OPENSHOT_FFMPEG_WRITER_H

/**
 * \file
 * \brief Header file for FFmpegWriter class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "FileReaderBase.h"
#include "FileWriterBase.h"

// Required for libavformat to build on Windows
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavcodec/opt.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavutil/mathematics.h>
}
#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include "Magick++.h"
#include "Cache.h"
#include "Exceptions.h"


using namespace std;

namespace openshot
{
	/**
	 * This enumeration designates which a type of stream when encoding
	 */
	enum Stream_Type
	{
		VIDEO_STREAM,
		AUDIO_STREAM
	};

	/**
	 * \brief This class uses the FFmpeg libraries, to write and encode video files and audio files
	 *
	 * TODO
	 */
	class FFmpegWriter : public FileWriterBase
	{
	private:
		string path;
		int cache_size;
		bool is_writing;
		int64 write_frame_count;

	    AVOutputFormat *fmt;
	    AVFormatContext *oc;
	    AVStream *audio_st, *video_st;
	    AVCodecContext *video_codec;
	    AVCodecContext *audio_codec;
	    SwsContext *img_convert_ctx;
	    double audio_pts, video_pts;
	    int16_t *samples;
	    uint8_t *audio_outbuf;

	    int num_of_rescalers;
		int rescaler_position;
		vector<SwsContext*> image_rescalers;

	    int audio_outbuf_size;
	    int audio_input_frame_size;
	    int initial_audio_input_frame_size;
	    int audio_input_position;
	    AudioResampler *resampler;

	    tr1::shared_ptr<Frame> last_frame;
	    deque<tr1::shared_ptr<Frame> > spooled_audio_frames;
	    deque<tr1::shared_ptr<Frame> > spooled_video_frames;

	    deque<tr1::shared_ptr<Frame> > queued_audio_frames;
	    deque<tr1::shared_ptr<Frame> > queued_video_frames;

	    deque<tr1::shared_ptr<Frame> > processed_frames;
	    deque<tr1::shared_ptr<Frame> > deallocate_frames;

	    map<tr1::shared_ptr<Frame>, AVFrame*> av_frames;

	    /// Add an AVFrame to the cache
	    void add_avframe(tr1::shared_ptr<Frame> frame, AVFrame* av_frame);

		/// Add an audio output stream
		AVStream* add_audio_stream();

		/// Add a video output stream
		AVStream* add_video_stream();

		/// Allocate an AVFrame object
		AVFrame* allocate_avframe(PixelFormat pix_fmt, int width, int height, int *buffer_size);

		/// Auto detect format (from path)
		void auto_detect_format();

		/// Close the audio codec
		void close_audio(AVFormatContext *oc, AVStream *st);

		/// Close the video codec
		void close_video(AVFormatContext *oc, AVStream *st);

		/// initialize streams
		void initialize_streams();

		/// open audio codec
		void open_audio(AVFormatContext *oc, AVStream *st);

		/// open video codec
		void open_video(AVFormatContext *oc, AVStream *st);

		/// process video frame
		void process_video_packet(tr1::shared_ptr<Frame> frame);

		/// write all queued frames' audio to the video file
		void write_audio_packets();

		/// write video frame
		void write_video_packet(tr1::shared_ptr<Frame> frame, AVFrame* frame_final);

		/// write all queued frames
		void write_queued_frames();

	public:

		/// Constructor for FFmpegWriter. Throws one of the following exceptions.
		FFmpegWriter(string path) throw(InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory);

		/// Close the writer
		void Close();

		/// Get the cache size (number of frames to queue before writing)
		int GetCacheSize() { return cache_size; };

		/// Init a collection of software rescalers (thread safe)
		void InitScalers(int source_width, int source_height);

		/// Output the ffmpeg info about this format, streams, and codecs (i.e. dump format)
		void OutputStreamInfo();

		/// Prepare & initialize streams and open codecs
		void PrepareStreams();

		/// Remove & deallocate all software scalers
		void RemoveScalers();

		/// Set audio export options
		void SetAudioOptions(bool has_audio, string codec, int sample_rate, int channels, int bit_rate, bool visualize);

		/// Set the cache size (number of frames to queue before writing)
		int SetCacheSize(int new_size) { cache_size = new_size; };

		/// Set video export options
		void SetVideoOptions(bool has_video, string codec, Fraction fps, int width, int height,
				Fraction pixel_ratio, bool interlaced, bool top_field_first, int bit_rate);

		/// Set custom options (some codecs accept additional params)
		void SetOption(Stream_Type stream, string name, string value);

		/// Write the file header (after the options are set)
		void WriteHeader();

		/// Add a frame to the stack waiting to be encoded.
		void WriteFrame(tr1::shared_ptr<Frame> frame);

		/// Write a block of frames from a reader
		void WriteFrame(FileReaderBase* reader, int start, int length);

		/// Write the file trailer (after all frames are written)
		void WriteTrailer();

	};

}

#endif
