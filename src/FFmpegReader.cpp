#include "../include/FFmpegReader.h"

using namespace openshot;

FFmpegReader::FFmpegReader(string path) throw(InvalidFile, NoStreamsFound, InvalidCodec)
	: last_video_frame(0), last_audio_frame(0), is_seeking(0), seeking_pts(0), seeking_frame(0),
	  audio_pts_offset(99999), video_pts_offset(99999), working_cache(50), path(path),
	  is_video_seek(true), check_interlace(false), enable_seek(true) {

	// Open the file (if possible)
	Open();

	// Get Frame 1 (to determine the offset between the PTS and the Frame Number)
	GetFrame(1);
}

void FFmpegReader::Open()
{
	// Register all formats and codecs
	av_register_all();

	// Open video file
	if (av_open_input_file(&pFormatCtx, path.c_str(), NULL, 0, NULL) != 0)
		throw InvalidFile("File could not be opened.", path);

	// Retrieve stream information
	if (av_find_stream_info(pFormatCtx) < 0)
		throw NoStreamsFound("No streams found in file.", path);

	// Dump information about file onto standard error
	//dump_format(pFormatCtx, 0, path.c_str(), 0);

	videoStream = -1;
	audioStream = -1;
	// Loop through each stream, and identify the video and audio stream index
	for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
	{
		// Is this a video stream?
		if (pFormatCtx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO && videoStream < 0) {
			videoStream = i;
		}
		// Is this an audio stream?
		if (pFormatCtx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO && audioStream < 0) {
			audioStream = i;
		}
	}
	if (videoStream == -1 && audioStream == -1)
		throw NoStreamsFound("No video or audio streams found in this file.", path);

	// Init FileInfo struct (clear all values)
	InitFileInfo();

	// Is there a video stream?
	if (videoStream != -1)
	{
		// Set the stream index
		info.video_stream_index = videoStream;

		// Set the codec and codec context pointers
		pStream = pFormatCtx->streams[videoStream];
		pCodecCtx = pFormatCtx->streams[videoStream]->codec;

		// Find the decoder for the video stream
		pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
		if (pCodec == NULL) {
			throw InvalidCodec("A valid video codec could not be found for this file.", path);
		}
		// Open video codec
		if (avcodec_open(pCodecCtx, pCodec) < 0)
			throw InvalidCodec("A video codec was found, but could not be opened.", path);

		// Update the File Info struct with video details (if a video stream is found)
		UpdateVideoInfo();
	}

	// Is there an audio stream?
	if (audioStream != -1)
	{
		// Set the stream index
		info.audio_stream_index = audioStream;

		// Get a pointer to the codec context for the audio stream
		aStream = pFormatCtx->streams[audioStream];
		aCodecCtx = pFormatCtx->streams[audioStream]->codec;

		// Find the decoder for the audio stream
		aCodec = avcodec_find_decoder(aCodecCtx->codec_id);
		if (aCodec == NULL) {
			throw InvalidCodec("A valid audio codec could not be found for this file.", path);
		}
		// Open audio codec
		if (avcodec_open(aCodecCtx, aCodec) < 0)
			throw InvalidCodec("An audio codec was found, but could not be opened.", path);

		// Update the File Info struct with audio details (if an audio stream is found)
		UpdateAudioInfo();
	}

}

void FFmpegReader::Close()
{
	// Delete packet
	//av_free_packet(&packet);

	// Close the codec
	if (info.has_video)
		avcodec_close(pCodecCtx);
	if (info.has_audio)
		avcodec_close(aCodecCtx);

	// Close the video file
	av_close_input_file(pFormatCtx);
}

void FFmpegReader::UpdateAudioInfo()
{
	// Set values of FileInfo struct
	info.has_audio = true;
	info.file_size = pFormatCtx->file_size;
	info.acodec = aCodecCtx->codec->name;
	info.channels = aCodecCtx->channels;
	info.sample_rate = aCodecCtx->sample_rate;
	info.audio_bit_rate = aCodecCtx->bit_rate;

	// Set audio timebase
	info.audio_timebase.num = aStream->time_base.num;
	info.audio_timebase.den = aStream->time_base.den;

	// Timebase of audio stream
	info.duration = aStream->duration * info.audio_timebase.ToDouble();

	// Check for an invalid video length
	if (info.has_video && info.video_length == 0)
	{
		// Calculate the video length from the audio duration
		info.video_length = info.duration * info.fps.ToDouble();
	}

	// Set video timebase (if no video stream was found)
	if (!info.has_video)
	{
		// Set a few important default video settings (so audio can be divided into frames)
		info.fps.num = 30;
		info.fps.den = 1;
		info.video_timebase.num = 1;
		info.video_timebase.den = 30;
		info.video_length = info.duration * info.fps.ToDouble();

	}

}

