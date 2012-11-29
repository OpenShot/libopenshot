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

FFmpegWriter::FFmpegWriter(string path) throw (InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory) :
		path(path), fmt(NULL), oc(NULL), audio_st(NULL), video_st(NULL), audio_pts(0), video_pts(0), samples(NULL),
		audio_outbuf(NULL), audio_outbuf_size(0), audio_input_frame_size(0), audio_input_position(0),
		initial_audio_input_frame_size(0), resampler(NULL), img_convert_ctx(NULL), cache_size(8), num_of_rescalers(32),
		rescaler_position(0), video_codec(NULL), audio_codec(NULL), is_writing(false), write_video_count(0), write_audio_count(0)
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
	if (fmt->video_codec != CODEC_ID_NONE && info.has_video)
		// Add video stream
		video_st = add_video_stream();

	if (fmt->audio_codec != CODEC_ID_NONE && info.has_audio)
		// Add audio stream
		audio_st = add_audio_stream();
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
		else {
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
void FFmpegWriter::SetOption(Stream_Type stream, string name, string value)
{
	// Declare codec context
	AVCodecContext *c = NULL;
	stringstream convert(value);

	if (info.has_video && stream == VIDEO_STREAM)
		c = video_st->codec;
	else if (info.has_audio && stream == AUDIO_STREAM)
		c = audio_st->codec;

	// Init AVOption
	const AVOption *option = NULL;

	// Was a codec / stream found?
	if (c)
		// Find AVOption (if it exists)
		option = av_find_opt(c->priv_data, name.c_str(), NULL, NULL, NULL);

	// Was option found?
	if (option || (name == "g" || name == "qmin" || name == "qmax" || name == "max_b_frames" || name == "mb_decision" ||
			       name == "level" || name == "profile" || name == "slices" || name == "rc_min_rate"  || name == "rc_max_rate"))
	{
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

		else
			// Set AVOption
			av_set_string3 (c->priv_data, name.c_str(), value.c_str(), 0, NULL);
	}
	else
		throw InvalidOptions("The option is not valid for this codec.", path);

}

// Prepare & initialize streams and open codecs
void FFmpegWriter::PrepareStreams()
{
	if (!info.has_audio && !info.has_video)
		throw InvalidOptions("No video or audio options have been set.  You must set has_video or has_audio (or both).", path);

	// Initialize the streams (i.e. add the streams)
	initialize_streams();

	// Now that all the parameters are set, we can open the audio and video codecs and allocate the necessary encode buffers
	if (info.has_video && video_st)
		open_video(oc, video_st);
	if (info.has_audio && audio_st)
		open_audio(oc, audio_st);
}

// Write the file header (after the options are set)
void FFmpegWriter::WriteHeader()
{
	if (!info.has_audio && !info.has_video)
		throw InvalidOptions("No video or audio options have been set.  You must set has_video or has_audio (or both).", path);

	// Open the output file, if needed
	if (!(fmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&oc->pb, path.c_str(), AVIO_FLAG_WRITE) < 0)
			throw InvalidFile("Could not open or write file.", path);
	}

	// Write the stream header, if any
	// TODO: add avoptions / parameters instead of NULL
	avformat_write_header(oc, NULL);
}

// Add a frame to the queue waiting to be encoded.
void FFmpegWriter::WriteFrame(tr1::shared_ptr<Frame> frame)
{
	// Add frame pointer to "queue", waiting to be processed the next
	// time the WriteFrames() method is called.
	if (info.has_video && video_st)
		spooled_video_frames.push_back(frame);

	if (info.has_audio && audio_st)
		spooled_audio_frames.push_back(frame);

	// Write the frames once it reaches the correct cache size
	if (spooled_video_frames.size() == cache_size || spooled_audio_frames.size() == cache_size)
	{
		// Is writer currently writing?
		if (!is_writing)
			// Write frames to video file
			write_queued_frames();

		else
		{
			// YES, WRITING... so wait until it finishes, before writing again
			while (is_writing)
				usleep(250 * 1000); // sleep for 250 milliseconds

			// Write frames to video file
			write_queued_frames();
		}
	}

	// Keep track of the last frame added
	last_frame = frame;
}

