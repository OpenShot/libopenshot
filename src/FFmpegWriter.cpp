/**
 * @file
 * @brief Source file for FFmpegWriter class
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

#include "../include/FFmpegWriter.h"

using namespace openshot;

FFmpegWriter::FFmpegWriter(string path) :
		path(path), fmt(NULL), oc(NULL), audio_st(NULL), video_st(NULL), audio_pts(0), video_pts(0), samples(NULL),
		audio_outbuf(NULL), audio_outbuf_size(0), audio_input_frame_size(0), audio_input_position(0),
		initial_audio_input_frame_size(0), img_convert_ctx(NULL), cache_size(8), num_of_rescalers(32),
		rescaler_position(0), video_codec(NULL), audio_codec(NULL), is_writing(false), write_video_count(0), write_audio_count(0),
		original_sample_rate(0), original_channels(0), avr(NULL), avr_planar(NULL), is_open(false), prepare_streams(false),
		write_header(false), write_trailer(false), audio_encoder_buffer_size(0), audio_encoder_buffer(NULL)
{

	// Disable audio & video (so they can be independently enabled)
	info.has_audio = false;
	info.has_video = false;

	// Initialize FFMpeg, and register all formats and codecs
	AV_REGISTER_ALL

	// auto detect format
	auto_detect_format();
}

// Open the writer
void FFmpegWriter::Open()
{
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
void FFmpegWriter::auto_detect_format()
{
	// Auto detect the output format from the name. default is mpeg.
	fmt = av_guess_format(NULL, path.c_str(), NULL);
	if (!fmt)
		throw InvalidFormat("Could not deduce output format from file extension.", path);

	// Allocate the output media context
	AV_OUTPUT_CONTEXT(&oc, path.c_str());
	if (!oc)
		throw OutOfMemory("Could not allocate memory for AVFormatContext.", path);

	// Set the AVOutputFormat for the current AVFormatContext
	oc->oformat = fmt;

	// Update codec names
	if (fmt->video_codec != AV_CODEC_ID_NONE && info.has_video)
		// Update video codec name
		info.vcodec = avcodec_find_encoder(fmt->video_codec)->name;

	if (fmt->audio_codec != AV_CODEC_ID_NONE && info.has_audio)
		// Update audio codec name
		info.acodec = avcodec_find_encoder(fmt->audio_codec)->name;
}

// initialize streams
void FFmpegWriter::initialize_streams()
{
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::initialize_streams", "fmt->video_codec", fmt->video_codec, "fmt->audio_codec", fmt->audio_codec, "AV_CODEC_ID_NONE", AV_CODEC_ID_NONE, "", -1, "", -1, "", -1);

	// Add the audio and video streams using the default format codecs and initialize the codecs
	video_st = NULL;
	audio_st = NULL;
	if (fmt->video_codec != AV_CODEC_ID_NONE && info.has_video)
		// Add video stream
		video_st = add_video_stream();

	if (fmt->audio_codec != AV_CODEC_ID_NONE && info.has_audio)
		// Add audio stream
		audio_st = add_audio_stream();
}

// Set video export options
void FFmpegWriter::SetVideoOptions(bool has_video, string codec, Fraction fps, int width, int height, Fraction pixel_ratio, bool interlaced, bool top_field_first, int bit_rate)
{
	// Set the video options
	if (codec.length() > 0)
	{
		AVCodec *new_codec = avcodec_find_encoder_by_name(codec.c_str());
		if (new_codec == NULL)
			throw InvalidCodec("A valid video codec could not be found for this file.", path);
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
	if (bit_rate >= 1000)			// bit_rate is the bitrate in b/s
		info.video_bit_rate = bit_rate;
	if ((bit_rate >= 0) && 	(bit_rate < 64)	)	// bit_rate is the bitrate in crf
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

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::SetVideoOptions (" + codec + ")", "width", width, "height", height, "size.num", size.num, "size.den", size.den, "fps.num", fps.num, "fps.den", fps.den);

	// Enable / Disable video
	info.has_video = has_video;
}

// Set audio export options
void FFmpegWriter::SetAudioOptions(bool has_audio, string codec, int sample_rate, int channels, ChannelLayout channel_layout, int bit_rate)
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
	info.channel_layout = channel_layout;

	// init resample options (if zero)
	if (original_sample_rate == 0)
		original_sample_rate = info.sample_rate;
	if (original_channels == 0)
		original_channels = info.channels;

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::SetAudioOptions (" + codec + ")", "sample_rate", sample_rate, "channels", channels, "bit_rate", bit_rate, "", -1, "", -1, "", -1);

	// Enable / Disable audio
	info.has_audio = has_audio;
}

// Set custom options (some codecs accept additional params)
void FFmpegWriter::SetOption(StreamType stream, string name, string value)
{
	// Declare codec context
	AVCodecContext *c = NULL;
	AVStream *st = NULL;
	stringstream convert(value);

	if (info.has_video && stream == VIDEO_STREAM && video_st) {
		st = video_st;
		// Get codec context
		c = AV_GET_CODEC_PAR_CONTEXT(st, video_codec);
	}
	else if (info.has_audio && stream == AUDIO_STREAM && audio_st) {
		st = audio_st;
		// Get codec context
		c = AV_GET_CODEC_PAR_CONTEXT(st, audio_codec);
	}
	else
		throw NoStreamsFound("The stream was not found. Be sure to call PrepareStreams() first.", path);

	// Init AVOption
	const AVOption *option = NULL;

	// Was a codec / stream found?
	if (c)
		// Find AVOption (if it exists)
		option = AV_OPTION_FIND(c->priv_data, name.c_str());

	// Was option found?
	if (option || (name == "g" || name == "qmin" || name == "qmax" || name == "max_b_frames" || name == "mb_decision" ||
			       name == "level" || name == "profile" || name == "slices" || name == "rc_min_rate"  || name == "rc_max_rate" ||
					 	 name == "crf"))
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

		else if (name == "rc_buffer_size")
			// Buffer size
			convert >> c->rc_buffer_size;

		else if (name == "crf") {
			// encode quality and special settings like lossless
			// This might be better in an extra methods as more options
			// and way to set quality are possible
			#if  LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 39, 101)
					switch (c->codec_id) {
						#if (LIBAVCODEC_VERSION_MAJOR >= 58)
						case AV_CODEC_ID_AV1 :
							c->bit_rate = 0;
							av_opt_set_int(c->priv_data, "crf", min(stoi(value),63), 0);
							break;
						#endif
						case AV_CODEC_ID_VP8 :
							c->bit_rate = 10000000;
							av_opt_set_int(c->priv_data, "crf", max(min(stoi(value),63),4), 0); // 4-63
							break;
						case AV_CODEC_ID_VP9 :
							c->bit_rate = 0;		// Must be zero!
							av_opt_set_int(c->priv_data, "crf", min(stoi(value),63), 0); // 0-63
							if (stoi(value) == 0) {
								av_opt_set(c->priv_data, "preset", "veryslow", 0);
								av_opt_set_int(c->priv_data, "lossless", 1, 0);
							 }
							 break;
						case AV_CODEC_ID_H264 :
							av_opt_set_int(c->priv_data, "crf", min(stoi(value),51), 0); // 0-51
							if (stoi(value) == 0) {
								av_opt_set(c->priv_data, "preset", "veryslow", 0);
						 	}
							break;
						case AV_CODEC_ID_H265 :
							av_opt_set_int(c->priv_data, "crf", min(stoi(value),51), 0); // 0-51
							if (stoi(value) == 0) {
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
									mbs = 380.0;
								}
								else {
									mbs *= pow(0.912,info.video_bit_rate);
								}
							}
							c->bit_rate = (int)(mbs);
					}
			#endif
		}

		else
			// Set AVOption
			AV_OPTION_SET(st, c->priv_data, name.c_str(), value.c_str(), c);

		ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::SetOption (" + (string)name + ")", "stream == VIDEO_STREAM", stream == VIDEO_STREAM, "", -1, "", -1, "", -1, "", -1, "", -1);

	}
	else
		throw InvalidOptions("The option is not valid for this codec.", path);

}

/// Determine if codec name is valid
bool FFmpegWriter::IsValidCodec(string codec_name) {
	// Initialize FFMpeg, and register all formats and codecs
	AV_REGISTER_ALL

	// Find the codec (if any)
	if (avcodec_find_encoder_by_name(codec_name.c_str()) == NULL)
		return false;
	else
		return true;
}

// Prepare & initialize streams and open codecs
void FFmpegWriter::PrepareStreams()
{
	if (!info.has_audio && !info.has_video)
		throw InvalidOptions("No video or audio options have been set.  You must set has_video or has_audio (or both).", path);

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::PrepareStreams [" + path + "]", "info.has_audio", info.has_audio, "info.has_video", info.has_video, "", -1, "", -1, "", -1, "", -1);

	// Initialize the streams (i.e. add the streams)
	initialize_streams();

	// Mark as 'prepared'
	prepare_streams = true;
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

    // Force the output filename (which doesn't always happen for some reason)
		snprintf(oc->AV_FILENAME, sizeof(oc->AV_FILENAME), "%s", path.c_str());

	// Write the stream header, if any
	// TODO: add avoptions / parameters instead of NULL

	// Add general metadata (if any)
	for(std::map<string, string>::iterator iter = info.metadata.begin(); iter != info.metadata.end(); ++iter)
	{
		av_dict_set(&oc->metadata, iter->first.c_str(), iter->second.c_str(), 0);
	}

	if (avformat_write_header(oc, NULL) != 0) {
					throw InvalidFile("Could not write header to file.", path);
	};

	// Mark as 'written'
	write_header = true;

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::WriteHeader", "", -1, "", -1, "", -1, "", -1, "", -1, "", -1);
}

// Add a frame to the queue waiting to be encoded.
void FFmpegWriter::WriteFrame(std::shared_ptr<Frame> frame)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw WriterClosed("The FFmpegWriter is closed.  Call Open() before calling this method.", path);

	// Add frame pointer to "queue", waiting to be processed the next
	// time the WriteFrames() method is called.
	if (info.has_video && video_st)
		spooled_video_frames.push_back(frame);

	if (info.has_audio && audio_st)
		spooled_audio_frames.push_back(frame);

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::WriteFrame", "frame->number", frame->number, "spooled_video_frames.size()", spooled_video_frames.size(), "spooled_audio_frames.size()", spooled_audio_frames.size(), "cache_size", cache_size, "is_writing", is_writing, "", -1);

	// Write the frames once it reaches the correct cache size
	if (spooled_video_frames.size() == cache_size || spooled_audio_frames.size() == cache_size)
	{
		// Is writer currently writing?
		if (!is_writing)
			// Write frames to video file
			write_queued_frames();

		else
		{
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
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_queued_frames", "spooled_video_frames.size()", spooled_video_frames.size(), "spooled_audio_frames.size()", spooled_audio_frames.size(), "", -1, "", -1, "", -1, "", -1);

	// Flip writing flag
	is_writing = true;

	// Transfer spool to queue
	queued_video_frames = spooled_video_frames;
	queued_audio_frames = spooled_audio_frames;

	// Empty spool
	spooled_video_frames.clear();
	spooled_audio_frames.clear();

	// Set the number of threads in OpenMP
	omp_set_num_threads(OPEN_MP_NUM_PROCESSORS);
	// Allow nested OpenMP sections
	omp_set_nested(true);

	// Create blank exception
	bool has_error_encoding_video = false;

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
				std::shared_ptr<Frame> frame = queued_video_frames.front();

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
				std::shared_ptr<Frame> frame = processed_frames.front();

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
						bool success = write_video_packet(frame, frame_final);
						if (!success)
							has_error_encoding_video = true;
					}
				}

				// Remove front item
				processed_frames.pop_front();
			}

			// Loop through, and deallocate AVFrames
			while (!deallocate_frames.empty())
			{
				// Get front frame (from the queue)
				std::shared_ptr<Frame> frame = deallocate_frames.front();

				// Does this frame's AVFrame still exist
				if (av_frames.count(frame))
				{
					// Get AVFrame
					AVFrame *av_frame = av_frames[frame];

					// Deallocate AVPicture and AVFrame
					av_freep(&(av_frame->data[0]));
					AV_FREE_FRAME(&av_frame);
					av_frames.erase(frame);
				}

				// Remove front item
				deallocate_frames.pop_front();
			}

			// Done writing
			is_writing = false;

		} // end omp single
	} // end omp parallel

	// Raise exception from main thread
	if (has_error_encoding_video)
		throw ErrorEncodingVideo("Error while writing raw video frame", -1);
}

// Write a block of frames from a reader
void FFmpegWriter::WriteFrame(ReaderBase* reader, int64_t start, int64_t length)
{
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::WriteFrame (from Reader)", "start", start, "length", length, "", -1, "", -1, "", -1, "", -1);

	// Loop through each frame (and encoded it)
	for (int64_t number = start; number <= length; number++)
	{
		// Get the frame
		std::shared_ptr<Frame> f = reader->GetFrame(number);

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

	// Mark as 'written'
	write_trailer = true;

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::WriteTrailer", "", -1, "", -1, "", -1, "", -1, "", -1, "", -1);
}

// Flush encoders
void FFmpegWriter::flush_encoders()
{
	if (info.has_audio && audio_codec && AV_GET_CODEC_TYPE(audio_st) == AVMEDIA_TYPE_AUDIO && AV_GET_CODEC_ATTRIBUTES(audio_st, audio_codec)->frame_size <= 1)
		return;
#if (LIBAVFORMAT_VERSION_MAJOR < 58)
	if (info.has_video && video_codec && AV_GET_CODEC_TYPE(video_st) == AVMEDIA_TYPE_VIDEO && (oc->oformat->flags & AVFMT_RAWPICTURE) && AV_FIND_DECODER_CODEC_ID(video_st) == AV_CODEC_ID_RAWVIDEO)
		return;
#endif

    int error_code = 0;
    int stop_encoding = 1;

    // FLUSH VIDEO ENCODER
    if (info.has_video)
		for (;;) {

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

			#if IS_FFMPEG_3_2
			#pragma omp critical (write_video_packet)
			{
				// Encode video packet (latest version of FFmpeg)
				error_code = avcodec_send_frame(video_codec, NULL);
				got_packet = 0;
				while (error_code >= 0) {
					error_code = avcodec_receive_packet(video_codec, &pkt);
					if (error_code == AVERROR(EAGAIN)|| error_code == AVERROR_EOF) {
						got_packet = 0;
						// Write packet
						avcodec_flush_buffers(video_codec);
						break;
					}
					if (pkt.pts != AV_NOPTS_VALUE)
						pkt.pts = av_rescale_q(pkt.pts, video_codec->time_base, video_st->time_base);
					if (pkt.dts != AV_NOPTS_VALUE)
						pkt.dts = av_rescale_q(pkt.dts, video_codec->time_base, video_st->time_base);
					if (pkt.duration > 0)
						pkt.duration = av_rescale_q(pkt.duration, video_codec->time_base, video_st->time_base);
					pkt.stream_index = video_st->index;
					error_code = av_interleaved_write_frame(oc, &pkt);
				}
			}
			#else

				#if LIBAVFORMAT_VERSION_MAJOR >= 54
				// Encode video packet (older than FFmpeg 3.2)
				error_code = avcodec_encode_video2(video_codec, &pkt, NULL, &got_packet);

				#else
					// Encode video packet (even older version of FFmpeg)
					int video_outbuf_size = 0;

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
			#endif

			if (error_code < 0) {
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::flush_encoders ERROR [" + (string)av_err2str(error_code) + "]", "error_code", error_code, "", -1, "", -1, "", -1, "", -1, "", -1);
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
			if (error_code < 0) {
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::flush_encoders ERROR [" + (string)av_err2str(error_code) + "]", "error_code", error_code, "", -1, "", -1, "", -1, "", -1, "", -1);
			}

			// Deallocate memory (if needed)
			if (video_outbuf)
				av_freep(&video_outbuf);
		}

    // FLUSH AUDIO ENCODER
    if (info.has_audio)
		for (;;) {

			// Increment PTS (in samples and scaled to the codec's timebase)
#if LIBAVFORMAT_VERSION_MAJOR >= 54
			// for some reason, it requires me to multiply channels X 2
			write_audio_count += av_rescale_q(audio_input_position / (audio_codec->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)), (AVRational){1, info.sample_rate}, audio_codec->time_base);
#else
			write_audio_count += av_rescale_q(audio_input_position / audio_codec->channels, (AVRational){1, info.sample_rate}, audio_codec->time_base);
#endif

			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.data = NULL;
			pkt.size = 0;
			pkt.pts = pkt.dts = write_audio_count;

			/* encode the image */
			int got_packet = 0;
			#if IS_FFMPEG_3_2
			avcodec_send_frame(audio_codec, NULL);
			got_packet = 0;
			#else
			error_code = avcodec_encode_audio2(audio_codec, &pkt, NULL, &got_packet);
			#endif
			if (error_code < 0) {
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::flush_encoders ERROR [" + (string)av_err2str(error_code) + "]", "error_code", error_code, "", -1, "", -1, "", -1, "", -1, "", -1);
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
			error_code = av_interleaved_write_frame(oc, &pkt);
			if (error_code < 0) {
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::flush_encoders ERROR [" + (string)av_err2str(error_code) + "]", "error_code", error_code, "", -1, "", -1, "", -1, "", -1, "", -1);
			}

			// deallocate memory for packet
			AV_FREE_PACKET(&pkt);
		}


}

