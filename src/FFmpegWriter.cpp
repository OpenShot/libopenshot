/*
 * This file is originally based on the Libavformat API example, and then modified
 * by the libopenshot project.
 *
 * Copyright (c) 2003 Fabrice Bellard, OpenShot Studios, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "../include/FFmpegWriter.h"

using namespace openshot;

FFmpegWriter::FFmpegWriter(string path) throw(InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory) :
		path(path), audio_pts(0), video_pts(0)
{
	// Init FileInfo struct (clear all values)
	InitFileInfo();

	// Disable audio & video (so they can be independently enabled)
	info.has_audio = false;
	info.has_video = false;

	// Initialize FFMpeg, and register all formats and codecs
	av_register_all();

    // auto detect format
	auto_detect_format();
}

// auto detect format (from path)
void FFmpegWriter::auto_detect_format()
{
	// Auto detect the output format from the name. default is mpeg.
	fmt = av_guess_format(NULL, path.c_str(), NULL);
	if (!fmt)
		throw InvalidFormat("Could not deduce output format from file extension.", path);

    // Allocate the output media context
    oc = avformat_alloc_context();
    if (!oc)
    	throw OutOfMemory("Could not allocate memory for AVFormatContext.", path);

    // Set the AVOutputFormat for the current AVFormatContext
    oc->oformat = fmt;

    // Update codec names
    if (fmt->video_codec != CODEC_ID_NONE)
    	// Update video codec name
        info.vcodec = avcodec_find_encoder(fmt->video_codec)->name;

    if (fmt->audio_codec != CODEC_ID_NONE)
    	// Update audio codec name
        info.acodec = avcodec_find_encoder(fmt->audio_codec)->name;
}

// initialize streams
void FFmpegWriter::initialize_streams()
{
    // Add the audio and video streams using the default format codecs and initialize the codecs
    video_st = NULL;
    audio_st = NULL;
    if (fmt->video_codec != CODEC_ID_NONE) {
        video_st = add_video_stream();
    }
    if (fmt->audio_codec != CODEC_ID_NONE) {
        audio_st = add_audio_stream();
    }

	// output debug info
	av_dump_format(oc, 0, path.c_str(), 1);
}

// Set video export options
void FFmpegWriter::SetVideoOptions(bool has_video, string codec, Fraction fps, int width, int height,
		Fraction pixel_ratio, bool interlaced, bool top_field_first, int bit_rate)
{
	// Set the video options
	if (codec.length() > 0)
	{
		AVCodec *new_codec = avcodec_find_encoder_by_name(codec.c_str());
		if (new_codec == NULL)
			throw InvalidCodec("A valid audio codec could not be found for this file.", path);
		else
		{
			// Set video codec
			info.vcodec = new_codec->name;

			// Update video codec in fmt
			fmt->video_codec = new_codec->id;
		}
	}
	if (fps.num > 0)
	{
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
	if (pixel_ratio.num > 0)
	{
		info.pixel_ratio.num = pixel_ratio.num;
		info.pixel_ratio.den = pixel_ratio.den;
	}
	if (bit_rate >= 1000)
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

	// Enable / Disable video
	info.has_video = has_video;
}

// Set audio export options
void FFmpegWriter::SetAudioOptions(bool has_audio, string codec, int sample_rate, int channels, int bit_rate)
{
	// Set audio options
	if (codec.length() > 0)
	{
		AVCodec *new_codec = avcodec_find_encoder_by_name(codec.c_str());
		if (new_codec == NULL)
			throw InvalidCodec("A valid audio codec could not be found for this file.", path);
		else
		{
			// Set audio codec
			info.acodec = new_codec->name;

			// Update audio codec in fmt
			fmt->audio_codec = new_codec->id;
		}
	}
	if (sample_rate > 7999)
		info.sample_rate = sample_rate;
	if (channels > 0)
		info.channels = channels;
	if (bit_rate > 999)
		info.audio_bit_rate = bit_rate;

	// Enable / Disable audio
	info.has_audio = has_audio;
}

// Set custom options (some codecs accept additional params)
void FFmpegWriter::SetOption(Stream_Type stream, string name, double value)
{

}

// Write the file header (after the options are set)
void FFmpegWriter::WriteHeader()
{
	if (!info.has_audio && !info.has_video)
		throw InvalidOptions("No video or audio options have been set.  You must set has_video or has_audio (or both).", path);

	// initialize the streams (i.e. add the streams)
	initialize_streams();

    // Now that all the parameters are set, we can open the audio and video codecs and allocate the necessary encode buffers
    if (info.has_video && video_st)
        open_video(oc, video_st);
    if (info.has_audio && audio_st)
        open_audio(oc, audio_st);

    // Open the output file, if needed
    if (!(fmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&oc->pb, path.c_str(), AVIO_FLAG_WRITE) < 0)
        	throw InvalidFile("Could not open or write file.", path);
    }

    // Write the stream header, if any
    // TODO: add avoptions / parameters instead of NULL
    avformat_write_header(oc, NULL);
}

// Write a single frame
void FFmpegWriter::WriteFrame(Frame* frame)
{

}

// Write a block of frames from a reader
void FFmpegWriter::WriteFrame(FileReaderBase* reader, int start, int length)
{

}

// Write the file trailer (after all frames are written)
void FFmpegWriter::WriteTrailer()
{
    /* write the trailer, if any.  the trailer must be written
     * before you close the CodecContexts open when you wrote the
     * header; otherwise write_trailer may try to use memory that
     * was freed on av_codec_close() */
    av_write_trailer(oc);
}