// Write all frames in the queue to the video file.
void FFmpegWriter::write_queued_frames()
{
	// Flip writing flag
	is_writing = true;

	// Transfer spool to queue
	queued_video_frames = spooled_video_frames;
	queued_audio_frames = spooled_audio_frames;

	// Empty spool
	spooled_video_frames.clear();
	spooled_audio_frames.clear();

	//omp_set_num_threads(1);
	omp_set_nested(true);
	#pragma omp parallel
	{
		#pragma omp single
		{
			// Process all audio frames (in a separate thread)
			if (info.has_audio && audio_st && !queued_audio_frames.empty())
				write_audio_packets(false);

			// Loop through each queued image frame
			while (!queued_video_frames.empty())
			{
				// Get front frame (from the queue)
				tr1::shared_ptr<Frame> frame = queued_video_frames.front();

				// Add to processed queue
				processed_frames.push_back(frame);

				// Encode and add the frame to the output file
				if (info.has_video && video_st)
					process_video_packet(frame);

				// Remove front item
				queued_video_frames.pop_front();

			} // end while
		} // end omp single

		#pragma omp single
		{
			// Loop back through the frames (in order), and write them to the video file
			while (!processed_frames.empty())
			{
				// Get front frame (from the queue)
				tr1::shared_ptr<Frame> frame = processed_frames.front();

				if (info.has_video && video_st)
				{
					// Add to deallocate queue (so we can remove the AVFrames when we are done)
					deallocate_frames.push_back(frame);

					// Does this frame's AVFrame still exist
					if (av_frames.count(frame))
					{
						// Get AVFrame
						AVFrame *frame_final = av_frames[frame];

						// Write frame to video file
						write_video_packet(frame, frame_final);
					}
				}

				// Remove front item
				processed_frames.pop_front();
			}

			// Loop through, and deallocate AVFrames
			while (!deallocate_frames.empty())
			{
				// Get front frame (from the queue)
				tr1::shared_ptr<Frame> frame = deallocate_frames.front();

				// Does this frame's AVFrame still exist
				if (av_frames.count(frame))
				{
					// Get AVFrame
					AVFrame *av_frame = av_frames[frame];

					// deallocate AVFrame
					av_free(av_frame->data[0]);
					av_free(av_frame);

					av_frames.erase(frame);
				}

				// Remove front item
				deallocate_frames.pop_front();
			}

			// Done writing
			is_writing = false;

		} // end omp single
	} // end omp parallel

}

// Write a block of frames from a reader
void FFmpegWriter::WriteFrame(FileReaderBase* reader, int start, int length)
{
	// Loop through each frame (and encoded it)
	for (int number = start; number <= length; number++)
	{
		// Get the frame
		tr1::shared_ptr<Frame> f = reader->GetFrame(number);

		// Encode frame
		WriteFrame(f);
	}
}

// Write the file trailer (after all frames are written)
void FFmpegWriter::WriteTrailer()
{
	// Write any remaining queued frames to video file
	write_queued_frames();

	// Process final audio frame (if any)
	if (info.has_audio && audio_st)
		write_audio_packets(true);

	// Flush encoders (who sometimes hold on to frames)
	flush_encoders();

	/* write the trailer, if any. The trailer must be written
	 * before you close the CodecContexts open when you wrote the
	 * header; otherwise write_trailer may try to use memory that
	 * was freed on av_codec_close() */
	av_write_trailer(oc);
}

