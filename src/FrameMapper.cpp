/**
 * @file
 * @brief Source file for the FrameMapper class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
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

#include "../include/FrameMapper.h"

using namespace std;
using namespace openshot;

FrameMapper::FrameMapper(ReaderBase *reader, Fraction target, PulldownType target_pulldown, int target_sample_rate, int target_channels, ChannelLayout target_channel_layout) :
		reader(reader), target(target), pulldown(target_pulldown), final_cache(820 * 1024), is_dirty(true), avr(NULL)
{
	// Set the original frame rate from the reader
	original = Fraction(reader->info.fps.num, reader->info.fps.den);

	// Set all info struct members equal to the internal reader
	info = reader->info;
	info.fps.num = target.num;
	info.fps.den = target.den;
	info.video_timebase.num = target.den;
	info.video_timebase.den = target.num;
	info.video_length = round(info.duration * info.fps.ToDouble());
	info.sample_rate = target_sample_rate;
	info.channels = target_channels;
	info.channel_layout = target_channel_layout;
	info.width = reader->info.width;
	info.height = reader->info.height;

	// Used to toggle odd / even fields
	field_toggle = true;

	// init mapping between original and target frames
	Init();
}

void FrameMapper::AddField(long int frame)
{
	// Add a field, and toggle the odd / even field
	AddField(Field(frame, field_toggle));
}

void FrameMapper::AddField(Field field)
{
	// Add a field to the end of the field list
	fields.push_back(field);

	// toggle the odd / even flag
	field_toggle = (field_toggle ? false : true);
}

// Use the original and target frame rates and a pull-down technique to create
// a mapping between the original fields and frames or a video to a new frame rate.
// This might repeat or skip fields and frames of the original video, depending on
// whether the frame rate is increasing or decreasing.
void FrameMapper::Init()
{
	AppendDebugMethod("FrameMapper::Init (Calculate frame mappings)", "", -1, "", -1, "", -1, "", -1, "", -1, "", -1);

	// Clear the fields & frames lists
	fields.clear();
	frames.clear();

	// Mark as not dirty
	is_dirty = false;

	// Clear cache
	final_cache.Clear();

	// Some framerates are handled special, and some use a generic Keyframe curve to
	// map the framerates. These are the special framerates:
	if ((original.ToInt() == 24 || original.ToInt() == 25 || original.ToInt() == 30) &&
		(target.ToInt() == 24 || target.ToInt() == 25 || target.ToInt() == 30)) {

		// Get the difference (in frames) between the original and target frame rates
		float difference = target.ToInt() - original.ToInt();

		// Find the number (i.e. interval) of fields that need to be skipped or repeated
		int field_interval = 0;
		int frame_interval = 0;

		if (difference != 0)
		{
			field_interval = round(fabs(original.ToInt() / difference));

			// Get frame interval (2 fields per frame)
			frame_interval = field_interval * 2.0f;
		}


		// Calculate # of fields to map
		long int frame = 1;
		long int number_of_fields = reader->info.video_length * 2;

		// Loop through all fields in the original video file
		for (long int field = 1; field <= number_of_fields; field++)
		{

			if (difference == 0) // Same frame rate, NO pull-down or special techniques required
			{
				// Add fields
				AddField(frame);
			}
			else if (difference > 0) // Need to ADD fake fields & frames, because original video has too few frames
			{
				// Add current field
				AddField(frame);

				if (pulldown == PULLDOWN_CLASSIC && field % field_interval == 0)
				{
					// Add extra field for each 'field interval
					AddField(frame);
				}
				else if (pulldown == PULLDOWN_ADVANCED && field % field_interval == 0 && field % frame_interval != 0)
				{
					// Add both extra fields in the middle 'together' (i.e. 2:3:3:2 technique)
					AddField(frame); // add field for current frame

					if (frame + 1 <= info.video_length)
						// add field for next frame (if the next frame exists)
						AddField(Field(frame + 1, field_toggle));
				}
				else if (pulldown == PULLDOWN_NONE && field % frame_interval == 0)
				{
					// No pull-down technique needed, just repeat this frame
					AddField(frame);
					AddField(frame);
				}
			}
			else if (difference < 0) // Need to SKIP fake fields & frames, because we want to return to the original film frame rate
			{

				if (pulldown == PULLDOWN_CLASSIC && field % field_interval == 0)
				{
					// skip current field and toggle the odd/even flag
					field_toggle = (field_toggle ? false : true);
				}
				else if (pulldown == PULLDOWN_ADVANCED && field % field_interval == 0 && field % frame_interval != 0)
				{
					// skip this field, plus the next field
					field++;
				}
				else if (pulldown == PULLDOWN_NONE && frame % field_interval == 0)
				{
					// skip this field, plus the next one
					field++;
				}
				else
				{
					// No skipping needed, so add the field
					AddField(frame);
				}
			}

			// increment frame number (if field is divisible by 2)
			if (field % 2 == 0 && field > 0)
				frame++;
		}

	} else {
		// Map the remaining framerates using a simple Keyframe curve
		// Calculate the difference (to be used as a multiplier)
		float rate_diff = target.ToFloat() / original.ToFloat();
		long int new_length = reader->info.video_length * rate_diff;

		// Build curve for framerate mapping
		Keyframe rate_curve;
		rate_curve.AddPoint(1, 1, LINEAR);
		rate_curve.AddPoint(new_length, reader->info.video_length, LINEAR);

		// Loop through curve, and build list of frames
		for (long int frame_num = 1; frame_num <= new_length; frame_num++)
		{
			// Add 2 fields per frame
			AddField(rate_curve.GetInt(frame_num));
			AddField(rate_curve.GetInt(frame_num));
		}
	}

	// Loop through the target frames again (combining fields into frames)
	Field Odd(0, true);		// temp field used to track the ODD field
	Field Even(0, true);	// temp field used to track the EVEN field

	// Variables used to remap audio samples
	int start_samples_frame = 1;
	int start_samples_position = 0;

	for (long int field = 1; field <= fields.size(); field++)
	{
		// Get the current field
		Field f = fields[field - 1];

		// Is field divisible by 2?
		if (field % 2 == 0 && field > 0)
		{
			// New frame number
			long int frame_number = field / 2;

			// Set the bottom frame
			if (f.isOdd)
				Odd = f;
			else
				Even = f;

			// Determine the range of samples (from the original rate). Resampling happens in real-time when
			// calling the GetFrame() method. So this method only needs to redistribute the original samples with
			// the original sample rate.
			int end_samples_frame = start_samples_frame;
			int end_samples_position = start_samples_position;
			int remaining_samples = Frame::GetSamplesPerFrame(frame_number, target, reader->info.sample_rate, reader->info.channels);

			while (remaining_samples > 0)
			{
				// get original samples
				int original_samples = Frame::GetSamplesPerFrame(end_samples_frame, original, reader->info.sample_rate, reader->info.channels) - end_samples_position;

				// Enough samples
				if (original_samples >= remaining_samples)
				{
					// Take all that we need, and break loop
					end_samples_position += remaining_samples;
					remaining_samples = 0;
				} else
				{
					// Not enough samples (take them all, and keep looping)
					end_samples_frame += 1; // next frame
					end_samples_position = 0; // next frame, starting on 1st sample
					remaining_samples -= original_samples; // reduce the remaining amount
				}
			}



			// Create the sample mapping struct
			SampleRange Samples = {start_samples_frame, start_samples_position, end_samples_frame, end_samples_position, Frame::GetSamplesPerFrame(frame_number, target, reader->info.sample_rate, reader->info.channels)};

			// Reset the audio variables
			start_samples_frame = end_samples_frame;
			start_samples_position = end_samples_position + 1;
			if (start_samples_position >= Frame::GetSamplesPerFrame(start_samples_frame, original, reader->info.sample_rate, reader->info.channels))
			{
				start_samples_frame += 1; // increment the frame (since we need to wrap onto the next one)
				start_samples_position = 0; // reset to 0, since we wrapped
			}

			// Create a frame and ADD it to the frames collection
			MappedFrame frame = {Odd, Even, Samples};
			frames.push_back(frame);
		}
		else
		{
			// Set the top field
			if (f.isOdd)
				Odd = f;
			else
				Even = f;
		}
	}

	// Clear the internal fields list (no longer needed)
	fields.clear();
}

MappedFrame FrameMapper::GetMappedFrame(long int TargetFrameNumber) throw(OutOfBoundsFrame)
{
	// Check if frame number is valid
	if(TargetFrameNumber < 1 || frames.size() == 0)
		// frame too small, return error
		throw OutOfBoundsFrame("An invalid frame was requested.", TargetFrameNumber, frames.size());

	else if (TargetFrameNumber > frames.size())
		// frame too large, set to end frame
		TargetFrameNumber = frames.size();

	// Return frame
	return frames[TargetFrameNumber - 1];
}

// Get an openshot::Frame object for a specific frame number of this reader.
tr1::shared_ptr<Frame> FrameMapper::GetFrame(long int requested_frame) throw(ReaderClosed)
{
	// Check final cache, and just return the frame (if it's available)
	tr1::shared_ptr<Frame> final_frame = final_cache.GetFrame(requested_frame);
	if (final_frame) return final_frame;

	// Create a scoped lock, allowing only a single thread to run the following code at one time
	const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

	// Check if mappings are dirty (and need to be recalculated)
	if (is_dirty)
		// Recalculate mappings
		Init();

	// Check final cache a 2nd time (due to potential lock already generating this frame)
	final_frame = final_cache.GetFrame(requested_frame);
	if (final_frame) return final_frame;

	// Minimum number of frames to process (for performance reasons)
	int minimum_frames = OPEN_MP_NUM_PROCESSORS;

	// Set the number of threads in OpenMP
	omp_set_num_threads(OPEN_MP_NUM_PROCESSORS);
	// Allow nested OpenMP sections
	omp_set_nested(true);

	#pragma omp parallel
	{
		// Debug output
		AppendDebugMethod("FrameMapper::GetFrame (Loop through frames)", "requested_frame", requested_frame, "minimum_frames", minimum_frames, "", -1, "", -1, "", -1, "", -1);

		// Loop through all requested frames, each frame gets it's own thread
		#pragma omp for ordered
		for (long int frame_number = requested_frame; frame_number < requested_frame + minimum_frames; frame_number++)
		{
			// Get the mapped frame
			MappedFrame mapped = GetMappedFrame(frame_number);
			tr1::shared_ptr<Frame> mapped_frame;

			#pragma omp ordered
				mapped_frame = reader->GetFrame(mapped.Odd.Frame);

			int channels_in_frame = mapped_frame->GetAudioChannelsCount();

			// Init some basic properties about this frame
			int samples_in_frame = Frame::GetSamplesPerFrame(frame_number, target, mapped_frame->SampleRate(), channels_in_frame);

			// Create a new frame
			tr1::shared_ptr<Frame> frame = tr1::shared_ptr<Frame>(new Frame(frame_number, 1, 1, "#000000", samples_in_frame, channels_in_frame));
			frame->SampleRate(mapped_frame->SampleRate());
			frame->ChannelsLayout(mapped_frame->ChannelsLayout());


			// Copy the image from the odd field
			tr1::shared_ptr<Frame> odd_frame;
			#pragma omp ordered
			odd_frame = reader->GetFrame(mapped.Odd.Frame);
			if (odd_frame)
				frame->AddImage(tr1::shared_ptr<QImage>(new QImage(*odd_frame->GetImage())), true);
			if (mapped.Odd.Frame != mapped.Even.Frame) {
				// Add even lines (if different than the previous image)
				tr1::shared_ptr<Frame> even_frame;
				#pragma omp ordered
				even_frame = reader->GetFrame(mapped.Even.Frame);
				if (even_frame)
					frame->AddImage(tr1::shared_ptr<QImage>(new QImage(*even_frame->GetImage())), false);
			}


			// Copy the samples
			int samples_copied = 0;
			int starting_frame = mapped.Samples.frame_start;
			while (info.has_audio && samples_copied < mapped.Samples.total)
			{
				// Init number of samples to copy this iteration
				int remaining_samples = mapped.Samples.total - samples_copied;
				int number_to_copy = 0;

				// Loop through each channel
				for (int channel = 0; channel < channels_in_frame; channel++)
				{
					// number of original samples on this frame
					tr1::shared_ptr<Frame> original_frame = reader->GetFrame(starting_frame);
					int original_samples = original_frame->GetAudioSamplesCount();

					if (starting_frame == mapped.Samples.frame_start)
					{
						// Starting frame (take the ending samples)
						number_to_copy = original_samples - mapped.Samples.sample_start;
						if (number_to_copy > remaining_samples)
							number_to_copy = remaining_samples;

						// Add samples to new frame
						#pragma omp critical (openshot_adding_audio)
						frame->AddAudio(true, channel, samples_copied, original_frame->GetAudioSamples(channel) + mapped.Samples.sample_start, number_to_copy, 1.0);
					}
					else if (starting_frame > mapped.Samples.frame_start && starting_frame < mapped.Samples.frame_end)
					{
						// Middle frame (take all samples)
						number_to_copy = original_samples;
						if (number_to_copy > remaining_samples)
							number_to_copy = remaining_samples;

						// Add samples to new frame
						#pragma omp critical (openshot_adding_audio)
						frame->AddAudio(true, channel, samples_copied, original_frame->GetAudioSamples(channel), number_to_copy, 1.0);
					}
					else
					{
						// Ending frame (take the beginning samples)
						number_to_copy = mapped.Samples.sample_end;
						if (number_to_copy > remaining_samples)
							number_to_copy = remaining_samples;

						// Add samples to new frame
						#pragma omp critical (openshot_adding_audio)
						frame->AddAudio(false, channel, samples_copied, original_frame->GetAudioSamples(channel), number_to_copy, 1.0);
					}
				}

				// increment frame
				samples_copied += number_to_copy;
				starting_frame++;
			}

			// Resample audio on frame (if needed)
			if (info.has_audio &&
					( info.sample_rate != frame->SampleRate() ||
					  info.channels != frame->GetAudioChannelsCount() ||
					  info.channel_layout != frame->ChannelsLayout()))
				// Resample audio and correct # of channels if needed
				ResampleMappedAudio(frame, mapped.Odd.Frame);


			// Add frame to final cache
			final_cache.Add(frame->number, frame);

		} // for loop
	} // omp parallel

	// Return processed openshot::Frame
	return final_cache.GetFrame(requested_frame);
}

void FrameMapper::PrintMapping()
{
	// Get the difference (in frames) between the original and target frame rates
	float difference = target.ToInt() - original.ToInt();

	int field_interval = 0;
	int frame_interval = 0;

	if (difference != 0)
	{
		// Find the number (i.e. interval) of fields that need to be skipped or repeated
		field_interval = round(fabs(original.ToInt() / difference));

		// Get frame interval (2 fields per frame)
		frame_interval = field_interval * 2.0f;
	}

	// Loop through frame mappings
	for (float map = 1; map <= frames.size(); map++)
	{
		MappedFrame frame = frames[map - 1];
		cout << "Target frame #: " << map << " mapped to original frame #:\t(" << frame.Odd.Frame << " odd, " << frame.Even.Frame << " even)" << endl;
		cout << "  - Audio samples mapped to frame " << frame.Samples.frame_start << ":" << frame.Samples.sample_start << " to frame " << frame.Samples.frame_end << ":" << frame.Samples.sample_end << endl;
	}

}


// Determine if reader is open or closed
bool FrameMapper::IsOpen() {
	if (reader)
		return reader->IsOpen();
	else
		return false;
}


// Open the internal reader
void FrameMapper::Open() throw(InvalidFile)
{
	if (reader)
	{
		AppendDebugMethod("FrameMapper::Open", "", -1, "", -1, "", -1, "", -1, "", -1, "", -1);

		// Open the reader
		reader->Open();

		// Set child reader in debug mode (if needed)
		if (debug)
			reader->debug = true;
	}
}

// Close the internal reader
void FrameMapper::Close()
{
	if (reader)
	{
		AppendDebugMethod("FrameMapper::Open", "", -1, "", -1, "", -1, "", -1, "", -1, "", -1);

		// Close internal reader
		reader->Close();

		// Deallocate resample buffer
		if (avr) {
			avresample_close(avr);
			avresample_free(&avr);
			avr = NULL;
		}
	}
}


// Generate JSON string of this object
string FrameMapper::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value FrameMapper::JsonValue() {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "FrameMapper";

	// return JsonValue
	return root;
}

// Load JSON string into this object
void FrameMapper::SetJson(string value) throw(InvalidJSON) {

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
void FrameMapper::SetJsonValue(Json::Value root) throw(InvalidFile) {

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Re-Open path, and re-init everything (if needed)
	if (reader) {

		Close();
		Open();
	}
}

// Change frame rate or audio mapping details
void FrameMapper::ChangeMapping(Fraction target_fps, PulldownType target_pulldown,  int target_sample_rate, int target_channels, ChannelLayout target_channel_layout)
{
	AppendDebugMethod("FrameMapper::ChangeMapping", "target_fps.num", target_fps.num, "target_fps.den", target_fps.num, "target_pulldown", target_pulldown, "target_sample_rate", target_sample_rate, "target_channels", target_channels, "target_channel_layout", target_channel_layout);

	// Mark as dirty
	is_dirty = true;

	// Update mapping details
	target = target_fps;
	pulldown = target_pulldown;
	info.sample_rate = target_sample_rate;
	info.channels = target_channels;
	info.channel_layout = target_channel_layout;

	// Clear cache
	final_cache.Clear();

	// Deallocate resample buffer
	if (avr) {
		avresample_close(avr);
		avresample_free(&avr);
		avr = NULL;
	}

}

// Resample audio and map channels (if needed)
void FrameMapper::ResampleMappedAudio(tr1::shared_ptr<Frame> frame, long int original_frame_number)
{
	// Init audio buffers / variables
	int total_frame_samples = 0;
	int channels_in_frame = frame->GetAudioChannelsCount();
	int sample_rate_in_frame = frame->SampleRate();
	int samples_in_frame = frame->GetAudioSamplesCount();
	ChannelLayout channel_layout_in_frame = frame->ChannelsLayout();

	AppendDebugMethod("FrameMapper::ResampleMappedAudio", "frame->number", frame->number, "original_frame_number", original_frame_number, "channels_in_frame", channels_in_frame, "samples_in_frame", samples_in_frame, "sample_rate_in_frame", sample_rate_in_frame, "", -1);

	// Get audio sample array
	float* frame_samples_float = NULL;
	// Get samples interleaved together (c1 c2 c1 c2 c1 c2)
	frame_samples_float = frame->GetInterleavedAudioSamples(sample_rate_in_frame, NULL, &samples_in_frame);

	// Calculate total samples
	total_frame_samples = samples_in_frame * channels_in_frame;

	// Create a new array (to hold all S16 audio samples for the current queued frames)
	int16_t* frame_samples = new int16_t[total_frame_samples];

	// Translate audio sample values back to 16 bit integers
	for (int s = 0; s < total_frame_samples; s++)
		// Translate sample value and copy into buffer
		frame_samples[s] = int(frame_samples_float[s] * (1 << 15));


	// Deallocate float array
	delete[] frame_samples_float;
	frame_samples_float = NULL;

	AppendDebugMethod("FrameMapper::ResampleMappedAudio (got sample data from frame)", "frame->number", frame->number, "total_frame_samples", total_frame_samples, "target channels", info.channels, "channels_in_frame", channels_in_frame, "target sample_rate", info.sample_rate, "samples_in_frame", samples_in_frame);


	// Create input frame (and allocate arrays)
	AVFrame *audio_frame = avcodec_alloc_frame();
	avcodec_get_frame_defaults(audio_frame);
	audio_frame->nb_samples = total_frame_samples / channels_in_frame;

	int error_code = avcodec_fill_audio_frame(audio_frame, channels_in_frame, AV_SAMPLE_FMT_S16, (uint8_t *) frame_samples,
			audio_frame->nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * channels_in_frame, 1);

	if (error_code != 0)
	{
		AppendDebugMethod("FrameMapper::ResampleMappedAudio ERROR [" + (string)av_err2str(error_code) + "]", "error_code", error_code, "", -1, "", -1, "", -1, "", -1, "", -1);
		throw ErrorEncodingVideo("Error while resampling audio in frame mapper", frame->number);
	}

	// Update total samples & input frame size (due to bigger or smaller data types)
	total_frame_samples = Frame::GetSamplesPerFrame(frame->number, target, info.sample_rate, info.channels) + FF_INPUT_BUFFER_PADDING_SIZE;

	AppendDebugMethod("FrameMapper::ResampleMappedAudio (adjust # of samples)", "total_frame_samples", total_frame_samples, "info.sample_rate", info.sample_rate, "sample_rate_in_frame", sample_rate_in_frame, "info.channels", info.channels, "channels_in_frame", channels_in_frame, "original_frame_number", original_frame_number);

	// Create output frame (and allocate arrays)
	AVFrame *audio_converted = avcodec_alloc_frame();
	avcodec_get_frame_defaults(audio_converted);
	audio_converted->nb_samples = total_frame_samples;
	av_samples_alloc(audio_converted->data, audio_converted->linesize, info.channels, total_frame_samples, AV_SAMPLE_FMT_S16, 0);

	AppendDebugMethod("FrameMapper::ResampleMappedAudio (preparing for resample)", "in_sample_fmt", AV_SAMPLE_FMT_S16, "out_sample_fmt", AV_SAMPLE_FMT_S16, "in_sample_rate", sample_rate_in_frame, "out_sample_rate", info.sample_rate, "in_channels", channels_in_frame, "out_channels", info.channels);

	int nb_samples = 0;
	// Force the audio resampling to happen in order (1st thread to last thread), so the waveform
	// is smooth and continuous.
	#pragma omp ordered
	{
		// setup resample context
		if (!avr) {
			avr = avresample_alloc_context();
			av_opt_set_int(avr,  "in_channel_layout", channel_layout_in_frame, 0);
			av_opt_set_int(avr, "out_channel_layout", info.channel_layout, 0);
			av_opt_set_int(avr,  "in_sample_fmt",     AV_SAMPLE_FMT_S16,     0);
			av_opt_set_int(avr, "out_sample_fmt",     AV_SAMPLE_FMT_S16,     0);
			av_opt_set_int(avr,  "in_sample_rate",    sample_rate_in_frame,    0);
			av_opt_set_int(avr, "out_sample_rate",    info.sample_rate,    0);
			av_opt_set_int(avr,  "in_channels",       channels_in_frame,    0);
			av_opt_set_int(avr, "out_channels",       info.channels,    0);
			avresample_open(avr);
		}

		// Convert audio samples
		nb_samples = avresample_convert(avr, 	// audio resample context
				audio_converted->data, 			// output data pointers
				audio_converted->linesize[0], 	// output plane size, in bytes. (0 if unknown)
				audio_converted->nb_samples,	// maximum number of samples that the output buffer can hold
				audio_frame->data,				// input data pointers
				audio_frame->linesize[0],		// input plane size, in bytes (0 if unknown)
				audio_frame->nb_samples);		// number of input samples to convert
	}

	// Create a new array (to hold all resampled S16 audio samples)
	int16_t* resampled_samples = new int16_t[(nb_samples * info.channels) + FF_INPUT_BUFFER_PADDING_SIZE];

	// Copy audio samples over original samples
	memcpy(resampled_samples, audio_converted->data[0], (nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * info.channels));

	// Free frames
	av_freep(&audio_frame[0]);
	avcodec_free_frame(&audio_frame);
	frame_samples = NULL;
	av_freep(&audio_converted[0]);
	avcodec_free_frame(&audio_converted);

	// Resize the frame to hold the right # of channels and samples
	int channel_buffer_size = nb_samples;
	#pragma omp critical (openshot_adding_audio)
	frame->ResizeAudio(info.channels, channel_buffer_size, info.sample_rate, info.channel_layout);

	AppendDebugMethod("FrameMapper::ResampleMappedAudio (Audio successfully resampled)", "nb_samples", nb_samples, "total_frame_samples", total_frame_samples, "info.sample_rate", info.sample_rate, "channels_in_frame", channels_in_frame, "info.channels", info.channels, "info.channel_layout", info.channel_layout);

	// Array of floats (to hold samples for each channel)
	float *channel_buffer = new float[channel_buffer_size];

	// Divide audio into channels. Loop through each channel
	for (int channel_filter = 0; channel_filter < info.channels; channel_filter++)
	{
		// Init array
		for (int z = 0; z < channel_buffer_size; z++)
			channel_buffer[z] = 0.0f;

		// Loop through all samples and add them to our Frame based on channel.
		// Toggle through each channel number, since channel data is stored like (left right left right)
		int channel = 0;
		int position = 0;
		for (int sample = 0; sample < (nb_samples * info.channels); sample++)
		{
			// Only add samples for current channel
			if (channel_filter == channel)
			{
				// Add sample (convert from (-32768 to 32768)  to (-1.0 to 1.0))
				channel_buffer[position] = resampled_samples[sample] * (1.0f / (1 << 15));

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

		// Add samples to frame for this channel
		#pragma omp critical (openshot_adding_audio)
		frame->AddAudio(true, channel_filter, 0, channel_buffer, position, 1.0f);

		AppendDebugMethod("FrameMapper::ResampleMappedAudio (Add audio to channel)", "number of samples", position, "channel_filter", channel_filter, "", -1, "", -1, "", -1, "", -1);
	}

	// Update frame's audio meta data
	frame->SampleRate(info.sample_rate);
	frame->ChannelsLayout(info.channel_layout);

	// clear channel buffer
	delete[] channel_buffer;
	channel_buffer = NULL;

	// Delete arrays
	delete[] resampled_samples;
	resampled_samples = NULL;
}