void FFmpegReader::UpdateVideoInfo()
{
	// Set values of FileInfo struct
	info.has_video = true;
	info.file_size = pFormatCtx->file_size;
	info.height = pCodecCtx->height;
	info.width = pCodecCtx->width;
	info.vcodec = pCodecCtx->codec->name;
	info.video_bit_rate = pFormatCtx->bit_rate;
	info.fps.num = pStream->r_frame_rate.num;
	info.fps.den = pStream->r_frame_rate.den;
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
	info.video_length = round(info.duration * info.fps.ToDouble());

}

Frame FFmpegReader::GetFrame(int requested_frame)
{
	// Check the cache for this frame
	if (final_cache.Exists(requested_frame))
	{
		cout << "Cached Frame!" << endl;
		// Return the cached frame
		return final_cache.GetFrame(requested_frame);
	}
	else
	{
		// Frame is not in cache
		// Adjust for a requested frame that is too small or too large
		if (requested_frame < 1)
			requested_frame = 1;
		if (requested_frame > info.video_length)
			requested_frame = info.video_length;
		if (info.has_video && info.video_length == 0)
			// Invalid duration of video file
			throw InvalidFile("Could not detect the duration of the video or audio stream.", path);

		// Are we within 20 frames of the requested frame?
		int diff = requested_frame - last_video_frame;
		if (abs(diff) >= 0 && abs(diff) <= 19)
		{
			// Continue walking the stream
			cout << " >> CLOSE, SO WALK THE STREAM" << endl;
			return ReadStream(requested_frame);
		}
		else
		{
			// Greater than 20 frames away, we need to seek to the nearest key frame
			cout << " >> TOO FAR, SO SEEK FIRST AND THEN WALK THE STREAM" << endl;
			if (enable_seek)
				// Only seek if enabled
				Seek(requested_frame);

			// Then continue walking the stream
			return ReadStream(requested_frame);
		}

	}
}

// Read the stream until we find the requested Frame
Frame FFmpegReader::ReadStream(int requested_frame)
{
	// Allocate video frame
	pFrame = avcodec_alloc_frame();
	bool end_of_stream = false;

	#pragma XXX omp parallel private(i)
	{
		#pragma XXX omp master
		{
			// Loop through the stream until the correct frame is found
			while (true)
			{
				// Get the next packet (if any)
				if (GetNextPacket() < 0)
				{
					// Break loop when no more packets found
					end_of_stream = true;
					break;
				}

				// Video packet
				if (packet.stream_index == videoStream)
				{
					// Check the status of a seek (if any)
					if (CheckSeek(true))
						// Jump to the next iteration of this loop
						continue;

					// Check if the AVFrame is finished and set it
					if (GetAVFrame())
					{
						cout << endl << "VIDEO PACKET (PTS: " << GetVideoPTS() << ")" << endl;

						// Update PTS / Frame Offset (if any)
						UpdatePTSOffset(true);

						// Process Video Packet
						ProcessVideoPacket(requested_frame);
					}

				}
				// Audio packet
				else if (packet.stream_index == audioStream)
				{
					cout << "AUDIO PACKET (PTS: " << packet.pts << ")" << endl;

					// Check the status of a seek (if any)
					if (CheckSeek(false))
						// Jump to the next iteration of this loop
						continue;

					// Update PTS / Frame Offset (if any)
					UpdatePTSOffset(false);

					// Determine related video frame and starting sample # from audio PTS
					audio_packet_location location = GetAudioPTSLocation(packet.pts);

					// Process Audio Packet
					ProcessAudioPacket(requested_frame, location.frame, location.sample_start);
				}

				// Check if working frames are 'finished'
				if (!is_seeking)
					CheckWorkingFrames(false);

				// Check if requested 'final' frame is available
				if (final_cache.Exists(requested_frame))
					break;

			} // end while

		} // end omp master
	} // end omp parallel

	// Set flag to not get the next packet (since we already got it)
	//needs_packet = false;

	// Delete packet
	//av_free_packet(&packet);

	// Free the YUV frame
	av_free(pFrame);

	// End of stream?  Mark the any other working frames as 'finished'
	if (end_of_stream)
		CheckWorkingFrames(end_of_stream);

	// Return requested frame (if found)
	if (final_cache.Exists(requested_frame))
		// Return prepared frame
		return final_cache.GetFrame(requested_frame);
	else
		// Return blank frame
		return CreateFrame(requested_frame);

}