// Flush encoders
void FFmpegWriter::flush_encoders()
{
    if (info.has_audio && audio_codec && audio_st->codec->codec_type == AVMEDIA_TYPE_AUDIO && audio_codec->frame_size <= 1)
        return;
    if (info.has_video && video_st->codec->codec_type == AVMEDIA_TYPE_VIDEO && (oc->oformat->flags & AVFMT_RAWPICTURE) && video_codec->codec->id == CODEC_ID_RAWVIDEO)
        return;

    int error_code = 0;
    int stop_encoding = 1;

    // FLUSH VIDEO ENCODER
    if (info.has_video)
		for (;;) {

			cout << "Flushing VIDEO buffer!" << endl;

			// Increment PTS (in frames and scaled to the codec's timebase)
			write_video_count += av_rescale_q(1, (AVRational){info.fps.den, info.fps.num}, video_codec->time_base);

			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.data = NULL;
			pkt.size = 0;

			// Pointer for video buffer (if using old FFmpeg version)
			uint8_t *video_outbuf = NULL;

			/* encode the image */
			int got_packet = 0;
			int error_code = 0;

			#if LIBAVFORMAT_VERSION_MAJOR >= 54
				// Newer versions of FFMpeg
			error_code = avcodec_encode_video2(video_codec, &pkt, NULL, &got_packet);

			#else
				// Older versions of FFmpeg (much sloppier)

				// Encode Picture and Write Frame
				int video_outbuf_size = 0;
				//video_outbuf = new uint8_t[200000];

				/* encode the image */
				int out_size = avcodec_encode_video(video_codec, NULL, video_outbuf_size, NULL);

				/* if zero size, it means the image was buffered */
				if (out_size > 0) {
					if(video_codec->coded_frame->key_frame)
						pkt.flags |= AV_PKT_FLAG_KEY;
					pkt.data= video_outbuf;
					pkt.size= out_size;

					// got data back (so encode this frame)
					got_packet = 1;
				}
			#endif

			if (error_code < 0) {
				string error_description = "Unknown";

				#ifdef av_err2str
					error_description = av_err2str(error_code);
				#endif

				cout << "error encoding video: " << error_code << ": " << error_description << endl;
				//throw ErrorEncodingVideo("Error while flushing video frame", -1);
			}
			if (!got_packet) {
				stop_encoding = 1;
				break;
			}

			// Override PTS (in frames and scaled to the codec's timebase)
			//pkt.pts = write_video_count;

			// set the timestamp
			if (pkt.pts != AV_NOPTS_VALUE)
				pkt.pts = av_rescale_q(pkt.pts, video_codec->time_base, video_st->time_base);
			if (pkt.dts != AV_NOPTS_VALUE)
				pkt.dts = av_rescale_q(pkt.dts, video_codec->time_base, video_st->time_base);
			if (pkt.duration > 0)
				pkt.duration = av_rescale_q(pkt.duration, video_codec->time_base, video_st->time_base);
			pkt.stream_index = video_st->index;

			// Write packet
			error_code = av_interleaved_write_frame(oc, &pkt);
			if (error_code != 0) {
				string error_description = "Unknown";

				#ifdef av_err2str
					error_description = av_err2str(error_code);
				#endif

				cout << "error writing video: " << error_code << ": " << error_description << endl;
				//throw ErrorEncodingVideo("Error while writing video packet to flush encoder", -1);
			}

			// Deallocate memory (if needed)
			if (video_outbuf)
				delete[] video_outbuf;
		}

    // FLUSH AUDIO ENCODER
    if (info.has_audio)
		for (;;) {

			cout << "Flushing AUDIO buffer!" << endl;

			// Increment PTS (in samples and scaled to the codec's timebase)
			write_audio_count += av_rescale_q(audio_codec->frame_size / av_get_bytes_per_sample(audio_codec->sample_fmt), (AVRational){1, info.sample_rate}, audio_codec->time_base);

			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.data = NULL;
			pkt.size = 0;
			pkt.pts = pkt.dts = write_audio_count;

			/* encode the image */
			int got_packet = 0;
			error_code = avcodec_encode_audio2(audio_codec, &pkt, NULL, &got_packet);
			if (error_code < 0) {
				string error_description = "Unknown";

				#ifdef av_err2str
					error_description = av_err2str(error_code);
				#endif

				cout << "error encoding audio: " << error_code << ": " << error_description << endl;
				//throw ErrorEncodingAudio("Error while flushing audio frame", -1);
			}
			if (!got_packet) {
				stop_encoding = 1;
				break;
			}

			// Since the PTS can change during encoding, set the value again.  This seems like a huge hack,
			// but it fixes lots of PTS related issues when I do this.
			pkt.pts = pkt.dts = write_audio_count;

			// Scale the PTS to the audio stream timebase (which is sometimes different than the codec's timebase)
			if (pkt.pts != AV_NOPTS_VALUE)
				pkt.pts = av_rescale_q(pkt.pts, audio_codec->time_base, audio_st->time_base);
			if (pkt.dts != AV_NOPTS_VALUE)
				pkt.dts = av_rescale_q(pkt.dts, audio_codec->time_base, audio_st->time_base);
			if (pkt.duration > 0)
				pkt.duration = av_rescale_q(pkt.duration, audio_codec->time_base, audio_st->time_base);

			// set stream
			pkt.stream_index = audio_st->index;
			pkt.flags |= AV_PKT_FLAG_KEY;

			// Write packet
			error_code = av_write_frame(oc, &pkt);
			if (error_code != 0) {
				string error_description = "Unknown";

				#ifdef av_err2str
					error_description = av_err2str(error_code);
				#endif

				cout << "error writing audio: " << error_code << ": " << error_description << endl;
				//throw ErrorEncodingAudio("Error while writing audio packet to flush encoder", -1);
			}
		}


}

// Close the video codec
void FFmpegWriter::close_video(AVFormatContext *oc, AVStream *st)
{
	avcodec_close(st->codec);
	video_codec = NULL;
}

// Close the audio codec
void FFmpegWriter::close_audio(AVFormatContext *oc, AVStream *st)
{
	avcodec_close(st->codec);
	audio_codec = NULL;

	delete[] samples;
	delete[] audio_outbuf;

	delete resampler;
}