// Close the video codec
void FFmpegWriter::close_video(AVFormatContext *oc, AVStream *st)
{
    avcodec_close(st->codec);

    //av_free(picture->data[0]);
    //av_free(picture);
    //if (tmp_picture) {
    //    av_free(tmp_picture->data[0]);
    //    av_free(tmp_picture);
    //}
    //av_free(video_outbuf);
}

// Close the audio codec
void FFmpegWriter::close_audio(AVFormatContext *oc, AVStream *st)
{
    avcodec_close(st->codec);

    //av_free(samples);
    //av_free(audio_outbuf);
}

// Close the writer
void FFmpegWriter::Close()
{
    // Close each codec
    if (video_st)
        close_video(oc, video_st);
    if (audio_st)
        close_audio(oc, audio_st);

    // Free the streams
    for(int i = 0; i < oc->nb_streams; i++) {
        av_freep(&oc->streams[i]->codec);
        av_freep(&oc->streams[i]);
    }

    if (!(fmt->flags & AVFMT_NOFILE)) {
        /* close the output file */
        avio_close(oc->pb);
    }

    /* free the stream */
    av_free(oc);
}

// Add an audio output stream
AVStream* FFmpegWriter::add_audio_stream()
{
    AVCodecContext *c;
    AVStream *st;

    // Find the audio codec
    cout << "info.acodec: " << info.acodec.c_str() << endl;
	AVCodec *codec = avcodec_find_encoder_by_name(info.acodec.c_str());
	if (codec == NULL) {
		throw InvalidCodec("A valid audio codec could not be found for this file.", path);
	}

    st = avformat_new_stream(oc, codec);
    if (!st)
    	throw OutOfMemory("Could not allocate memory for the audio stream.", path);

    c = st->codec;
    c->codec_id = codec->id;
    c->codec_type = AVMEDIA_TYPE_AUDIO;

    /* put sample parameters */
    c->sample_fmt = AV_SAMPLE_FMT_S16;
    c->bit_rate = info.audio_bit_rate;
    c->sample_rate = info.sample_rate;
    c->channels = info.channels;

    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}

// Add a video output stream
AVStream* FFmpegWriter::add_video_stream()
{
    AVCodecContext *c;
    AVStream *st;

    // Find the audio codec
	AVCodec *codec = avcodec_find_encoder_by_name(info.vcodec.c_str());
	if (codec == NULL) {
		throw InvalidCodec("A valid video codec could not be found for this file.", path);
	}

	st = avformat_new_stream(oc, codec);
    if (!st)
    	throw OutOfMemory("Could not allocate memory for the video stream.", path);

    c = st->codec;
    c->codec_id = codec->id;
    c->codec_type = AVMEDIA_TYPE_VIDEO;

    /* put sample parameters */
    c->bit_rate = info.video_bit_rate;
    /* resolution must be a multiple of two */
    // TODO: require /2 height and width
    c->width = info.width;
    c->height = info.height;

    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    c->time_base.den = info.video_timebase.den;
    c->time_base.num = info.video_timebase.num;
    c->gop_size = 12; /* TODO: add this to "info"... emit one intra frame every twelve frames at most */
    c->pix_fmt = PIX_FMT_YUV420P;
    if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
    }
    if (c->codec_id == CODEC_ID_MPEG1VIDEO){
        /* Needed to avoid using macroblocks in which some coeffs overflow.
           This does not happen with normal video, it just happens here as
           the motion of the chroma plane does not match the luma plane. */
        c->mb_decision=2;
    }
    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}

// open audio codec
void FFmpegWriter::open_audio(AVFormatContext *oc, AVStream *st)
{
    AVCodecContext *c;
    AVCodec *codec;

    c = st->codec;

    /* find the audio encoder */
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec)
    	throw InvalidCodec("Could not find codec", path);

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0)
    	throw InvalidCodec("Could not open codec", path);
}

// open video codec
void FFmpegWriter::open_video(AVFormatContext *oc, AVStream *st)
{
    AVCodec *codec;
    AVCodecContext *c;

    c = st->codec;

    /* find the video encoder */
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec)
    	throw InvalidCodec("Could not find codec", path);

    /* open the codec */
    if (avcodec_open2(c, codec, NULL) < 0)
    	throw InvalidCodec("Could not open codec", path);
}