// Close the video codec
void FFmpegWriter::close_video(AVFormatContext *oc, AVStream *st)
{
	AV_FREE_CONTEXT(video_codec);
	video_codec = NULL;
}

// Close the audio codec
void FFmpegWriter::close_audio(AVFormatContext *oc, AVStream *st)
{
	AV_FREE_CONTEXT(audio_codec);
	audio_codec = NULL;

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
}

// Close the writer
void FFmpegWriter::Close()
{
	// Write trailer (if needed)
	if (!write_trailer)
		WriteTrailer();

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
		av_freep(AV_GET_CODEC_ATTRIBUTES(&oc->streams[i], &oc->streams[i]));
		av_freep(&oc->streams[i]);
	}

	if (!(fmt->flags & AVFMT_NOFILE)) {
		/* close the output file */
		avio_close(oc->pb);
	}

	// Reset frame counters
	write_video_count = 0;
	write_audio_count = 0;

	// Free the context
	av_freep(&oc);

	// Close writer
	is_open = false;
	prepare_streams = false;
	write_header = false;
	write_trailer = false;

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::Close", "", -1, "", -1, "", -1, "", -1, "", -1, "", -1);
}

// Add an AVFrame to the cache
void FFmpegWriter::add_avframe(std::shared_ptr<Frame> frame, AVFrame* av_frame)
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
		AV_FREE_FRAME(&av_frame);
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
	AV_FORMAT_NEW_STREAM(oc, audio_codec, codec, st)

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
	int channel_layout = info.channel_layout;
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
#if (LIBAVCODEC_VERSION_MAJOR >= 57)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
#else
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
#endif

	AV_COPY_PARAMS_FROM_CONTEXT(st, c);
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::add_audio_stream", "c->codec_id", c->codec_id, "c->bit_rate", c->bit_rate, "c->channels", c->channels, "c->sample_fmt", c->sample_fmt, "c->channel_layout", c->channel_layout, "c->sample_rate", c->sample_rate);

	return st;
}