// Close the writer
void FFmpegWriter::Close()
{
	// Close each codec
	if (video_st)
		close_video(oc, video_st);
	if (audio_st)
		close_audio(oc, audio_st);

	// Deallocate image scalers
	if (image_rescalers.size() > 0)
		RemoveScalers();

	// Free the streams
	for (int i = 0; i < oc->nb_streams; i++) {
		av_freep(&oc->streams[i]->codec);
		av_freep(&oc->streams[i]);
	}

	if (!(fmt->flags & AVFMT_NOFILE)) {
		/* close the output file */
		avio_close(oc->pb);
	}

	// Reset frame counters
	write_video_count = 0;
	write_audio_count = 0;

	// Free the stream
	av_free(oc);
}

// Add an AVFrame to the cache
void FFmpegWriter::add_avframe(tr1::shared_ptr<Frame> frame, AVFrame* av_frame)
{
	// Add AVFrame to map (if it does not already exist)
	if (!av_frames.count(frame))
	{
		// Add av_frame
		av_frames[frame] = av_frame;
	}
	else
	{
		// Do not add, and deallocate this AVFrame
		av_free(av_frame->data[0]);
		av_free(av_frame);
	}
}

// Add an audio output stream
AVStream* FFmpegWriter::add_audio_stream()
{
	AVCodecContext *c;
	AVStream *st;

	// Find the audio codec
	AVCodec *codec = avcodec_find_encoder_by_name(info.acodec.c_str());
	if (codec == NULL)
		throw InvalidCodec("A valid audio codec could not be found for this file.", path);

	// Create a new audio stream
	st = avformat_new_stream(oc, codec);
	if (!st)
		throw OutOfMemory("Could not allocate memory for the audio stream.", path);

	// Set default values
    avcodec_get_context_defaults3(st->codec, codec);

	c = st->codec;
	c->codec_id = codec->id;
#if LIBAVFORMAT_VERSION_MAJOR >= 53
	c->codec_type = AVMEDIA_TYPE_AUDIO;
#else
	c->codec_type = CODEC_TYPE_AUDIO;
#endif

	// Set the sample parameters
	c->bit_rate = info.audio_bit_rate;
	c->channels = info.channels;

	// Set valid sample rate (or throw error)
	if (codec->supported_samplerates) {
		int i;
	    for (i = 0; codec->supported_samplerates[i] != 0; i++)
	        if (info.sample_rate == codec->supported_samplerates[i])
	        {
	        	// Set the valid sample rate
	        	c->sample_rate = info.sample_rate;
	            break;
	        }
	    if (codec->supported_samplerates[i] == 0)
	    	throw InvalidSampleRate("An invalid sample rate was detected for this codec.", path);
	} else
		// Set sample rate
		c->sample_rate = info.sample_rate;


	// Set a valid number of channels (or throw error)
	int channel_layout = info.channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	if (codec->channel_layouts) {
		int i;
		for (i = 0; codec->channel_layouts[i] != 0; i++)
			if (channel_layout == codec->channel_layouts[i])
			{
				// Set valid channel layout
				c->channel_layout = channel_layout;
				break;
			}
		if (codec->channel_layouts[i] == 0)
			throw InvalidChannels("An invalid channel layout was detected (i.e. MONO / STEREO).", path);
	} else
		// Set valid channel layout
		c->channel_layout = channel_layout;

	// Choose a valid sample_fmt
	if (codec->sample_fmts) {
		for (int i = 0; codec->sample_fmts[i] != AV_SAMPLE_FMT_NONE; i++)
		{
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
	if (codec == NULL)
		throw InvalidCodec("A valid video codec could not be found for this file.", path);

	// Create a new stream
	st = avformat_new_stream(oc, codec);
	if (!st)
		throw OutOfMemory("Could not allocate memory for the video stream.", path);

	// Set default values
    avcodec_get_context_defaults3(st->codec, codec);

	c = st->codec;
	c->codec_id = codec->id;
#if LIBAVFORMAT_VERSION_MAJOR >= 53
	c->codec_type = AVMEDIA_TYPE_VIDEO;
#else
	c->codec_type = CODEC_TYPE_VIDEO;
#endif

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
	c->time_base.num = info.video_timebase.num;
	c->time_base.den = info.video_timebase.den;
	c->gop_size = 12; /* TODO: add this to "info"... emit one intra frame every twelve frames at most */
	c->pix_fmt = PIX_FMT_YUV420P;
	if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
		/* just for testing, we also add B frames */
		c->max_b_frames = 2;
	}
	if (c->codec_id == CODEC_ID_MPEG1VIDEO) {
		/* Needed to avoid using macroblocks in which some coeffs overflow.
		 This does not happen with normal video, it just happens here as
		 the motion of the chroma plane does not match the luma plane. */
		c->mb_decision = 2;
	}
	// some formats want stream headers to be separate
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return st;
}

// open audio codec
void FFmpegWriter::open_audio(AVFormatContext *oc, AVStream *st)
{
	AVCodec *codec;
	audio_codec = st->codec;

	// Set number of threads equal to number of processors + 1
	audio_codec->thread_count = omp_get_num_procs();

	// Find the audio encoder
	codec = avcodec_find_encoder(audio_codec->codec_id);
	if (!codec)
		throw InvalidCodec("Could not find codec", path);

	// Open the codec
	if (avcodec_open2(audio_codec, codec, NULL) < 0)
		throw InvalidCodec("Could not open codec", path);

	// Calculate the size of the input frame (i..e how many samples per packet), and the output buffer
	// TODO: Ugly hack for PCM codecs (will be removed ASAP with new PCM support to compute the input frame size in samples
	if (audio_codec->frame_size <= 1) {
		// No frame size found... so calculate
		audio_input_frame_size = 50000 / info.channels;

		switch (st->codec->codec_id) {
		case CODEC_ID_PCM_S16LE:
		case CODEC_ID_PCM_S16BE:
		case CODEC_ID_PCM_U16LE:
		case CODEC_ID_PCM_U16BE:
			audio_input_frame_size >>= 1;
			break;
		default:
			break;
		}
	} else {
		// Set frame size based on the codec
		audio_input_frame_size = audio_codec->frame_size * info.channels;
	}

	// Set the initial frame size (since it might change during resampling)
	initial_audio_input_frame_size = audio_input_frame_size;

	// Allocate array for samples
	samples = new int16_t[AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];

	// Set audio output buffer (used to store the encoded audio)
	audio_outbuf_size = AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE;
	audio_outbuf = new uint8_t[audio_outbuf_size];

}

// open video codec
void FFmpegWriter::open_video(AVFormatContext *oc, AVStream *st)
{
	AVCodec *codec;
	video_codec = st->codec;

	// Set number of threads equal to number of processors + 1
	video_codec->thread_count = omp_get_num_procs();

	/* find the video encoder */
	codec = avcodec_find_encoder(video_codec->codec_id);
	if (!codec)
		throw InvalidCodec("Could not find codec", path);

	/* open the codec */
	if (avcodec_open2(video_codec, codec, NULL) < 0)
		throw InvalidCodec("Could not open codec", path);
}

// write all queued frames' audio to the video file
void FFmpegWriter::write_audio_packets(bool final)
{
	// Create a resampler (only once)
	if (!resampler)
		resampler = new AudioResampler();
	AudioResampler *new_sampler = resampler;

	#pragma omp task firstprivate(new_sampler)
	{
		// Init audio buffers / variables
		int total_frame_samples = 0;
		int frame_position = 0;
		int channels_in_frame = 0;
		int sample_rate_in_frame = 0;
		int samples_in_frame = 0;

		// Create a new array (to hold all S16 audio samples, for the current queued frames
		int16_t* frame_samples = new int16_t[(queued_audio_frames.size() * AVCODEC_MAX_AUDIO_FRAME_SIZE) + FF_INPUT_BUFFER_PADDING_SIZE];

		// create a new array (to hold all the re-sampled audio, for the current queued frames)
		int16_t* converted_audio = new int16_t[(queued_audio_frames.size() * AVCODEC_MAX_AUDIO_FRAME_SIZE) + FF_INPUT_BUFFER_PADDING_SIZE];

		// Loop through each queued audio frame
		while (!queued_audio_frames.empty())
		{
			// Get front frame (from the queue)
			tr1::shared_ptr<Frame> frame = queued_audio_frames.front();

			// Get the audio details from this frame
			sample_rate_in_frame = info.sample_rate; // resampling happens when getting the interleaved audio samples below
			samples_in_frame = frame->GetAudioSamplesCount(); // this is updated if resampling happens
			channels_in_frame = frame->GetAudioChannelsCount();

			// Get audio sample array
			float* frame_samples_float = frame->GetInterleavedAudioSamples(info.sample_rate, new_sampler, &samples_in_frame);

			// Calculate total samples
			total_frame_samples = samples_in_frame * channels_in_frame;

			// Translate audio sample values back to 16 bit integers
			for (int s = 0; s < total_frame_samples; s++, frame_position++)
				// Translate sample value and copy into buffer
				frame_samples[frame_position] = int(frame_samples_float[s] * (1 << 15));

			// Deallocate float array
			delete[] frame_samples_float;

			// Remove front item
			queued_audio_frames.pop_front();

		} // end while


		// Update total samples (since we've combined all queued frames)
		total_frame_samples = frame_position;
		int remaining_frame_samples = total_frame_samples;
		int samples_position = 0;

		// Re-sample audio samples (into additional channels or changing the sample format / number format)
		// The sample rate has already been resampled using the GetInterleavedAudioSamples method.
		if (!final && (audio_codec->sample_fmt != AV_SAMPLE_FMT_S16 || info.channels != channels_in_frame)) {

			// Audio needs to be converted
			// Create an audio resample context object (used to convert audio samples)
			ReSampleContext *resampleCtx = av_audio_resample_init(
					info.channels, channels_in_frame,
					info.sample_rate, sample_rate_in_frame,
					audio_codec->sample_fmt, AV_SAMPLE_FMT_S16, 0, 0, 0, 0.0f);

			if (!resampleCtx)
				throw ResampleError("Failed to resample & convert audio samples for encoding.", path);
			else {
				// FFmpeg audio resample & sample format conversion
				audio_resample(resampleCtx, (short *) converted_audio, (short *) frame_samples, total_frame_samples);

				// Update total frames & input frame size (due to bigger or smaller data types)
				total_frame_samples *= (av_get_bytes_per_sample(audio_codec->sample_fmt) / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16));
				audio_input_frame_size = initial_audio_input_frame_size * (av_get_bytes_per_sample(audio_codec->sample_fmt) / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16));

				// Set remaining samples
				remaining_frame_samples = total_frame_samples;

				// Copy audio samples over original samples
				memcpy(frame_samples, converted_audio, total_frame_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16));

				// Close context
				audio_resample_close(resampleCtx);
			}
		}

		// Loop until no more samples
		while (remaining_frame_samples > 0 || final) {
			// Get remaining samples needed for this packet
			int remaining_packet_samples = audio_input_frame_size - audio_input_position;

			// Determine how many samples we need
			int diff = 0;
			if (remaining_frame_samples >= remaining_packet_samples)
				diff = remaining_packet_samples;
			else if (remaining_frame_samples < remaining_packet_samples)
				diff = remaining_frame_samples;

			// Copy frame samples into the packet samples array
			if (!final)
				memcpy(samples + audio_input_position, frame_samples + samples_position, diff * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16));

			// Increment counters
			audio_input_position += diff;
			samples_position += diff;
			remaining_frame_samples -= diff;
			remaining_packet_samples -= diff;

			// Do we have enough samples to proceed?
			if (audio_input_position < audio_input_frame_size && !final)
				// Not enough samples to encode... so wait until the next frame
				break;

			// Increment PTS (in samples and scaled to the codec's timebase)
			write_audio_count += av_rescale_q(audio_input_position / av_get_bytes_per_sample(audio_codec->sample_fmt), (AVRational){1, info.sample_rate}, audio_codec->time_base);

			// Create AVFrame (and fill it with samples)
			AVFrame *frame_final = avcodec_alloc_frame();
			frame_final->nb_samples = audio_input_position / av_get_bytes_per_sample(audio_codec->sample_fmt);
			frame_final->pts = write_audio_count; // Set the AVFrame's PTS
			avcodec_fill_audio_frame(frame_final, audio_codec->channels, audio_codec->sample_fmt, (uint8_t *) samples,
					audio_input_position * av_get_bytes_per_sample(audio_codec->sample_fmt), 1);

			// Init the packet
			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.data = NULL;
			pkt.size = 0;

			// Set the packet's PTS prior to encoding
			pkt.pts = pkt.dts = write_audio_count;

			/* encode the audio samples */
			int got_packet_ptr = 0;
			int error_code = avcodec_encode_audio2(audio_codec, &pkt, frame_final, &got_packet_ptr);

			/* if zero size, it means the image was buffered */
			if (error_code == 0 && got_packet_ptr) {

				// Since the PTS can change during encoding, set the value again.  This seems like a huge hack,
				// but it fixes lots of PTS related issues when I do this.
				pkt.pts = pkt.dts = write_audio_count;

				// Scale the PTS to the audio stream timebase (which is sometimes different than the codec's timebase)
				if (pkt.pts != AV_NOPTS_VALUE)
					pkt.pts = av_rescale_q(pkt.pts, audio_codec->time_base, audio_st->time_base);
				if (pkt.dts != AV_NOPTS_VALUE)
					pkt.dts = av_rescale_q(pkt.dts, audio_codec->time_base, audio_st->time_base);
				if (pkt.duration > 0)
					pkt.duration = av_rescale_q(pkt.duration, audio_codec->time_base, audio_st->time_base);

				// set stream
				pkt.stream_index = audio_st->index;
				pkt.flags |= AV_PKT_FLAG_KEY;

				/* write the compressed frame in the media file */
				int error_code = av_interleaved_write_frame(oc, &pkt);
				if (error_code != 0)
				{
					string error_description = "Unknown";

					#ifdef av_err2str
						error_description = av_err2str(error_code);
					#endif

					cout << "error: " << error_code << ": " << error_description << endl;
					throw ErrorEncodingAudio("Error while writing compressed audio frame", write_audio_count);
				}
			}

			if (error_code < 0)
				cout << "Error encoding audio: " << error_code << endl;

			// deallocate AVFrame
			//av_free(frame_final->data[0]);
			av_free(frame_final);

			// deallocate memory for packet
			av_free_packet(&pkt);

			// Reset position
			audio_input_position = 0;
			final = false;
		}

		// Delete arrays
		delete[] frame_samples;
		delete[] converted_audio;

	} // end task
}