// Get the next packet (if any)
int FFmpegReader::GetNextPacket()
{
	// Get the next packet (if any)
	return av_read_frame(pFormatCtx, &packet);
}

// Get an AVFrame (if any)
bool FFmpegReader::GetAVFrame()
{
	// Decode video frame
	int frameFinished = 0;
	avcodec_decode_video(pCodecCtx, pFrame, &frameFinished,
			packet.data, packet.size);

	// Detect interlaced frame (only once)
	if (frameFinished && !check_interlace)
	{
		check_interlace = true;
		info.interlaced_frame = pFrame->interlaced_frame;
		info.top_field_first = pFrame->top_field_first;
	}

	// Did we get a video frame?
	return frameFinished;
}

// Check the current seek position and determine if we need to seek again
bool FFmpegReader::CheckSeek(bool is_video)
{
	// Are we seeking for a specific frame?
	if (is_seeking)
	{
		// CHECK VIDEO SEEK?
		int current_pts = 0;
		if (is_video && is_video_seek)
			current_pts = GetVideoPTS();
		// CHECK AUDIO SEEK?
		else if (!is_video && !is_video_seek)
			current_pts = packet.pts;

		// determine if we are "before" the requested frame
		if (current_pts != 0)
		{
			if (current_pts > seeking_pts)
			{
				// SEEKED TOO FAR
				cout << "Woops!  Need to seek backwards further..." << endl;

				// Seek again... to the nearest Keyframe
				Seek(seeking_frame - 10);
			}
			else
			{
				// SEEK WORKED!
				// Seek worked, and we are "before" the requested frame
				is_seeking = false;
				seeking_pts = 0;
				seeking_frame = 0;
			}
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
	last_video_frame = current_frame;

	// Are we close enough to decode the frame?
	if ((current_frame) < (requested_frame - 20))
		// Skip to next frame without decoding or caching
		return;


	#pragma XXX omp task private(current_frame, copyFrame)
	{
		AVPicture copyFrame;
		avpicture_alloc(&copyFrame, pCodecCtx->pix_fmt, info.width, info.height);
		av_picture_copy(&copyFrame, (AVPicture *) pFrame, pCodecCtx->pix_fmt, info.width, info.height);

		// Process Frame
		convert_image(current_frame, &copyFrame, info.width, info.height, pCodecCtx->pix_fmt);

		// Free AVPicture
		avpicture_free(&copyFrame);

	} // end omp task
}

// Process an audio packet
void FFmpegReader::ProcessAudioPacket(int requested_frame, int target_frame, int starting_sample)
{
	// Set last audio frame
	last_audio_frame = target_frame;

	// Are we close enough to decode the frame's audio?
	if (target_frame < (requested_frame - 20))
		// Skip to next frame without decoding or caching
		return;

	// Allocate audio buffer
	int16_t audio_buf[AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];

	int packet_samples = 0;
	while (packet.size > 0) {
		// re-initialize buffer size (it gets changed in the avcodec_decode_audio2 method call)
		int buf_size = AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE;

		// decode audio packet into samples (put samples in the audio_buf array)
		int used = avcodec_decode_audio2(aCodecCtx, audio_buf, &buf_size,
				packet.data, packet.size);

		if (used < 0) {
			// Throw exception
			throw ErrorDecodingAudio("Error decoding audio samples", target_frame);
			packet.size = 0;
			break;
		}

		// Calculate total number of samples
		packet_samples += (buf_size / sizeof(int16_t));

		// process samples...
		packet.data += used;
		packet.size -= used;
	}

	for (int channel_filter = 0; channel_filter < info.channels; channel_filter++)
	{
		// Array of floats (to hold samples for each channel)
		int starting_frame_number = target_frame;
		int channel_buffer_size = (packet_samples / info.channels) + 1;
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

				// Experimental audio conversion
//				switch (aCodecCtx->sample_fmt)
//				{
//				case SAMPLE_FMT_U8:
//					channel_buffer[position] = audio_buf[sample] * (1.0f / (1 << 7));
//				case SAMPLE_FMT_S16:
//					channel_buffer[position] = audio_buf[sample] * (1.0f / (1 << 15));
//				case SAMPLE_FMT_S32:
//					channel_buffer[position] = audio_buf[sample] * (1.0f / (1 << 31));
//				case SAMPLE_FMT_FLT:
//					channel_buffer[position] = audio_buf[sample];
//				case SAMPLE_FMT_DBL:
//					channel_buffer[position] = audio_buf[sample];
//				}

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

		// Get Samples per frame
		int samples_per_frame = GetSamplesPerFrame();

		// Loop through samples, and add them to the correct frames
		int start = starting_sample;
		int remaining_samples = channel_buffer_size;
		while (remaining_samples > 0)
		{
			// Create or get frame object
			Frame f = CreateFrame(starting_frame_number);
			last_audio_frame = starting_frame_number;

			// Calculate # of samples to add to this frame
			int samples = samples_per_frame - start;
			if (samples > remaining_samples)
				samples = remaining_samples;

			// Add samples for current channel to the frame
			f.AddAudio(channel_filter, start, channel_buffer, samples, 1.0f);

			// Update working cache
			working_cache.Add(starting_frame_number, f);

			// Decrement remaining samples
			remaining_samples -= samples;

			// Increment frame number
			starting_frame_number++;

			// Reset starting sample #
			start = 0;
		}

		// clear channel buffer
		delete channel_buffer;
		channel_buffer = NULL;
	}
}



// Seek to a specific frame.  This is not always frame accurate, it's more of an estimation on many codecs.
void FFmpegReader::Seek(int requested_frame)
{
	// Adjust for a requested frame that is too small or too large
	if (requested_frame < 1)
		requested_frame = 1;
	if (requested_frame > info.video_length)
		requested_frame = info.video_length;

	// Clear working cache (since we are seeking to another location in the file)
	working_cache.Clear();

	// Reset the last frame variables
	last_video_frame = 0;
	last_audio_frame = 0;

	// Set seeking flags
	int64_t seek_target = 0;

	// Find a valid stream index
	int stream_index = -1;
	if (info.has_video)
	{
		// VIDEO SEEK
		is_video_seek = true;
		stream_index = info.video_stream_index;

		// Calculate seek target
		seek_target = ConvertFrameToVideoPTS(requested_frame - 3);
		//seek_target = ((double)seeking_pts * info.video_timebase.ToDouble()) * (double)AV_TIME_BASE;
	}
	else if (info.has_audio)
	{
		// AUDIO SEEK
		is_video_seek = false;
		stream_index = info.audio_stream_index;

		// Calculate seek target
		seek_target = ConvertFrameToAudioPTS(requested_frame - 3); // Seek a few frames prior to the requested frame (to avoid missing some samples)
		//seek_target = ((double)seeking_pts * info.audio_timebase.ToDouble()) * (double)AV_TIME_BASE;
	}

	// If valid stream, rescale timestamp so the av_seek_frame method will understand it
//	if (stream_index >= 0) {
//		seek_target = av_rescale_q(seek_target, AV_TIME_BASE_Q,
//				pFormatCtx->streams[stream_index]->time_base);
//	}

	// If seeking to frame 1, we need to close and re-open the file (this is more reliable than seeking)
	if (requested_frame == 1)
	{
		// Close and re-open file (basically seeking to frame 1)
		Close();
		Open();

		// Not actually seeking, so clear these flags
		is_seeking = false;
		seeking_pts = ConvertFrameToVideoPTS(1);
	}
	else
	{
		// Seek to nearest key-frame (aka, i-frame)
		if (av_seek_frame(pFormatCtx, stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
			fprintf(stderr, "%s: error while seeking\n", pFormatCtx->filename);
		}
		else
		{
			// If not already seeking, set flags
			if (!is_seeking)
			{
				// init seek flags
				is_seeking = true;
				seeking_frame = requested_frame;
				seeking_pts = seek_target;
			}
			else
			{
				// update seek frame number
				seeking_frame = requested_frame;
			}
		}
	}

	// Flush buffers
	if (info.has_video)
		avcodec_flush_buffers(pCodecCtx);
	if (info.has_audio)
		avcodec_flush_buffers(aCodecCtx);
}

// Convert image to RGB format
void FFmpegReader::convert_image(int current_frame, AVPicture *copyFrame, int width, int height, PixelFormat pix_fmt)
{
	AVFrame *pFrameRGB = NULL;
	int numBytes;
	uint8_t *buffer = NULL;

	// Allocate an AVFrame structure
	pFrameRGB = avcodec_alloc_frame();
	if (pFrameRGB == NULL)
		throw OutOfBoundsFrame("Convert Image Broke!", current_frame, info.video_length);

	// Determine required buffer size and allocate buffer
	numBytes = avpicture_get_size(PIX_FMT_RGB24, width, height);
	buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *) pFrameRGB, buffer, PIX_FMT_RGB24, width, height);

	struct SwsContext *img_convert_ctx = NULL;

	// Convert the image into RGB (for ImageMagick++)
	if(img_convert_ctx == NULL) {
	     img_convert_ctx = sws_getContext(width, height, pix_fmt, width, height,
	                      PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
	     if(img_convert_ctx == NULL) {
	      fprintf(stderr, "Cannot initialize the conversion context!\n");
	      exit(1);
	     }
	}

	// Resize / Convert to RGB
	sws_scale(img_convert_ctx, copyFrame->data, copyFrame->linesize,
	 0, height, pFrameRGB->data, pFrameRGB->linesize);

	// Create or get frame object
	Frame f = CreateFrame(current_frame);

	// Add Image data to frame
	f.AddImage(width, height, "RGB", Magick::CharPixel, buffer);

	// Update working cache
	working_cache.Add(f.number, f);

	// Add interlaced image (if any)
	if (info.interlaced_frame)
	{
		// Update working cache
		Frame f1 = CreateFrame(current_frame + 1);

		// Add Image data to frame
		f1.AddImage(width, height, "RGB", Magick::CharPixel, buffer);

		// Update working cache
		working_cache.Add(f1.number, f1);
	}

	// Free the RGB image
	av_free(buffer);
	av_free(pFrameRGB);
}

// Get the PTS for the current video packet
int FFmpegReader::GetVideoPTS()
{
	int current_pts = 0;
	if(packet.dts != AV_NOPTS_VALUE)
		current_pts = packet.dts;

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
			audio_pts_offset = 0 - packet.pts;
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
audio_packet_location FFmpegReader::GetAudioPTSLocation(int pts)
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
	int samples_per_frame = GetSamplesPerFrame();

	// Calculate the sample # to start on
	int sample_start = round(double(samples_per_frame) * sample_start_percentage);

	// Prepare final audio packet location
	audio_packet_location location = {whole_frame, sample_start};

	// Return the associated video frame and starting sample #
	return location;
}

// Calculate the # of samples per video frame
int FFmpegReader::GetSamplesPerFrame()
{
	// Get the number of samples per video frame (sample rate X reciprocal of frame rate)
	return round(double(info.sample_rate) / info.fps.ToDouble());
}

// Create a new Frame (or return an existing one) and add it to the working queue.
Frame FFmpegReader::CreateFrame(int requested_frame)
{
	// Check working cache
	if (working_cache.Exists(requested_frame))
		// Return existing frame
		return working_cache.GetFrame(requested_frame);
	else
	{
		// Get Samples per frame
		int samples_per_frame = GetSamplesPerFrame();

		// Create a new frame on the working cache
		Frame f(requested_frame, info.width, info.height, "#000000", samples_per_frame, info.channels);
		f.SetPixelRatio(info.pixel_ratio.num, info.pixel_ratio.den);
		f.SetSampleRate(info.sample_rate);
		working_cache.Add(requested_frame, f);

		// Return new frame
		return f;
	}
}

// Check the working queue, and move finished frames to the finished queue
void FFmpegReader::CheckWorkingFrames(bool end_of_stream)
{
	// Adjust for video only, or audio only
	if (!info.has_video)
		last_video_frame = last_audio_frame;
	if (!info.has_audio)
		last_audio_frame = last_video_frame;

	// Loop through all working queue frames
	while (true)
	{
		// Break if no working frames
		if (working_cache.Count() == 0)
			break;

		// Get the front frame of working cache
		Frame f = working_cache.GetSmallestFrame();

		// Check if working frame is final
		if ((!end_of_stream && f.number <= last_video_frame && f.number < last_audio_frame) || end_of_stream)
		{
			// Move frame to final cache
			final_cache.Add(f.number, f);

			// Remove frame from working cache
			working_cache.Remove(f.number);
		}
		else
			// Stop looping
			break;
	}
}