// Add a video output stream
AVStream* FFmpegWriter::add_video_stream()
{
	AVCodecContext *c;
	AVStream *st;

	// Find the video codec
	AVCodec *codec = avcodec_find_encoder_by_name(info.vcodec.c_str());
	if (codec == NULL)
		throw InvalidCodec("A valid video codec could not be found for this file.", path);

	// Create a new video stream
	AV_FORMAT_NEW_STREAM(oc, video_codec, codec, st)

	c->codec_id = codec->id;
#if LIBAVFORMAT_VERSION_MAJOR >= 53
	c->codec_type = AVMEDIA_TYPE_VIDEO;
#else
	c->codec_type = CODEC_TYPE_VIDEO;
#endif

	/* Init video encoder options */
	if (info.video_bit_rate >= 1000) {
		c->bit_rate = info.video_bit_rate;
		if (info.video_bit_rate >= 1500000) {
			c->qmin = 2;
			c->qmax = 30;
		}
		// Here should be the setting for low fixed bitrate
		// Defaults are used because mpeg2 otherwise had problems
	}
	else {
		c->qmin = 0;
		c->qmax = 63;
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
	#if LIBAVFORMAT_VERSION_MAJOR >= 56
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
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
#else
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
#endif

	// Find all supported pixel formats for this codec
    const PixelFormat* supported_pixel_formats = codec->pix_fmts;
    while (supported_pixel_formats != NULL && *supported_pixel_formats != PIX_FMT_NONE) {
        // Assign the 1st valid pixel format (if one is missing)
        if (c->pix_fmt == PIX_FMT_NONE)
        	c->pix_fmt = *supported_pixel_formats;
        ++supported_pixel_formats;
    }

    // Codec doesn't have any pix formats?
    if (c->pix_fmt == PIX_FMT_NONE) {
        if(fmt->video_codec == AV_CODEC_ID_RAWVIDEO) {
            // Raw video should use RGB24
        	c->pix_fmt = PIX_FMT_RGB24;

#if (LIBAVFORMAT_VERSION_MAJOR < 58)
        if (strcmp(fmt->name, "gif") != 0)
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
#if (LIBAVFORMAT_VERSION_MAJOR < 58)
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::add_video_stream (" + (string)fmt->name + " : " + (string)av_get_pix_fmt_name(c->pix_fmt) + ")", "c->codec_id", c->codec_id, "c->bit_rate", c->bit_rate, "c->pix_fmt", c->pix_fmt, "oc->oformat->flags", oc->oformat->flags, "AVFMT_RAWPICTURE", AVFMT_RAWPICTURE, "", -1);
#else
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::add_video_stream (" + (string)fmt->name + " : " + (string)av_get_pix_fmt_name(c->pix_fmt) + ")", "c->codec_id", c->codec_id, "c->bit_rate", c->bit_rate, "c->pix_fmt", c->pix_fmt, "oc->oformat->flags", oc->oformat->flags, "", -1, "", -1);
#endif

	return st;
}

// open audio codec
void FFmpegWriter::open_audio(AVFormatContext *oc, AVStream *st)
{
	AVCodec *codec;
	AV_GET_CODEC_FROM_STREAM(st, audio_codec)

	// Set number of threads equal to number of processors (not to exceed 16)
	audio_codec->thread_count = min(FF_NUM_PROCESSORS, 16);

	// Find the audio encoder
	codec = avcodec_find_encoder_by_name(info.acodec.c_str());
	if (!codec)
		codec = avcodec_find_encoder(audio_codec->codec_id);
	if (!codec)
		throw InvalidCodec("Could not find codec", path);

	// Init options
	AVDictionary *opts = NULL;
	av_dict_set(&opts, "strict", "experimental", 0);

	// Open the codec
	if (avcodec_open2(audio_codec, codec, &opts) < 0)
		throw InvalidCodec("Could not open codec", path);
	AV_COPY_PARAMS_FROM_CONTEXT(st, audio_codec);

	// Free options
	av_dict_free(&opts);

	// Calculate the size of the input frame (i..e how many samples per packet), and the output buffer
	// TODO: Ugly hack for PCM codecs (will be removed ASAP with new PCM support to compute the input frame size in samples
	if (audio_codec->frame_size <= 1) {
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
		audio_input_frame_size = audio_codec->frame_size;
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
	for(std::map<string, string>::iterator iter = info.metadata.begin(); iter != info.metadata.end(); ++iter)
	{
		av_dict_set(&st->metadata, iter->first.c_str(), iter->second.c_str(), 0);
	}

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::open_audio", "audio_codec->thread_count", audio_codec->thread_count, "audio_input_frame_size", audio_input_frame_size, "buffer_size", AVCODEC_MAX_AUDIO_FRAME_SIZE + MY_INPUT_BUFFER_PADDING_SIZE, "", -1, "", -1, "", -1);

}

// open video codec
void FFmpegWriter::open_video(AVFormatContext *oc, AVStream *st)
{
	AVCodec *codec;
	AV_GET_CODEC_FROM_STREAM(st, video_codec)

	// Set number of threads equal to number of processors (not to exceed 16)
	video_codec->thread_count = min(FF_NUM_PROCESSORS, 16);

	/* find the video encoder */
	codec = avcodec_find_encoder_by_name(info.vcodec.c_str());
	if (!codec)
		codec = avcodec_find_encoder(AV_FIND_DECODER_CODEC_ID(st));
	if (!codec)
		throw InvalidCodec("Could not find codec", path);

    /* Force max_b_frames to 0 in some cases (i.e. for mjpeg image sequences */
    if(video_codec->max_b_frames && video_codec->codec_id != AV_CODEC_ID_MPEG4 && video_codec->codec_id != AV_CODEC_ID_MPEG1VIDEO && video_codec->codec_id != AV_CODEC_ID_MPEG2VIDEO)
        video_codec->max_b_frames = 0;

	// Init options
	AVDictionary *opts = NULL;
	av_dict_set(&opts, "strict", "experimental", 0);

	/* open the codec */
	if (avcodec_open2(video_codec, codec, &opts) < 0)
		throw InvalidCodec("Could not open codec", path);
	AV_COPY_PARAMS_FROM_CONTEXT(st, video_codec);

	// Free options
	av_dict_free(&opts);

	// Add video metadata (if any)
	for(std::map<string, string>::iterator iter = info.metadata.begin(); iter != info.metadata.end(); ++iter)
	{
		av_dict_set(&st->metadata, iter->first.c_str(), iter->second.c_str(), 0);
	}

	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::open_video", "video_codec->thread_count", video_codec->thread_count, "", -1, "", -1, "", -1, "", -1, "", -1);

}

// write all queued frames' audio to the video file
void FFmpegWriter::write_audio_packets(bool final)
{
	#pragma omp task firstprivate(final)
	{
		// Init audio buffers / variables
		int total_frame_samples = 0;
		int frame_position = 0;
		int channels_in_frame = 0;
		int sample_rate_in_frame = 0;
		int samples_in_frame = 0;
		ChannelLayout channel_layout_in_frame = LAYOUT_MONO; // default channel layout

		// Create a new array (to hold all S16 audio samples, for the current queued frames
		int16_t* all_queued_samples = (int16_t*)av_malloc((sizeof(int16_t)*(queued_audio_frames.size() * AVCODEC_MAX_AUDIO_FRAME_SIZE)));
		int16_t* all_resampled_samples = NULL;
		int16_t* final_samples_planar = NULL;
		int16_t* final_samples = NULL;

		// Loop through each queued audio frame
		while (!queued_audio_frames.empty())
		{
			// Get front frame (from the queue)
			std::shared_ptr<Frame> frame = queued_audio_frames.front();

			// Get the audio details from this frame
			sample_rate_in_frame = frame->SampleRate();
			samples_in_frame = frame->GetAudioSamplesCount();
			channels_in_frame = frame->GetAudioChannelsCount();
			channel_layout_in_frame = frame->ChannelsLayout();


			// Get audio sample array
			float* frame_samples_float = NULL;
			// Get samples interleaved together (c1 c2 c1 c2 c1 c2)
			frame_samples_float = frame->GetInterleavedAudioSamples(sample_rate_in_frame, NULL, &samples_in_frame);


			// Calculate total samples
			total_frame_samples = samples_in_frame * channels_in_frame;

			// Translate audio sample values back to 16 bit integers
			for (int s = 0; s < total_frame_samples; s++, frame_position++)
				// Translate sample value and copy into buffer
				all_queued_samples[frame_position] = int(frame_samples_float[s] * (1 << 15));


			// Deallocate float array
			delete[] frame_samples_float;

			// Remove front item
			queued_audio_frames.pop_front();

		} // end while


		// Update total samples (since we've combined all queued frames)
		total_frame_samples = frame_position;
		int remaining_frame_samples = total_frame_samples;
		int samples_position = 0;


		ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_audio_packets", "final", final, "total_frame_samples", total_frame_samples, "channel_layout_in_frame", channel_layout_in_frame, "channels_in_frame", channels_in_frame, "samples_in_frame", samples_in_frame, "LAYOUT_MONO", LAYOUT_MONO);

		// Keep track of the original sample format
		AVSampleFormat output_sample_fmt = audio_codec->sample_fmt;

		AVFrame *audio_frame = NULL;
		if (!final) {
			// Create input frame (and allocate arrays)
			audio_frame = AV_ALLOCATE_FRAME();
			AV_RESET_FRAME(audio_frame);
			audio_frame->nb_samples = total_frame_samples / channels_in_frame;

			// Fill input frame with sample data
			avcodec_fill_audio_frame(audio_frame, channels_in_frame, AV_SAMPLE_FMT_S16, (uint8_t *) all_queued_samples,
					audio_encoder_buffer_size, 0);

			// Do not convert audio to planar format (yet). We need to keep everything interleaved at this point.
			switch (audio_codec->sample_fmt)
			{
				case AV_SAMPLE_FMT_FLTP:
				{
					output_sample_fmt = AV_SAMPLE_FMT_FLT;
					break;
				}
				case AV_SAMPLE_FMT_S32P:
				{
					output_sample_fmt = AV_SAMPLE_FMT_S32;
					break;
				}
				case AV_SAMPLE_FMT_S16P:
				{
					output_sample_fmt = AV_SAMPLE_FMT_S16;
					break;
				}
				case AV_SAMPLE_FMT_U8P:
				{
					output_sample_fmt = AV_SAMPLE_FMT_U8;
					break;
				}
			}

			// Update total samples & input frame size (due to bigger or smaller data types)
			total_frame_samples *= (float(info.sample_rate) / sample_rate_in_frame); // adjust for different byte sizes
			total_frame_samples *= (float(info.channels) / channels_in_frame); // adjust for different # of channels

			// Set remaining samples
			remaining_frame_samples = total_frame_samples;

			// Create output frame (and allocate arrays)
			AVFrame *audio_converted = AV_ALLOCATE_FRAME();
			AV_RESET_FRAME(audio_converted);
			audio_converted->nb_samples = total_frame_samples / channels_in_frame;
			av_samples_alloc(audio_converted->data, audio_converted->linesize, info.channels, audio_converted->nb_samples, output_sample_fmt, 0);

			ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_audio_packets (1st resampling)", "in_sample_fmt", AV_SAMPLE_FMT_S16, "out_sample_fmt", output_sample_fmt, "in_sample_rate", sample_rate_in_frame, "out_sample_rate", info.sample_rate, "in_channels", channels_in_frame, "out_channels", info.channels);

			// setup resample context
			if (!avr) {
				avr = SWR_ALLOC();
				av_opt_set_int(avr,  "in_channel_layout", channel_layout_in_frame, 0);
				av_opt_set_int(avr, "out_channel_layout", info.channel_layout, 0);
				av_opt_set_int(avr,  "in_sample_fmt",     AV_SAMPLE_FMT_S16,     0);
				av_opt_set_int(avr, "out_sample_fmt",     output_sample_fmt,     0); // planar not allowed here
				av_opt_set_int(avr,  "in_sample_rate",    sample_rate_in_frame,    0);
				av_opt_set_int(avr, "out_sample_rate",    info.sample_rate,    0);
				av_opt_set_int(avr,  "in_channels",       channels_in_frame,    0);
				av_opt_set_int(avr, "out_channels",       info.channels,    0);
				SWR_INIT(avr);
			}
			int nb_samples = 0;

			// Convert audio samples
			nb_samples = SWR_CONVERT(avr, 	// audio resample context
					audio_converted->data, 			// output data pointers
					audio_converted->linesize[0], 	// output plane size, in bytes. (0 if unknown)
					audio_converted->nb_samples,	// maximum number of samples that the output buffer can hold
					audio_frame->data,				// input data pointers
					audio_frame->linesize[0],		// input plane size, in bytes (0 if unknown)
					audio_frame->nb_samples);		// number of input samples to convert

			// Create a new array (to hold all resampled S16 audio samples)
			all_resampled_samples = (int16_t*)av_malloc(sizeof(int16_t) * nb_samples * info.channels * (av_get_bytes_per_sample(output_sample_fmt)  / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)));

			// Copy audio samples over original samples
			memcpy(all_resampled_samples, audio_converted->data[0], nb_samples * info.channels * av_get_bytes_per_sample(output_sample_fmt));

			// Remove converted audio
			av_freep(&(audio_frame->data[0]));
            AV_FREE_FRAME(&audio_frame);
			av_freep(&audio_converted->data[0]);
            AV_FREE_FRAME(&audio_converted);
			all_queued_samples = NULL; // this array cleared with above call

			ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_audio_packets (Successfully completed 1st resampling)", "nb_samples", nb_samples, "remaining_frame_samples", remaining_frame_samples, "", -1, "", -1, "", -1, "", -1);
		}

		// Loop until no more samples
		while (remaining_frame_samples > 0 || final) {
			// Get remaining samples needed for this packet
			int remaining_packet_samples = (audio_input_frame_size * info.channels) - audio_input_position;

			// Determine how many samples we need
			int diff = 0;
			if (remaining_frame_samples >= remaining_packet_samples)
				diff = remaining_packet_samples;
			else if (remaining_frame_samples < remaining_packet_samples)
				diff = remaining_frame_samples;

			// Copy frame samples into the packet samples array
			if (!final)
				//TODO: Make this more sane
				memcpy(samples + (audio_input_position * (av_get_bytes_per_sample(output_sample_fmt) / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16))), all_resampled_samples + samples_position, diff * av_get_bytes_per_sample(output_sample_fmt));

			// Increment counters
			audio_input_position += diff;
			samples_position += diff * (av_get_bytes_per_sample(output_sample_fmt) / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16));
			remaining_frame_samples -= diff;
			remaining_packet_samples -= diff;

			// Do we have enough samples to proceed?
			if (audio_input_position < (audio_input_frame_size * info.channels) && !final)
				// Not enough samples to encode... so wait until the next frame
				break;

			// Convert to planar (if needed by audio codec)
			AVFrame *frame_final = AV_ALLOCATE_FRAME();
			AV_RESET_FRAME(frame_final);
			if (av_sample_fmt_is_planar(audio_codec->sample_fmt))
			{
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_audio_packets (2nd resampling for Planar formats)", "in_sample_fmt", output_sample_fmt, "out_sample_fmt", audio_codec->sample_fmt, "in_sample_rate", info.sample_rate, "out_sample_rate", info.sample_rate, "in_channels", info.channels, "out_channels", info.channels);

				// setup resample context
				if (!avr_planar) {
					avr_planar = SWR_ALLOC();
					av_opt_set_int(avr_planar,  "in_channel_layout", info.channel_layout, 0);
					av_opt_set_int(avr_planar, "out_channel_layout", info.channel_layout, 0);
					av_opt_set_int(avr_planar,  "in_sample_fmt",     output_sample_fmt,     0);
					av_opt_set_int(avr_planar, "out_sample_fmt",     audio_codec->sample_fmt,     0); // planar not allowed here
					av_opt_set_int(avr_planar,  "in_sample_rate",    info.sample_rate,    0);
					av_opt_set_int(avr_planar, "out_sample_rate",    info.sample_rate,    0);
					av_opt_set_int(avr_planar,  "in_channels",       info.channels,    0);
					av_opt_set_int(avr_planar, "out_channels",       info.channels,    0);
					SWR_INIT(avr_planar);
				}

				// Create input frame (and allocate arrays)
				audio_frame = AV_ALLOCATE_FRAME();
				AV_RESET_FRAME(audio_frame);
				audio_frame->nb_samples = audio_input_position / info.channels;

				// Create a new array
				final_samples_planar = (int16_t*)av_malloc(sizeof(int16_t) * audio_frame->nb_samples * info.channels * (av_get_bytes_per_sample(output_sample_fmt) / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)));

				// Copy audio into buffer for frame
				memcpy(final_samples_planar, samples, audio_frame->nb_samples * info.channels * av_get_bytes_per_sample(output_sample_fmt));

				// Fill input frame with sample data
				avcodec_fill_audio_frame(audio_frame, info.channels, output_sample_fmt, (uint8_t *) final_samples_planar,
						audio_encoder_buffer_size, 0);

				// Create output frame (and allocate arrays)
				frame_final->nb_samples = audio_input_frame_size;
				av_samples_alloc(frame_final->data, frame_final->linesize, info.channels, frame_final->nb_samples, audio_codec->sample_fmt, 0);

				// Convert audio samples
				int nb_samples = SWR_CONVERT(avr_planar, 	// audio resample context
						frame_final->data, 			// output data pointers
						frame_final->linesize[0], 	// output plane size, in bytes. (0 if unknown)
						frame_final->nb_samples,	// maximum number of samples that the output buffer can hold
						audio_frame->data,				// input data pointers
						audio_frame->linesize[0],		// input plane size, in bytes (0 if unknown)
						audio_frame->nb_samples);		// number of input samples to convert

				// Copy audio samples over original samples
				if (nb_samples > 0)
					memcpy(samples, frame_final->data[0], nb_samples * av_get_bytes_per_sample(audio_codec->sample_fmt) * info.channels);

				// deallocate AVFrame
 				av_freep(&(audio_frame->data[0]));
 				AV_FREE_FRAME(&audio_frame);
 				all_queued_samples = NULL; // this array cleared with above call

				ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_audio_packets (Successfully completed 2nd resampling for Planar formats)", "nb_samples", nb_samples, "", -1, "", -1, "", -1, "", -1, "", -1);

			} else {
				// Create a new array
				final_samples = (int16_t*)av_malloc(sizeof(int16_t) * audio_input_position * (av_get_bytes_per_sample(audio_codec->sample_fmt) / av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)));

				// Copy audio into buffer for frame
				memcpy(final_samples, samples, audio_input_position * av_get_bytes_per_sample(audio_codec->sample_fmt));

				// Init the nb_samples property
				frame_final->nb_samples = audio_input_frame_size;

				// Fill the final_frame AVFrame with audio (non planar)
				avcodec_fill_audio_frame(frame_final, audio_codec->channels, audio_codec->sample_fmt, (uint8_t *) final_samples,
						audio_encoder_buffer_size, 0);
			}

			// Increment PTS (in samples)
			write_audio_count += FFMIN(audio_input_frame_size, audio_input_position);
			frame_final->pts = write_audio_count; // Set the AVFrame's PTS

			// Init the packet
			AVPacket pkt;
			av_init_packet(&pkt);
			pkt.data = audio_encoder_buffer;
			pkt.size = audio_encoder_buffer_size;

			// Set the packet's PTS prior to encoding
			pkt.pts = pkt.dts = write_audio_count;

			/* encode the audio samples */
			int got_packet_ptr = 0;

			#if IS_FFMPEG_3_2
			// Encode audio (latest version of FFmpeg)
			int error_code;
			int ret = 0;
			int frame_finished = 0;
			error_code = ret =  avcodec_send_frame(audio_codec, frame_final);
			if (ret < 0 && ret !=  AVERROR(EINVAL) && ret != AVERROR_EOF) {
				avcodec_send_frame(audio_codec, NULL);
			}
			else {
				if (ret >= 0)
					pkt.size = 0;
				ret =  avcodec_receive_packet(audio_codec, &pkt);
				if (ret >= 0)
					frame_finished = 1;
				if(ret == AVERROR(EINVAL) || ret == AVERROR_EOF) {
					avcodec_flush_buffers(audio_codec);
					ret = 0;
				}
				if (ret >= 0) {
					ret = frame_finished;
				}
			}
			if (!pkt.data && !frame_finished)
			{
				ret = -1;
			}
			got_packet_ptr = ret;
			#else
			// Encode audio (older versions of FFmpeg)
			int error_code = avcodec_encode_audio2(audio_codec, &pkt, frame_final, &got_packet_ptr);
			#endif
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
				if (error_code < 0)
				{
					ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_audio_packets ERROR [" + (string)av_err2str(error_code) + "]", "error_code", error_code, "", -1, "", -1, "", -1, "", -1, "", -1);
				}
			}

			if (error_code < 0)
			{
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_audio_packets ERROR [" + (string)av_err2str(error_code) + "]", "error_code", error_code, "", -1, "", -1, "", -1, "", -1, "", -1);
			}

			// deallocate AVFrame
			av_freep(&(frame_final->data[0]));
            AV_FREE_FRAME(&frame_final);

			// deallocate memory for packet
			AV_FREE_PACKET(&pkt);

			// Reset position
			audio_input_position = 0;
			final = false;
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

	} // end task
}