// Allocate an AVFrame object
AVFrame* FFmpegWriter::allocate_avframe(PixelFormat pix_fmt, int width, int height, int *buffer_size)
{
	// Create an RGB AVFrame
	AVFrame *new_av_frame = NULL;
	uint8_t *new_buffer = NULL;

	// Allocate an AVFrame structure
	new_av_frame = avcodec_alloc_frame();
	if (new_av_frame == NULL)
		throw OutOfMemory("Could not allocate AVFrame", path);

	// Determine required buffer size and allocate buffer
	*buffer_size = avpicture_get_size(pix_fmt, width, height);
	new_buffer = new uint8_t[*buffer_size];

	// Attach buffer to AVFrame
	avpicture_fill((AVPicture *)new_av_frame, new_buffer, pix_fmt, width, height);

	// return AVFrame
	return new_av_frame;
}

// process video frame
void FFmpegWriter::process_video_packet(tr1::shared_ptr<Frame> frame)
{
	// Determine the height & width of the source image
	int source_image_width = frame->GetWidth();
	int source_image_height = frame->GetHeight();
	// If visualizing waveform (replace image with waveform image)
	if (info.visualize)
	{
		source_image_width = info.width;
		source_image_height = info.height;
	}

	// Init rescalers (if not initialized yet)
	if (image_rescalers.size() == 0)
		InitScalers(source_image_width, source_image_height);

	// Get a unique rescaler (for this thread)
	SwsContext *scaler = image_rescalers[rescaler_position];
	rescaler_position++;
	if (rescaler_position == num_of_rescalers)
		rescaler_position = 0;

	#pragma omp task firstprivate(frame, scaler, source_image_width, source_image_height)
	{
		// Allocate an RGB frame & final output frame
		int bytes_source = 0;
		int bytes_final = 0;
		AVFrame *frame_source = NULL;
		const Magick::PixelPacket *pixel_packets = NULL;

		// Get a list of pixels from source image
		pixel_packets = frame->GetPixels();

		// Init AVFrame for source image & final (converted image)
		frame_source = allocate_avframe(PIX_FMT_RGB24, source_image_width, source_image_height, &bytes_source);
		AVFrame *frame_final = allocate_avframe(video_codec->pix_fmt, info.width, info.height, &bytes_final);

		// Fill the AVFrame with RGB image data
		int source_total_pixels = source_image_width * source_image_height;
		for (int packet = 0, row = 0; packet < source_total_pixels; packet++, row+=3)
		{
			// Update buffer (which is already linked to the AVFrame: pFrameRGB)
			// Each color needs to be 8 bit (so I'm bit shifting the 16 bit ints)
			frame_source->data[0][row] = pixel_packets[packet].red >> 8;
			frame_source->data[0][row+1] = pixel_packets[packet].green >> 8;
			frame_source->data[0][row+2] = pixel_packets[packet].blue >> 8;
		}

		// Resize & convert pixel format
		sws_scale(scaler, frame_source->data, frame_source->linesize, 0,
				source_image_height, frame_final->data, frame_final->linesize);

		// Add resized AVFrame to av_frames map
		#pragma omp critical (av_frames_section)
		add_avframe(frame, frame_final);

		// Deallocate memory
		av_free(frame_source->data[0]);
		av_free(frame_source);

		if (info.visualize)
			// Deallocate the waveform's image (if needed)
			frame->ClearWaveform();

	} // end task

}

