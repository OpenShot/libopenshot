/**
 * @file
 * @brief Source file for FFmpegReader class
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

#include "../include/FFmpegReader.h"

using namespace openshot;

FFmpegReader::FFmpegReader(string path) throw(InvalidFile, NoStreamsFound, InvalidCodec)
	: last_frame(0), is_seeking(0), seeking_pts(0), seeking_frame(0), seek_count(0),
	  audio_pts_offset(99999), video_pts_offset(99999), path(path), is_video_seek(true), check_interlace(false),
	  check_fps(false), enable_seek(true), rescaler_position(0), num_of_rescalers(OPEN_MP_NUM_PROCESSORS), is_open(false),
	  seek_audio_frame_found(0), seek_video_frame_found(0), prev_samples(0), prev_pts(0),
	  pts_total(0), pts_counter(0), is_duration_known(false), largest_frame_processed(0) {

	// Initialize FFMpeg, and register all formats and codecs
	av_register_all();
	avcodec_register_all();

	// Init cache
	working_cache.SetMaxBytes(0);
	final_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 4, info.width, info.height, info.sample_rate, info.channels);

	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

// Init a collection of software rescalers (thread safe)
void FFmpegReader::InitScalers()
{
	// Init software rescalers vector (many of them, one for each thread)
	for (int x = 0; x < num_of_rescalers; x++)
	{
		SwsContext *img_convert_ctx = sws_getContext(info.width, info.height, pCodecCtx->pix_fmt, info.width,
				info.height, PIX_FMT_RGBA, SWS_FAST_BILINEAR, NULL, NULL, NULL);

		// Add rescaler to vector
		image_rescalers.push_back(img_convert_ctx);
	}
}

// This struct holds the associated video frame and starting sample # for an audio packet.
int AudioLocation::is_near(AudioLocation location, int samples_per_frame, int amount)
{
	// Is frame even close to this one?
	if (abs(location.frame - frame) >= 2)
		// This is too far away to be considered
		return false;

	int sample_diff = abs(location.sample_start - sample_start);
	if (location.frame == frame && sample_diff >= 0 && sample_diff <= amount)
		// close
		return true;

	// new frame is after
	if (location.frame > frame)
	{
		// remaining samples + new samples
		int sample_diff = (samples_per_frame - sample_start) + location.sample_start;
		if (sample_diff >= 0 && sample_diff <= amount)
			return true;
	}

	// new frame is before
	if (location.frame < frame)
	{
		// remaining new samples + old samples
		int sample_diff = (samples_per_frame - location.sample_start) + sample_start;
		if (sample_diff >= 0 && sample_diff <= amount)
			return true;
	}

	// not close
	return false;
}

// Remove & deallocate all software scalers
void FFmpegReader::RemoveScalers()
{
	// Close all rescalers
	for (int x = 0; x < num_of_rescalers; x++)
		sws_freeContext(image_rescalers[x]);

	// Clear vector
	image_rescalers.clear();
}

void FFmpegReader::Open() throw(InvalidFile, NoStreamsFound, InvalidCodec)
{
	// Open reader if not already open
	if (!is_open)
	{
		// Initialize format context
		pFormatCtx = NULL;

		// Open video file
		if (avformat_open_input(&pFormatCtx, path.c_str(), NULL, NULL) != 0)
			throw InvalidFile("File could not be opened.", path);

		// Retrieve stream information
		if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
			throw NoStreamsFound("No streams found in file.", path);

		videoStream = -1;
		audioStream = -1;
		// Loop through each stream, and identify the video and audio stream index
		for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
		{
			// Is this a video stream?
			if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && videoStream < 0) {
				videoStream = i;
			}
			// Is this an audio stream?
			if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO && audioStream < 0) {
				audioStream = i;
			}
		}
		if (videoStream == -1 && audioStream == -1)
			throw NoStreamsFound("No video or audio streams found in this file.", path);

		// Is there a video stream?
		if (videoStream != -1)
		{
			// Set the stream index
			info.video_stream_index = videoStream;

			// Set the codec and codec context pointers
			pStream = pFormatCtx->streams[videoStream];
			pCodecCtx = pFormatCtx->streams[videoStream]->codec;

			// Set number of threads equal to number of processors + 1
			pCodecCtx->thread_count = OPEN_MP_NUM_PROCESSORS;

			// Find the decoder for the video stream
			AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
			if (pCodec == NULL) {
				throw InvalidCodec("A valid video codec could not be found for this file.", path);
			}
			// Open video codec
			if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
				throw InvalidCodec("A video codec was found, but could not be opened.", path);

			// Update the File Info struct with video details (if a video stream is found)
			UpdateVideoInfo();

			// Init rescalers (if video stream detected)
			InitScalers();
		}

		// Is there an audio stream?
		if (audioStream != -1)
		{
			// Set the stream index
			info.audio_stream_index = audioStream;

			// Get a pointer to the codec context for the audio stream
			aStream = pFormatCtx->streams[audioStream];
			aCodecCtx = pFormatCtx->streams[audioStream]->codec;

			// Set number of threads equal to number of processors + 1
			aCodecCtx->thread_count = OPEN_MP_NUM_PROCESSORS;

			// Find the decoder for the audio stream
			AVCodec *aCodec = avcodec_find_decoder(aCodecCtx->codec_id);
			if (aCodec == NULL) {
				throw InvalidCodec("A valid audio codec could not be found for this file.", path);
			}
			// Open audio codec
			if (avcodec_open2(aCodecCtx, aCodec, NULL) < 0)
				throw InvalidCodec("An audio codec was found, but could not be opened.", path);

			// Update the File Info struct with audio details (if an audio stream is found)
			UpdateAudioInfo();
		}

		// Init previous audio location to zero
		previous_packet_location.frame = -1;
		previous_packet_location.sample_start = 0;

		// Adjust cache size based on size of frame and audio
		final_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 4, info.width, info.height, info.sample_rate, info.channels);

		// Mark as "open"
		is_open = true;
	}
}

void FFmpegReader::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Close the codec
		if (info.has_video)
		{
			// Clear image scalers
			RemoveScalers();

			avcodec_flush_buffers(pCodecCtx);
			avcodec_close(pCodecCtx);
		}
		if (info.has_audio)
		{
			avcodec_flush_buffers(aCodecCtx);
			avcodec_close(aCodecCtx);
		}

		// Clear final cache
		final_cache.Clear();
		working_cache.Clear();

		// Clear processed lists
		processed_video_frames.clear();
		processed_audio_frames.clear();
		processing_video_frames.clear();
		processing_audio_frames.clear();

		// Clear debug json
		debug_root.clear();

		// Close the video file
		avformat_close_input(&pFormatCtx);
		av_freep(&pFormatCtx);

		// Mark as "closed"
		is_open = false;

		// Reset some variables
		last_frame = 0;
		largest_frame_processed = 0;
		seek_audio_frame_found = 0;
		seek_video_frame_found = 0;
	}
}

void FFmpegReader::UpdateAudioInfo()
{
	// Set values of FileInfo struct
	info.has_audio = true;
	info.file_size = pFormatCtx->pb ? avio_size(pFormatCtx->pb) : -1;
	info.acodec = aCodecCtx->codec->name;
	info.channels = aCodecCtx->channels;
	if (aCodecCtx->channel_layout == 0)
		aCodecCtx->channel_layout = av_get_default_channel_layout( aCodecCtx->channels );;
	info.channel_layout = (ChannelLayout) aCodecCtx->channel_layout;
	info.sample_rate = aCodecCtx->sample_rate;
	info.audio_bit_rate = aCodecCtx->bit_rate;

	// Set audio timebase
	info.audio_timebase.num = aStream->time_base.num;
	info.audio_timebase.den = aStream->time_base.den;

	// Get timebase of audio stream (if valid)
	if (aStream->duration > 0.0f)
		info.duration = aStream->duration * info.audio_timebase.ToDouble();

	// Check for an invalid video length
	if (info.has_video && info.video_length <= 0)
	{
		// Calculate the video length from the audio duration
		info.video_length = info.duration * info.fps.ToDouble();
	}

	// Set video timebase (if no video stream was found)
	if (!info.has_video)
	{
		// Set a few important default video settings (so audio can be divided into frames)
		info.fps.num = 24;
		info.fps.den = 1;
		info.video_timebase.num = 1;
		info.video_timebase.den = 24;
		info.video_length = info.duration * info.fps.ToDouble();

	}

}

void FFmpegReader::UpdateVideoInfo()
{
	// Set values of FileInfo struct
	info.has_video = true;
	info.file_size = pFormatCtx->pb ? avio_size(pFormatCtx->pb) : -1;
	info.height = pCodecCtx->height;
	info.width = pCodecCtx->width;
	info.vcodec = pCodecCtx->codec->name;
	info.video_bit_rate = pFormatCtx->bit_rate;
	if (!check_fps)
	{
		// set frames per second (fps)
		info.fps.num = pStream->avg_frame_rate.num;
		info.fps.den = pStream->avg_frame_rate.den;
	}
	if (pStream->sample_aspect_ratio.num != 0)
	{
		info.pixel_ratio.num = pStream->sample_aspect_ratio.num;
		info.pixel_ratio.den = pStream->sample_aspect_ratio.den;
	}
	else if (pCodecCtx->sample_aspect_ratio.num != 0)
	{
		info.pixel_ratio.num = pCodecCtx->sample_aspect_ratio.num;
		info.pixel_ratio.den = pCodecCtx->sample_aspect_ratio.den;
	}
	else
	{
		info.pixel_ratio.num = 1;
		info.pixel_ratio.den = 1;
	}

	info.pixel_format = pCodecCtx->pix_fmt;

	// Calculate the DAR (display aspect ratio)
	Fraction size(info.width * info.pixel_ratio.num, info.height * info.pixel_ratio.den);

	// Reduce size fraction
	size.Reduce();

	// Set the ratio based on the reduced fraction
	info.display_ratio.num = size.num;
	info.display_ratio.den = size.den;

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
	if (info.duration <= 0.0f)
	{
		// No duration is found in the video stream
		info.duration = -1;
		info.video_length = -1;
		is_duration_known = false;
	}
	else
	{
		// Yes, a duration was found
		is_duration_known = true;

		// Calculate number of frames
		info.video_length = round(info.duration * info.fps.ToDouble());
	}

	// Override an invalid framerate
	if (info.fps.ToFloat() > 120.0f)
	{
		// Set a few important default video settings (so audio can be divided into frames)
		info.fps.num = 24;
		info.fps.den = 1;
		info.video_timebase.num = 1;
		info.video_timebase.den = 24;
	}

}


tr1::shared_ptr<Frame> FFmpegReader::GetFrame(int requested_frame) throw(OutOfBoundsFrame, ReaderClosed, TooManySeeks)
{
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
	AppendDebugMethod("FFmpegReader::GetFrame", "requested_frame", requested_frame, "last_frame", last_frame, "", -1, "", -1, "", -1, "", -1);

	// Check the cache for this frame
	if (final_cache.Exists(requested_frame)) {
		// Debug output
		AppendDebugMethod("FFmpegReader::GetFrame", "returned cached frame", requested_frame, "", -1, "", -1, "", -1, "", -1, "", -1);

		// Return the cached frame
		return final_cache.GetFrame(requested_frame);
	}
	else
	{
		// Create a scoped lock, allowing only a single thread to run the following code at one time
		const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

		// Check the cache a 2nd time (due to a potential previous lock)
		if (final_cache.Exists(requested_frame)) {
			// Debug output
			AppendDebugMethod("FFmpegReader::GetFrame", "returned cached frame on 2nd look", requested_frame, "", -1, "", -1, "", -1, "", -1, "", -1);

			// Return the cached frame
			return final_cache.GetFrame(requested_frame);
		}

		// Frame is not in cache
		// Reset seek count
		seek_count = 0;

		// Check for first frame (always need to get frame 1 before other frames, to correctly calculate offsets)
		if (last_frame == 0 && requested_frame != 1)
			// Get first frame
			ReadStream(1);

		// Are we within X frames of the requested frame?
		int diff = requested_frame - last_frame;
		if (diff >= 1 && diff <= 20)
		{
			// Continue walking the stream
			return ReadStream(requested_frame);
		}
		else
		{
			// Greater than 30 frames away, or backwards, we need to seek to the nearest key frame
			if (enable_seek)
				// Only seek if enabled
				Seek(requested_frame);

			else if (!enable_seek && diff < 0)
			{
				// Start over, since we can't seek, and the requested frame is smaller than our position
				Close();
				Open();
			}

			// Then continue walking the stream
			return ReadStream(requested_frame);
		}

	}
}

// Read the stream until we find the requested Frame
tr1::shared_ptr<Frame> FFmpegReader::ReadStream(int requested_frame)
{
	// Allocate video frame
	bool end_of_stream = false;
	bool check_seek = false;
	bool frame_finished = false;
	int packet_error = -1;

	// Minimum number of packets to process (for performance reasons)
	int packets_processed = 0;
	int minimum_packets = OPEN_MP_NUM_PROCESSORS;

	// Set the number of threads in OpenMP
	omp_set_num_threads(OPEN_MP_NUM_PROCESSORS);
	// Allow nested OpenMP sections
	omp_set_nested(true);

	// Debug output
	AppendDebugMethod("FFmpegReader::ReadStream", "requested_frame", requested_frame, "OPEN_MP_NUM_PROCESSORS", OPEN_MP_NUM_PROCESSORS, "", -1, "", -1, "", -1, "", -1);

	#pragma omp parallel
	{
		#pragma omp single
		{
			// Loop through the stream until the correct frame is found
			while (true)
			{
				// Get the next packet into a local variable called packet
				packet_error = GetNextPacket();

				// Wait if too many frames are being processed
				while (processing_video_frames.size() + processing_audio_frames.size() >= minimum_packets)
					usleep(50);

				// Get the next packet (if any)
				if (packet_error < 0)
				{
					// Break loop when no more packets found
					end_of_stream = true;
					break;
				}

				// Debug output
				AppendDebugMethod("FFmpegReader::ReadStream (GetNextPacket)", "requested_frame", requested_frame, "processing_video_frames.size()", processing_video_frames.size(), "processing_audio_frames.size()", processing_audio_frames.size(), "minimum_packets", minimum_packets, "packets_processed", packets_processed, "", -1);

				// Video packet
				if (packet->stream_index == videoStream)
				{
					// Check the status of a seek (if any)
					if (is_seeking)
						#pragma omp critical (openshot_seek)
						check_seek = CheckSeek(true);
					else
						check_seek = false;

					if (check_seek) {
						// Remove packet (since this packet is pointless)
						RemoveAVPacket(packet);

						// Jump to the next iteration of this loop
						continue;
					}

					// Get the AVFrame from the current packet
					frame_finished = GetAVFrame();

					// Check if the AVFrame is finished and set it
					if (frame_finished)
					{
						// Update PTS / Frame Offset (if any)
						UpdatePTSOffset(true);

						// Process Video Packet
						ProcessVideoPacket(requested_frame);
					}

				}
				// Audio packet
				else if (packet->stream_index == audioStream)
				{
					// Check the status of a seek (if any)
					if (is_seeking)
						#pragma omp critical (openshot_seek)
						check_seek = CheckSeek(false);
					else
						check_seek = false;

					if (check_seek) {
						// Remove packet (since this packet is pointless)
						RemoveAVPacket(packet);

						// Jump to the next iteration of this loop
						continue;
					}

					// Update PTS / Frame Offset (if any)
					UpdatePTSOffset(false);

					// Determine related video frame and starting sample # from audio PTS
					AudioLocation location = GetAudioPTSLocation(packet->pts);

					// Process Audio Packet
					ProcessAudioPacket(requested_frame, location.frame, location.sample_start);
				}

				// Check if working frames are 'finished'
				bool is_cache_found = false;
				if (!is_seeking)
					CheckWorkingFrames(false);

				// Check if requested 'final' frame is available
				is_cache_found = final_cache.Exists(requested_frame);

				// Increment frames processed
				packets_processed++;

				// Break once the frame is found
				if (is_cache_found && packets_processed >= minimum_packets)
					break;

			} // end while

		} // end omp single
	} // end omp parallel

	// Debug output
	AppendDebugMethod("FFmpegReader::ReadStream (Completed)", "packets_processed", packets_processed, "end_of_stream", end_of_stream, "largest_frame_processed", largest_frame_processed, "Working Cache Count", working_cache.Count(), "", -1, "", -1);

	// End of stream?
	if (end_of_stream) {
		// Mark the any other working frames as 'finished'
		CheckWorkingFrames(end_of_stream);

		// Update readers video length (to a largest processed frame number)
		info.video_length = largest_frame_processed; // just a guess, but this frame is certainly out of bounds
		is_duration_known = largest_frame_processed;
	}

	// Return requested frame (if found)
	if (final_cache.Exists(requested_frame))
		// Return prepared frame
		return final_cache.GetFrame(requested_frame);
	else {

		// Check if largest frame is still cached
		if (final_cache.Exists(largest_frame_processed)) {
			// return the largest processed frame (assuming it was the last in the video file)
			return final_cache.GetFrame(largest_frame_processed);
		}
		else {
			// The largest processed frame is no longer in cache, return a blank frame
			tr1::shared_ptr<Frame> f = CreateFrame(largest_frame_processed);
			f->AddColor(info.width, info.height, "#000");
			return f;
		}
	}

}

// Get the next packet (if any)
int FFmpegReader::GetNextPacket()
{
	int found_packet = 0;
	AVPacket *next_packet = new AVPacket();
	found_packet = av_read_frame(pFormatCtx, next_packet);

	if (found_packet >= 0)
	{
		#pragma omp critical (packet_cache)
		{
			// Add packet to packet cache
			packets[next_packet] = next_packet;

			// Update current packet pointer
			packet = packets[next_packet];
		} // end omp critical

	}
	else
	{
		// Free packet, since it's unused
		av_free_packet(next_packet);
		delete next_packet;
	}

	// Return if packet was found (or error number)
	return found_packet;
}

// Get an AVFrame (if any)
bool FFmpegReader::GetAVFrame()
{
	int frameFinished = -1;

	// Decode video frame
	AVFrame *next_frame = avcodec_alloc_frame();
	#pragma omp critical (packet_cache)
	avcodec_decode_video2(pCodecCtx, next_frame, &frameFinished, packet);

	// is frame finished
	if (frameFinished)
	{
		// AVFrames are clobbered on the each call to avcodec_decode_video, so we
		// must make a copy of the image data before this method is called again.
		AVPicture *copyFrame = new AVPicture();
		avpicture_alloc(copyFrame, pCodecCtx->pix_fmt, info.width, info.height);
		av_picture_copy(copyFrame, (AVPicture *) next_frame, pCodecCtx->pix_fmt, info.width, info.height);

		#pragma omp critical (packet_cache)
		{
			// add to AVFrame cache (if frame finished)
			frames[copyFrame] = copyFrame;
			pFrame = frames[copyFrame];
		}

		// Detect interlaced frame (only once)
		if (!check_interlace)
		{
			check_interlace = true;
			info.interlaced_frame = next_frame->interlaced_frame;
			info.top_field_first = next_frame->top_field_first;
		}

	}
	else
	{
		// Remove packet (since this packet is pointless)
		RemoveAVPacket(packet);
	}

	// deallocate the frame
	avcodec_free_frame(&next_frame);

	// Did we get a video frame?
	return frameFinished;
}

// Check the current seek position and determine if we need to seek again
bool FFmpegReader::CheckSeek(bool is_video)
{
	// Are we seeking for a specific frame?
	if (is_seeking)
	{
		// Determine if both an audio and video packet have been decoded since the seek happened.
		// If not, allow the ReadStream method to keep looping
		if ((is_video_seek && !seek_video_frame_found) || (!is_video_seek && !seek_audio_frame_found))
			return false;

		// Determine max seeked frame
		int max_seeked_frame = seek_audio_frame_found; // determine max seeked frame
		if (seek_video_frame_found > max_seeked_frame)
			max_seeked_frame = seek_video_frame_found;

		// determine if we are "before" the requested frame
		if (max_seeked_frame >= seeking_frame)
		{
			// SEEKED TOO FAR
			AppendDebugMethod("FFmpegReader::CheckSeek (Too far, seek again)", "is_video_seek", is_video_seek, "max_seeked_frame", max_seeked_frame, "seeking_frame", seeking_frame, "seeking_pts", seeking_pts, "seek_video_frame_found", seek_video_frame_found, "seek_audio_frame_found", seek_audio_frame_found);

			// Seek again... to the nearest Keyframe
			Seek(seeking_frame - 10);
		}
		else
		{
			// SEEK WORKED
			AppendDebugMethod("FFmpegReader::CheckSeek (Successful)", "is_video_seek", is_video_seek, "current_pts", packet->pts, "seeking_pts", seeking_pts, "seeking_frame", seeking_frame, "seek_video_frame_found", seek_video_frame_found, "seek_audio_frame_found", seek_audio_frame_found);

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
void FFmpegReader::ProcessVideoPacket(int requested_frame)
{
	// Calculate current frame #
	int current_frame = ConvertVideoPTStoFrame(GetVideoPTS());

	// Are we close enough to decode the frame?
	if ((current_frame) < (requested_frame - 20))
	{
		// Remove frame and packet
		RemoveAVFrame(pFrame);
		RemoveAVPacket(packet);

		// Debug output
		AppendDebugMethod("FFmpegReader::ProcessVideoPacket (Skipped)", "requested_frame", requested_frame, "current_frame", current_frame, "", -1, "", -1, "", -1, "", -1);

		// Skip to next frame without decoding or caching
		return;
	}

	// Debug output
	AppendDebugMethod("FFmpegReader::ProcessVideoPacket (Before)", "requested_frame", requested_frame, "current_frame", current_frame, "", -1, "", -1, "", -1, "", -1);

	// Init some things local (for OpenMP)
	PixelFormat pix_fmt = pCodecCtx->pix_fmt;
	int height = info.height;
	int width = info.width;
	long int video_length = info.video_length;
	AVPacket *my_packet = packets[packet];
	AVPicture *my_frame = frames[pFrame];

	// Get a scaling context
	SwsContext *img_convert_ctx = image_rescalers[rescaler_position];
	rescaler_position++;
	if (rescaler_position == num_of_rescalers)
		rescaler_position = 0;

	// Add video frame to list of processing video frames
	#pragma omp critical (processing_list)
	processing_video_frames[current_frame] = current_frame;

	// Track 1st video packet after a successful seek
	if (!seek_video_frame_found && is_seeking)
		seek_video_frame_found = current_frame;

	#pragma omp task firstprivate(current_frame, my_packet, my_frame, height, width, video_length, pix_fmt, img_convert_ctx)
	{
		// Create variables for a RGB Frame (since most videos are not in RGB, we must convert it)
		AVFrame *pFrameRGB = NULL;
		int numBytes;
		uint8_t *buffer = NULL;

		// Allocate an AVFrame structure
		pFrameRGB = avcodec_alloc_frame();
		if (pFrameRGB == NULL)
			throw OutOfBoundsFrame("Convert Image Broke!", current_frame, video_length);

		// Determine required buffer size and allocate buffer
		numBytes = avpicture_get_size(PIX_FMT_RGBA, width, height);
		buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t) * 2);

		// Assign appropriate parts of buffer to image planes in pFrameRGB
		// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
		// of AVPicture
		avpicture_fill((AVPicture *) pFrameRGB, buffer, PIX_FMT_RGBA, width, height);

		// Resize / Convert to RGB
		sws_scale(img_convert_ctx, my_frame->data, my_frame->linesize, 0,
				height, pFrameRGB->data, pFrameRGB->linesize);

		// Create or get the existing frame object
		tr1::shared_ptr<Frame> f = CreateFrame(current_frame);

		// Add Image data to frame
		f->AddImage(width, height, 4, QImage::Format_RGBA8888, buffer);

		// Update working cache
		working_cache.Add(f->number, f);

		// Free the RGB image
		av_free(buffer);
		avcodec_free_frame(&pFrameRGB);

		// Remove frame and packet
		RemoveAVFrame(my_frame);
		RemoveAVPacket(my_packet);

		// Remove video frame from list of processing video frames
		{
			const GenericScopedLock<CriticalSection> lock(processingCriticalSection);
			processing_video_frames.erase(current_frame);
			processed_video_frames[current_frame] = current_frame;
		}

		// Debug output
		AppendDebugMethod("FFmpegReader::ProcessVideoPacket (After)", "requested_frame", requested_frame, "current_frame", current_frame, "f->number", f->number, "", -1, "", -1, "", -1);

	} // end omp task

}

// Process an audio packet
void FFmpegReader::ProcessAudioPacket(int requested_frame, int target_frame, int starting_sample)
{
	// Are we close enough to decode the frame's audio?
	if (target_frame < (requested_frame - 20))
	{
		// Remove packet
		RemoveAVPacket(packet);

		// Debug output
		AppendDebugMethod("FFmpegReader::ProcessAudioPacket (Skipped)", "requested_frame", requested_frame, "target_frame", target_frame, "starting_sample", starting_sample, "", -1, "", -1, "", -1);

		// Skip to next frame without decoding or caching
		return;
	}

	// Init some local variables (for OpenMP)
	AVPacket *my_packet = packets[packet];

	// Track 1st audio packet after a successful seek
	if (!seek_audio_frame_found && is_seeking)
		seek_audio_frame_found = target_frame;

	// Debug output
	AppendDebugMethod("FFmpegReader::ProcessAudioPacket (Before)", "requested_frame", requested_frame, "target_frame", target_frame, "starting_sample", starting_sample, "", -1, "", -1, "", -1);

	// Init an AVFrame to hold the decoded audio samples
	int frame_finished = 0;
	AVFrame *audio_frame = avcodec_alloc_frame();
	avcodec_get_frame_defaults(audio_frame);

	int packet_samples = 0;
	int data_size = 0;

	// re-initialize buffer size (it gets changed in the avcodec_decode_audio2 method call)
	int buf_size = AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE;
	int used = avcodec_decode_audio4(aCodecCtx, audio_frame, &frame_finished, my_packet);

	if (used <= 0) {
		// Throw exception
		throw ErrorDecodingAudio("Error decoding audio samples", target_frame);

	} else if (frame_finished) {

		// determine how many samples were decoded
		int planar = av_sample_fmt_is_planar(aCodecCtx->sample_fmt);
		int plane_size = -1;
		data_size = av_samples_get_buffer_size(&plane_size,
				aCodecCtx->channels,
				audio_frame->nb_samples,
				aCodecCtx->sample_fmt, 1);

		// Calculate total number of samples
		packet_samples = audio_frame->nb_samples * aCodecCtx->channels;
	}

	// Estimate the # of samples and the end of this packet's location (to prevent GAPS for the next timestamp)
	int pts_remaining_samples = packet_samples / info.channels; // Adjust for zero based array

	// DEBUG (FOR AUDIO ISSUES) - Get the audio packet start time (in seconds)
	int adjusted_pts = packet->pts + audio_pts_offset;
	double audio_seconds = double(adjusted_pts) * info.audio_timebase.ToDouble();
	double sample_seconds = float(pts_total) / info.sample_rate;

	// Debug output
	AppendDebugMethod("FFmpegReader::ProcessAudioPacket (Decode Info A)", "pts_counter", pts_counter, "PTS", adjusted_pts, "Offset", audio_pts_offset, "PTS Diff", adjusted_pts - prev_pts, "Samples", pts_remaining_samples, "Sample PTS ratio", float(adjusted_pts - prev_pts) / pts_remaining_samples);
	AppendDebugMethod("FFmpegReader::ProcessAudioPacket (Decode Info B)", "Sample Diff", pts_remaining_samples - prev_samples - prev_pts, "Total", pts_total, "PTS Seconds", audio_seconds, "Sample Seconds", sample_seconds, "Seconds Diff", audio_seconds - sample_seconds, "raw samples", packet_samples);

	// DEBUG (FOR AUDIO ISSUES)
	prev_pts = adjusted_pts;
	pts_total += pts_remaining_samples;
	pts_counter++;
	prev_samples = pts_remaining_samples;

	// Add audio frame to list of processing audio frames
	#pragma omp critical (processing_list)
	processing_audio_frames.insert(pair<int, int>(previous_packet_location.frame, previous_packet_location.frame));

	while (pts_remaining_samples)
	{
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
			#pragma omp critical (processing_list)
			processing_audio_frames.insert(pair<int, int>(previous_packet_location.frame, previous_packet_location.frame));

		} else {
			// Increment sample start
			previous_packet_location.sample_start += samples;
		}
	}



	// Process the audio samples in a separate thread (this includes resampling to 16 bit integer, and storing
	// in a openshot::Frame object).
	#pragma omp task firstprivate(requested_frame, target_frame, starting_sample, my_packet, audio_frame)
	{
		// Allocate audio buffer
		int16_t *audio_buf = new int16_t[AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];

		AppendDebugMethod("FFmpegReader::ProcessAudioPacket (ReSample)", "packet_samples", packet_samples, "info.channels", info.channels, "info.sample_rate", info.sample_rate, "aCodecCtx->sample_fmt", aCodecCtx->sample_fmt, "AV_SAMPLE_FMT_S16", AV_SAMPLE_FMT_S16, "", -1);

		// Create output frame
		AVFrame *audio_converted = avcodec_alloc_frame();
		avcodec_get_frame_defaults(audio_converted);
		audio_converted->nb_samples = audio_frame->nb_samples;
		av_samples_alloc(audio_converted->data, audio_converted->linesize, info.channels, audio_frame->nb_samples, AV_SAMPLE_FMT_S16, 0);

		AVAudioResampleContext *avr = NULL;
		int nb_samples = 0;
		#pragma ordered
		{
		// setup resample context
		avr = avresample_alloc_context();
		av_opt_set_int(avr,  "in_channel_layout", aCodecCtx->channel_layout, 0);
		av_opt_set_int(avr, "out_channel_layout", aCodecCtx->channel_layout, 0);
		av_opt_set_int(avr,  "in_sample_fmt",     aCodecCtx->sample_fmt,     0);
		av_opt_set_int(avr, "out_sample_fmt",     AV_SAMPLE_FMT_S16,     0);
		av_opt_set_int(avr,  "in_sample_rate",    info.sample_rate,    0);
		av_opt_set_int(avr, "out_sample_rate",    info.sample_rate,    0);
		av_opt_set_int(avr,  "in_channels",       info.channels,    0);
		av_opt_set_int(avr, "out_channels",       info.channels,    0);
		int r = avresample_open(avr);

		// Convert audio samples
		nb_samples = avresample_convert(avr, 	// audio resample context
				audio_converted->data, 			// output data pointers
				audio_converted->linesize[0], 	// output plane size, in bytes. (0 if unknown)
				audio_converted->nb_samples,	// maximum number of samples that the output buffer can hold
				audio_frame->data,				// input data pointers
				audio_frame->linesize[0],		// input plane size, in bytes (0 if unknown)
				audio_frame->nb_samples);		// number of input samples to convert
		}

		// Copy audio samples over original samples
		memcpy(audio_buf, audio_converted->data[0], audio_converted->nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * info.channels);

		// Deallocate resample buffer
		avresample_close(avr);
		avresample_free(&avr);
		avr = NULL;

		// Free AVFrames
		avcodec_free_frame(&audio_frame);
		av_freep(&audio_converted[0]);
		avcodec_free_frame(&audio_converted);


		int starting_frame_number = -1;
		bool partial_frame = true;
		for (int channel_filter = 0; channel_filter < info.channels; channel_filter++)
		{
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
			for (int sample = 0; sample < packet_samples; sample++)
			{
				// Only add samples for current channel
				if (channel_filter == channel)
				{
					// Add sample (convert from (-32768 to 32768)  to (-1.0 to 1.0))
					channel_buffer[position] = audio_buf[sample] * (1.0f / (1 << 15));

					// Increment audio position
					position++;
				}

				// increment channel (if needed)
				if ((channel + 1) < info.channels)
					// move to next channel
					channel ++;
				else
					// reset channel
					channel = 0;
			}

			// Loop through samples, and add them to the correct frames
			int start = starting_sample;
			int remaining_samples = channel_buffer_size;
			float *iterate_channel_buffer = channel_buffer;	// pointer to channel buffer
			while (remaining_samples > 0)
			{
				// Get Samples per frame (for this frame number)
				int samples_per_frame = Frame::GetSamplesPerFrame(starting_frame_number, info.fps, info.sample_rate, info.channels);

				// Calculate # of samples to add to this frame
				int samples = samples_per_frame - start;
				if (samples > remaining_samples)
					samples = remaining_samples;

				// Create or get the existing frame object
				tr1::shared_ptr<Frame> f = CreateFrame(starting_frame_number);

				// Determine if this frame was "partially" filled in
				if (samples_per_frame == start + samples)
					partial_frame = false;
				else
					partial_frame = true;

				// Add samples for current channel to the frame. Reduce the volume to 98%, to prevent
				// some louder samples from maxing out at 1.0 (not sure why this happens)
				f->AddAudio(true, channel_filter, start, iterate_channel_buffer, samples, 0.98f);

				// Debug output
				AppendDebugMethod("FFmpegReader::ProcessAudioPacket (f->AddAudio)", "frame", starting_frame_number, "start", start, "samples", samples, "channel", channel_filter, "partial_frame", partial_frame, "samples_per_frame", samples_per_frame);

				// Add or update cache
				working_cache.Add(f->number, f);

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
		#pragma omp critical (processing_list)
		{
			// Update all frames as completed
			for (int f = target_frame; f < starting_frame_number; f++) {
				// Remove the frame # from the processing list. NOTE: If more than one thread is
				// processing this frame, the frame # will be in this list multiple times. We are only
				// removing a single instance of it here.
				processing_audio_frames.erase(processing_audio_frames.find(f));

				// Check and see if this frame is also being processed by another thread
				if (processing_audio_frames.count(f) == 0)
					// No other thread is processing it. Mark the audio as processed (final)
					processed_audio_frames[f] = f;
			}
		}

		// Remove this packet
		RemoveAVPacket(my_packet);

		// Debug output
		AppendDebugMethod("FFmpegReader::ProcessAudioPacket (After)", "requested_frame", requested_frame, "starting_frame", target_frame, "end_frame", starting_frame_number - 1, "", -1, "", -1, "", -1);

	} // end task


	// TODO: Fix this bug. Wait on the task to complete. This is not ideal, but solves an issue with the
	// audio_frame being modified by the next call to this method. I think this is a scope issue with OpenMP.
	#pragma omp taskwait
}



// Seek to a specific frame.  This is not always frame accurate, it's more of an estimation on many codecs.
void FFmpegReader::Seek(int requested_frame) throw(TooManySeeks)
{
	// Adjust for a requested frame that is too small or too large
	if (requested_frame < 1)
		requested_frame = 1;
	if (requested_frame > info.video_length)
		requested_frame = info.video_length;

	// Debug output
	AppendDebugMethod("FFmpegReader::Seek", "requested_frame", requested_frame, "seek_count", seek_count, "last_frame", last_frame, "", -1, "", -1, "", -1);

	// Clear working cache (since we are seeking to another location in the file)
	working_cache.Clear();

	// Clear processed lists
	{
		const GenericScopedLock<CriticalSection> lock(processingCriticalSection);
		processing_audio_frames.clear();
		processing_video_frames.clear();
		processed_video_frames.clear();
		processed_audio_frames.clear();
	}

	// Reset the last frame variable
	last_frame = 0;

	// Increment seek count
	seek_count++;

	// too many seeks
	if (seek_count > 10)
		throw TooManySeeks("Too many seek attempts... something seems wrong.", path);

	// If seeking to frame 1, we need to close and re-open the file (this is more reliable than seeking)
	int buffer_amount = 6;
	if (requested_frame - buffer_amount <= 1)
	{
		// Close and re-open file (basically seeking to frame 1)
		Close();
		Open();

		// Not actually seeking, so clear these flags
		is_seeking = false;
		seeking_frame = 1;
		seeking_pts = ConvertFrameToVideoPTS(1);
		seek_audio_frame_found = 0; // used to detect which frames to throw away after a seek
		seek_video_frame_found = 0; // used to detect which frames to throw away after a seek
	}
	else
	{
		// Seek to nearest key-frame (aka, i-frame)
		bool seek_worked = false;

		// Seek video stream (if any)
		int64_t seek_target = ConvertFrameToVideoPTS(requested_frame - buffer_amount);
		if (info.has_video) {
			if (av_seek_frame(pFormatCtx, info.video_stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
				fprintf(stderr, "%s: error while seeking video stream\n", pFormatCtx->filename);
			} else
			{
				// VIDEO SEEK
				is_video_seek = true;
				seek_worked = true;
			}
		}

		// Seek audio stream (if not already seeked... and if an audio stream is found)
		if (!seek_worked && info.has_audio)
		{
			seek_target = ConvertFrameToAudioPTS(requested_frame - buffer_amount);

			if (info.has_audio && av_seek_frame(pFormatCtx, info.audio_stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
				fprintf(stderr, "%s: error while seeking audio stream\n", pFormatCtx->filename);
			} else
			{
				// AUDIO SEEK
				is_video_seek = false;
				seek_worked = true;
			}
		}

		// Was the seek successful?
		if (seek_worked)
		{
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
			seeking_pts = seek_target;
			seeking_frame = requested_frame;
			seek_audio_frame_found = 0; // used to detect which frames to throw away after a seek
			seek_video_frame_found = 0; // used to detect which frames to throw away after a seek

		}
		else
		{
			// seek failed
			is_seeking = false;
			seeking_pts = 0;
			seeking_frame = 0;
		}
	}
}

// Get the PTS for the current video packet
int FFmpegReader::GetVideoPTS()
{
	int current_pts = 0;
	if(packet->dts != AV_NOPTS_VALUE)
		current_pts = packet->dts;

	// Return adjusted PTS
	return current_pts;
}

// Update PTS Offset (if any)
void FFmpegReader::UpdatePTSOffset(bool is_video)
{
	// Determine the offset between the PTS and Frame number (only for 1st frame)
	if (is_video)
	{
		// VIDEO PACKET
		if (video_pts_offset == 99999) // Has the offset been set yet?
			// Find the difference between PTS and frame number
			video_pts_offset = 0 - GetVideoPTS();
	}
	else
	{
		// AUDIO PACKET
		if (audio_pts_offset == 99999) // Has the offset been set yet?
			// Find the difference between PTS and frame number
			audio_pts_offset = 0 - packet->pts;
	}
}

// Convert PTS into Frame Number
int FFmpegReader::ConvertVideoPTStoFrame(int pts)
{
	// Apply PTS offset
	pts = pts + video_pts_offset;

	// Get the video packet start time (in seconds)
	double video_seconds = double(pts) * info.video_timebase.ToDouble();

	// Divide by the video timebase, to get the video frame number (frame # is decimal at this point)
	int frame = round(video_seconds * info.fps.ToDouble()) + 1;

	// Return frame #
	return frame;
}

// Convert Frame Number into Video PTS
int FFmpegReader::ConvertFrameToVideoPTS(int frame_number)
{
	// Get timestamp of this frame (in seconds)
	double seconds = double(frame_number) / info.fps.ToDouble();

	// Calculate the # of video packets in this timestamp
	int video_pts = round(seconds / info.video_timebase.ToDouble());

	// Apply PTS offset (opposite)
	return video_pts - video_pts_offset;
}

// Convert Frame Number into Video PTS
int FFmpegReader::ConvertFrameToAudioPTS(int frame_number)
{
	// Get timestamp of this frame (in seconds)
	double seconds = double(frame_number) / info.fps.ToDouble();

	// Calculate the # of audio packets in this timestamp
	int audio_pts = round(seconds / info.audio_timebase.ToDouble());

	// Apply PTS offset (opposite)
	return audio_pts - audio_pts_offset;
}

// Calculate Starting video frame and sample # for an audio PTS
AudioLocation FFmpegReader::GetAudioPTSLocation(int pts)
{
	// Apply PTS offset
	pts = pts + audio_pts_offset;

	// Get the audio packet start time (in seconds)
	double audio_seconds = double(pts) * info.audio_timebase.ToDouble();

	// Divide by the video timebase, to get the video frame number (frame # is decimal at this point)
	double frame = (audio_seconds * info.fps.ToDouble()) + 1;

	// Frame # as a whole number (no more decimals)
	int whole_frame = int(frame);

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
	if (previous_packet_location.frame != -1 && location.is_near(previous_packet_location, samples_per_frame, samples_per_frame))
	{
		int orig_frame = location.frame;
		int orig_start = location.sample_start;

		// Update sample start, to prevent gaps in audio
		if (previous_packet_location.sample_start <= samples_per_frame)
		{
			location.sample_start = previous_packet_location.sample_start;
			location.frame = previous_packet_location.frame;
		}
		else
		{
			// set to next frame (since we exceeded the # of samples on a frame)
			location.sample_start = 0;
			location.frame++;
		}

		// Debug output
		AppendDebugMethod("FFmpegReader::GetAudioPTSLocation (Audio Gap Detected)", "Source Frame", orig_frame, "Source Audio Sample", orig_start, "Target Frame", location.frame, "Target Audio Sample", location.sample_start, "pts", pts, "", -1);

	}

	// Set previous location
	previous_packet_location = location;

	// Return the associated video frame and starting sample #
	return location;
}

// Create a new Frame (or return an existing one) and add it to the working queue.
tr1::shared_ptr<Frame> FFmpegReader::CreateFrame(int requested_frame)
{
	tr1::shared_ptr<Frame> output;
	// Check working cache
	if (working_cache.Exists(requested_frame))
		// Return existing frame
		output = working_cache.GetFrame(requested_frame);
	else
	{
		// Create a new frame on the working cache
		output = tr1::shared_ptr<Frame>(new Frame(requested_frame, info.width, info.height, "#000000", Frame::GetSamplesPerFrame(requested_frame, info.fps, info.sample_rate, info.channels), info.channels));
		output->SetPixelRatio(info.pixel_ratio.num, info.pixel_ratio.den); // update pixel ratio
		output->ChannelsLayout(info.channel_layout); // update audio channel layout from the parent reader
		output->SampleRate(info.sample_rate); // update the frame's sample rate of the parent reader

		working_cache.Add(requested_frame, output);

		// Set the largest processed frame (if this is larger)
		if (requested_frame > largest_frame_processed)
			largest_frame_processed = requested_frame;
	}

	// Return new frame
	return output;
}

// Determine if frame is partial due to seek
bool FFmpegReader::IsPartialFrame(int requested_frame) {

	// Sometimes a seek gets partial frames, and we need to remove them
	bool seek_trash = false;
	int max_seeked_frame = seek_audio_frame_found; // determine max seeked frame
	if (seek_video_frame_found > max_seeked_frame)
		max_seeked_frame = seek_video_frame_found;
	if ((info.has_audio && seek_audio_frame_found && max_seeked_frame >= requested_frame) ||
	   (info.has_video && seek_video_frame_found && max_seeked_frame >= requested_frame))
	   seek_trash = true;

	return seek_trash;
}

// Check the working queue, and move finished frames to the finished queue
void FFmpegReader::CheckWorkingFrames(bool end_of_stream)
{
	// Loop through all working queue frames
	while (true)
	{
		// Break if no working frames
		if (working_cache.Count() == 0)
			break;

		// Get the front frame of working cache
		tr1::shared_ptr<Frame> f(working_cache.GetSmallestFrame());

		bool is_video_ready = false;
		bool is_audio_ready = false;
		{ // limit scope of next few lines
			const GenericScopedLock<CriticalSection> lock(processingCriticalSection);
			is_video_ready = processed_video_frames.count(f->number);
			is_audio_ready = processed_audio_frames.count(f->number);
		}

		if (previous_packet_location.frame == f->number && !end_of_stream)
			is_audio_ready = false; // don't finalize the last processed audio frame
		bool is_seek_trash = IsPartialFrame(f->number);

		// Adjust for available streams
		if (!info.has_video) is_video_ready = true;
		if (!info.has_audio) is_audio_ready = true;

		// Debug output
		AppendDebugMethod("FFmpegReader::CheckWorkingFrames", "frame_number", f->number, "is_video_ready", is_video_ready, "is_audio_ready", is_audio_ready, "", -1, "", -1, "", -1);

		// Check if working frame is final
		if ((!end_of_stream && is_video_ready && is_audio_ready) || end_of_stream || is_seek_trash || working_cache.Count() >= 200)
		{
			// Debug output
			AppendDebugMethod("FFmpegReader::CheckWorkingFrames (mark frame as final)", "f->number", f->number, "is_seek_trash", is_seek_trash, "Working Cache Count", working_cache.Count(), "Final Cache Count", final_cache.Count(), "", -1, "", -1);

			if (!is_seek_trash)
			{
				// Move frame to final cache
				final_cache.Add(f->number, f);

				// Remove frame from working cache
				working_cache.Remove(f->number);

				// Update last frame processed
				last_frame = f->number;

			} else {
				// Seek trash, so delete the frame from the working cache, and never add it to the final cache.
				working_cache.Remove(f->number);
			}
		}
		else
			// Stop looping
			break;
	}
}

// Check for the correct frames per second (FPS) value by scanning the 1st few seconds of video packets.
void FFmpegReader::CheckFPS()
{
	check_fps = true;
	avpicture_alloc(pFrame, pCodecCtx->pix_fmt, info.width, info.height);

	int first_second_counter = 0;
	int second_second_counter = 0;
	int third_second_counter = 0;
	int forth_second_counter = 0;
	int fifth_second_counter = 0;

	int iterations = 0;
	int threshold = 500;

	// Loop through the stream
	while (true)
	{
		// Get the next packet (if any)
		if (GetNextPacket() < 0)
			// Break loop when no more packets found
			break;

		// Video packet
		if (packet->stream_index == videoStream)
		{
			// Check if the AVFrame is finished and set it
			if (GetAVFrame())
			{
				// Update PTS / Frame Offset (if any)
				UpdatePTSOffset(true);

				// Get PTS of this packet
				int pts = GetVideoPTS();

				// Remove pFrame
				RemoveAVFrame(pFrame);

				// remove packet
				RemoveAVPacket(packet);

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
				else
					// Too far
					break;
			}
			else
				// remove packet
				RemoveAVPacket(packet);
		}
		else
			// remove packet
			RemoveAVPacket(packet);

		// Increment counters
		iterations++;

		// Give up (if threshold exceeded)
		if (iterations > threshold)
			break;
	}

	// Double check that all counters have greater than zero (or give up)
	if (second_second_counter == 0 || third_second_counter == 0 || forth_second_counter == 0 || fifth_second_counter == 0)
	{
		// Seek to frame 1
		Seek(1);

		// exit with no changes to FPS (not enough data to calculate)
		return;
	}

	int sum_fps = second_second_counter + third_second_counter + forth_second_counter + fifth_second_counter;
	int avg_fps = round(sum_fps / 4.0f);

	// Sometimes the FPS is incorrectly detected by FFmpeg.  If the 1st and 2nd seconds counters
	// agree with each other, we are going to adjust the FPS of this reader instance.  Otherwise, print
	// a warning message.

	// Get diff from actual frame rate
	double fps = info.fps.ToDouble();
	double diff = fps - double(avg_fps);

	// Is difference bigger than 1 frame?
	if (diff <= -1 || diff >= 1)
	{
		// Compare to half the frame rate (the most common type of issue)
		double half_fps = Fraction(info.fps.num / 2, info.fps.den).ToDouble();
		diff = half_fps - double(avg_fps);

		// Is difference bigger than 1 frame?
		if (diff <= -1 || diff >= 1)
		{
			// Update FPS for this reader instance
			info.fps = Fraction(avg_fps, 1);
		}
		else
		{
			// Update FPS for this reader instance (to 1/2 the original framerate)
			info.fps = Fraction(info.fps.num / 2, info.fps.den);
		}
	}

	// Seek to frame 1
	Seek(1);
}

// Remove AVFrame from cache (and deallocate it's memory)
void FFmpegReader::RemoveAVFrame(AVPicture* remove_frame)
{
	#pragma omp critical (packet_cache)
	{
		// Remove pFrame (if exists)
		if (frames.count(remove_frame))
		{
			// Free memory
			avpicture_free(frames[remove_frame]);

			// Remove from cache
			frames.erase(remove_frame);

			// Delete the object
			delete remove_frame;
		}

	} // end omp critical
}

// Remove AVPacket from cache (and deallocate it's memory)
void FFmpegReader::RemoveAVPacket(AVPacket* remove_packet)
{
	#pragma omp critical (packet_cache)
	{
		// Remove packet (if any)
		if (packets.count(remove_packet))
		{
			// deallocate memory for packet
			av_free_packet(remove_packet);

			// Remove from cache
			packets.erase(remove_packet);

			// Delete the object
			delete remove_packet;
		}

	} // end omp critical
}

/// Get the smallest video frame that is still being processed
int FFmpegReader::GetSmallestVideoFrame()
{
	// Loop through frame numbers
	map<int, int>::iterator itr;
	int smallest_frame = -1;
	for(itr = processing_video_frames.begin(); itr != processing_video_frames.end(); ++itr)
	{
		if (itr->first < smallest_frame || smallest_frame == -1)
			smallest_frame = itr->first;
	}

	// Return frame number
	return smallest_frame;
}

/// Get the smallest audio frame that is still being processed
int FFmpegReader::GetSmallestAudioFrame()
{
	// Loop through frame numbers
	map<int, int>::iterator itr;
	int smallest_frame = -1;
	for(itr = processing_audio_frames.begin(); itr != processing_audio_frames.end(); ++itr)
	{
		if (itr->first < smallest_frame || smallest_frame == -1)
			smallest_frame = itr->first;
	}

	// Return frame number
	return smallest_frame;
}

// Generate JSON string of this object
string FFmpegReader::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value FFmpegReader::JsonValue() {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "FFmpegReader";
	root["path"] = path;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void FFmpegReader::SetJson(string value) throw(InvalidJSON) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::Reader reader;
	bool success = reader.parse( value, root );
	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)", "");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (exception e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)", "");
	}
}

// Load Json::JsonValue into this object
void FFmpegReader::SetJsonValue(Json::Value root) throw(InvalidFile) {

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["path"].isNull())
		path = root["path"].asString();

	// Re-Open path, and re-init everything (if needed)
	if (is_open)
	{
		Close();
		Open();
	}
}