// Allocate an AVFrame object
AVFrame* FFmpegWriter::allocate_avframe(PixelFormat pix_fmt, int width, int height, int *buffer_size, uint8_t *new_buffer)
{
	// Create an RGB AVFrame
	AVFrame *new_av_frame = NULL;

	// Allocate an AVFrame structure
	new_av_frame = AV_ALLOCATE_FRAME();
	if (new_av_frame == NULL)
		throw OutOfMemory("Could not allocate AVFrame", path);

	// Determine required buffer size and allocate buffer
	*buffer_size = AV_GET_IMAGE_SIZE(pix_fmt, width, height);

	// Create buffer (if not provided)
	if (!new_buffer)
	{
		// New Buffer
		new_buffer = (uint8_t*)av_malloc(*buffer_size * sizeof(uint8_t));
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
void FFmpegWriter::process_video_packet(std::shared_ptr<Frame> frame)
{
	// Determine the height & width of the source image
	int source_image_width = frame->GetWidth();
	int source_image_height = frame->GetHeight();

	// Do nothing if size is 1x1 (i.e. no image in this frame)
	if (source_image_height == 1 && source_image_width == 1)
		return;

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
		const uchar *pixels = NULL;

		// Get a list of pixels from source image
		pixels = frame->GetPixels();

		// Init AVFrame for source image & final (converted image)
		frame_source = allocate_avframe(PIX_FMT_RGBA, source_image_width, source_image_height, &bytes_source, (uint8_t*) pixels);
		#if IS_FFMPEG_3_2
		AVFrame *frame_final = allocate_avframe((AVPixelFormat)(video_st->codecpar->format), info.width, info.height, &bytes_final, NULL);
		#else
		AVFrame *frame_final = allocate_avframe(video_codec->pix_fmt, info.width, info.height, &bytes_final, NULL);
		#endif

		// Fill with data
        AV_COPY_PICTURE_DATA(frame_source, (uint8_t*)pixels, PIX_FMT_RGBA, source_image_width, source_image_height);
		ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::process_video_packet", "frame->number", frame->number, "bytes_source", bytes_source, "bytes_final", bytes_final, "", -1, "", -1, "", -1);

		// Resize & convert pixel format
		sws_scale(scaler, frame_source->data, frame_source->linesize, 0,
				source_image_height, frame_final->data, frame_final->linesize);

		// Add resized AVFrame to av_frames map
		#pragma omp critical (av_frames_section)
		add_avframe(frame, frame_final);

		// Deallocate memory
		AV_FREE_FRAME(&frame_source);

	} // end task

}

// write video frame
bool FFmpegWriter::write_video_packet(std::shared_ptr<Frame> frame, AVFrame* frame_final)
{
#if (LIBAVFORMAT_VERSION_MAJOR >= 58)
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_video_packet", "frame->number", frame->number, "oc->oformat->flags", oc->oformat->flags, "", -1, "", -1, "", -1, "", -1);
#else
	ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_video_packet", "frame->number", frame->number, "oc->oformat->flags & AVFMT_RAWPICTURE", oc->oformat->flags & AVFMT_RAWPICTURE, "", -1, "", -1, "", -1, "", -1);

	if (oc->oformat->flags & AVFMT_RAWPICTURE) {
		// Raw video case.
		AVPacket pkt;
		av_init_packet(&pkt);

		pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index= video_st->index;
		pkt.data= (uint8_t*)frame_final->data;
		pkt.size= sizeof(AVPicture);

		// Increment PTS (in frames and scaled to the codec's timebase)
		write_video_count += av_rescale_q(1, (AVRational){info.fps.den, info.fps.num}, video_codec->time_base);
		pkt.pts = write_video_count;

		/* write the compressed frame in the media file */
		int error_code = av_interleaved_write_frame(oc, &pkt);
		if (error_code < 0)
		{
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_video_packet ERROR [" + (string)av_err2str(error_code) + "]", "error_code", error_code, "", -1, "", -1, "", -1, "", -1, "", -1);
			return false;
		}

		// Deallocate packet
		AV_FREE_PACKET(&pkt);

	} else
#endif
        {

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
		#if IS_FFMPEG_3_2
		// Write video packet (latest version of FFmpeg)
		int frameFinished = 0;
		int ret = avcodec_send_frame(video_codec, frame_final);
		error_code = ret;
		if (ret < 0 ) {
			ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_video_packet (Frame not sent)", "", -1, "", -1, "", -1, "", -1, "", -1, "", -1);
			if (ret == AVERROR(EAGAIN) )
				cerr << "Frame EAGAIN" << "\n";
			if (ret == AVERROR_EOF )
				cerr << "Frame AVERROR_EOF" << "\n";
			avcodec_send_frame(video_codec, NULL);
		}
		else {
			while (ret >= 0) {
				ret = avcodec_receive_packet(video_codec, &pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					avcodec_flush_buffers(video_codec);
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
			#if LIBAVFORMAT_VERSION_MAJOR >= 54
				// Write video packet (older than FFmpeg 3.2)
				error_code = avcodec_encode_video2(video_codec, &pkt, frame_final, &got_packet_ptr);
				if (error_code != 0 )
					cerr << "Frame AVERROR_EOF" << "\n";
				if (got_packet_ptr == 0 )
					cerr << "Frame gotpacket error" << "\n";
			#else
				// Write video packet (even older versions of FFmpeg)
				int video_outbuf_size = 200000;
				video_outbuf = (uint8_t*) av_malloc(200000);

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
			if (error_code < 0)
			{
				ZmqLogger::Instance()->AppendDebugMethod("FFmpegWriter::write_video_packet ERROR [" + (string)av_err2str(error_code) + "]", "error_code", error_code, "", -1, "", -1, "", -1, "", -1, "", -1);
				return false;
			}
		}

		// Deallocate memory (if needed)
		if (video_outbuf)
			delete[] video_outbuf;

		// Deallocate packet
		AV_FREE_PACKET(&pkt);
	}

	// Success
	return true;
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
	int scale_mode = SWS_FAST_BILINEAR;
	if (openshot::Settings::Instance()->HIGH_QUALITY_SCALING) {
		scale_mode = SWS_LANCZOS;
	}

	// Init software rescalers vector (many of them, one for each thread)
	for (int x = 0; x < num_of_rescalers; x++)
	{
		// Init the software scaler from FFMpeg
		img_convert_ctx = sws_getContext(source_width, source_height, PIX_FMT_RGBA, info.width, info.height, AV_GET_CODEC_PIXEL_FORMAT(video_st, video_st->codec), scale_mode, NULL, NULL, NULL);

		// Add rescaler to vector
		image_rescalers.push_back(img_convert_ctx);
	}
}

// Set audio resample options
void FFmpegWriter::ResampleAudio(int sample_rate, int channels) {
	original_sample_rate = sample_rate;
	original_channels = channels;
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