// write video frame
void FFmpegWriter::write_video_packet(tr1::shared_ptr<Frame> frame, AVFrame* frame_final)
{
	if (oc->oformat->flags & AVFMT_RAWPICTURE) {
		// Raw video case.
		AVPacket pkt;
		av_init_packet(&pkt);

		pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index= video_st->index;
		pkt.data= (uint8_t *)frame_final;
		pkt.size= sizeof(AVPicture);

		// Increment PTS (in frames and scaled to the codec's timebase)
		write_video_count += av_rescale_q(1, (AVRational){info.fps.den, info.fps.num}, video_codec->time_base);
		pkt.pts = write_video_count;

		/* write the compressed frame in the media file */
		int error_code = av_interleaved_write_frame(oc, &pkt);
		if (error_code != 0)
		{
			string error_description = "Unknown";

			#ifdef av_err2str
				error_description = av_err2str(error_code);
			#endif

			throw ErrorEncodingVideo("Error while writing raw video frame", frame->number);
		}

		// Deallocate packet
		av_free_packet(&pkt);

	} else {

		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		pkt.pts = pkt.dts = AV_NOPTS_VALUE;

		// Pointer for video buffer (if using old FFmpeg version)
		uint8_t *video_outbuf = NULL;

		// Increment PTS (in frames and scaled to the codec's timebase)
		write_video_count += av_rescale_q(1, (AVRational){info.fps.den, info.fps.num}, video_codec->time_base);

		// Assign the initial AVFrame PTS from the frame counter
		frame_final->pts = write_video_count;

		/* encode the image */
		int got_packet_ptr = 0;
		int error_code = 0;
		#if LIBAVFORMAT_VERSION_MAJOR >= 54
			// Newer versions of FFMpeg
			error_code = avcodec_encode_video2(video_codec, &pkt, frame_final, &got_packet_ptr);

		#else
			// Older versions of FFmpeg (much sloppier)

			// Encode Picture and Write Frame
			int video_outbuf_size = 200000;
			video_outbuf = new uint8_t[200000];

			/* encode the image */
			int out_size = avcodec_encode_video(video_codec, video_outbuf, video_outbuf_size, frame_final);

			/* if zero size, it means the image was buffered */
			if (out_size > 0) {
				if(video_codec->coded_frame->key_frame)
					pkt.flags |= AV_PKT_FLAG_KEY;
				pkt.data= video_outbuf;
				pkt.size= out_size;

				// got data back (so encode this frame)
				got_packet_ptr = 1;
			}
		#endif

		/* if zero size, it means the image was buffered */
		if (error_code == 0 && got_packet_ptr) {

			// Since the PTS can change during encoding, set the value again.  This seems like a huge hack,
			// but it fixes lots of PTS related issues when I do this.
			//pkt.pts = pkt.dts = write_video_count;

			// set the timestamp
			if (pkt.pts != AV_NOPTS_VALUE)
				pkt.pts = av_rescale_q(pkt.pts, video_codec->time_base, video_st->time_base);
			if (pkt.dts != AV_NOPTS_VALUE)
				pkt.dts = av_rescale_q(pkt.dts, video_codec->time_base, video_st->time_base);
			if (pkt.duration > 0)
				pkt.duration = av_rescale_q(pkt.duration, video_codec->time_base, video_st->time_base);
			pkt.stream_index = video_st->index;

			/* write the compressed frame in the media file */
			int error_code = av_interleaved_write_frame(oc, &pkt);
			if (error_code != 0)
			{
				string error_description = "Unknown";

				#ifdef av_err2str
					error_description = av_err2str(error_code);
				#endif

				cout << "error: " << error_code << ": " << error_description << endl;
				throw ErrorEncodingVideo("Error while writing compressed video frame", frame->number);
			}
		}

		// Deallocate memory (if needed)
		if (video_outbuf)
			delete[] video_outbuf;

		// Deallocate packet
		av_free_packet(&pkt);
	}
}

// Output the ffmpeg info about this format, streams, and codecs (i.e. dump format)
void FFmpegWriter::OutputStreamInfo()
{
	// output debug info
	av_dump_format(oc, 0, path.c_str(), 1);
}

// Init a collection of software rescalers (thread safe)
void FFmpegWriter::InitScalers(int source_width, int source_height)
{
	// Get the codec
	AVCodecContext *c;
	c = video_st->codec;

	// Init software rescalers vector (many of them, one for each thread)
	for (int x = 0; x < num_of_rescalers; x++)
	{
		// Init the software scaler from FFMpeg
		img_convert_ctx = sws_getContext(source_width, source_height, PIX_FMT_RGB24, info.width, info.height, c->pix_fmt, SWS_FAST_BILINEAR, NULL, NULL, NULL);

		// Add rescaler to vector
		image_rescalers.push_back(img_convert_ctx);
	}
}

// Remove & deallocate all software scalers
void FFmpegWriter::RemoveScalers()
{
	// Close all rescalers
	for (int x = 0; x < num_of_rescalers; x++)
		sws_freeContext(image_rescalers[x]);

	// Clear vector
	image_rescalers.clear();
}
