/**
 * @file
 * @brief Source file for the FrameMapper class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
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
		reader(reader), target(target), pulldown(target_pulldown), is_dirty(true), avr(NULL)
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

	// Adjust cache size based on size of frame and audio
	final_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);
}

// Destructor
FrameMapper::~FrameMapper() {
	if (is_open)
		// Auto Close if not already
		Close();

	reader = NULL;
}

/// Get the current reader
ReaderBase* FrameMapper::Reader()
{
    if (reader)
        return reader;
    else
        // Throw error if reader not initialized
        throw ReaderClosed("No Reader has been initialized for FrameMapper.  Call Reader(*reader) before calling this method.");
}

void FrameMapper::AddField(int64_t frame)
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
	ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::Init (Calculate frame mappings)");

	// Do not initialize anything if just a picture with no audio
	if (info.has_video and !info.has_audio and info.has_single_image)
		// Skip initialization
		return;

	// Clear the fields & frames lists
	fields.clear();
	frames.clear();

	// Mark as not dirty
	is_dirty = false;

	// Clear cache
	final_cache.Clear();

	// Some framerates are handled special, and some use a generic Keyframe curve to
	// map the framerates. These are the special framerates:
	if ((fabs(original.ToFloat() - 24.0) < 1e-7 || fabs(original.ToFloat() - 25.0) < 1e-7 || fabs(original.ToFloat() - 30.0) < 1e-7) &&
		(fabs(target.ToFloat() - 24.0) < 1e-7 || fabs(target.ToFloat() - 25.0) < 1e-7 || fabs(target.ToFloat() - 30.0) < 1e-7)) {

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
		int64_t frame = 1;
		int64_t number_of_fields = reader->info.video_length * 2;

		// Loop through all fields in the original video file
		for (int64_t field = 1; field <= number_of_fields; field++)
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
		// Map the remaining framerates using a linear algorithm
		double rate_diff = target.ToDouble() / original.ToDouble();
		int64_t new_length = reader->info.video_length * rate_diff;

		// Calculate the value difference
		double value_increment = (reader->info.video_length + 1) / (double) (new_length);

		// Loop through curve, and build list of frames
		double original_frame_num = 1.0f;
		for (int64_t frame_num = 1; frame_num <= new_length; frame_num++)
		{
			// Add 2 fields per frame
			AddField(round(original_frame_num));
			AddField(round(original_frame_num));

			// Increment original frame number
			original_frame_num += value_increment;
		}
	}

	// Loop through the target frames again (combining fields into frames)
	Field Odd(0, true);		// temp field used to track the ODD field
	Field Even(0, true);	// temp field used to track the EVEN field

	// Variables used to remap audio samples
	int64_t start_samples_frame = 1;
	int start_samples_position = 0;

	for (std::vector<Field>::size_type field = 1; field <= fields.size(); field++)
	{
		// Get the current field
		Field f = fields[field - 1];

		// Is field divisible by 2?
		if (field % 2 == 0 && field > 0)
		{
			// New frame number
			int64_t frame_number = field / 2;

			// Set the bottom frame
			if (f.isOdd)
				Odd = f;
			else
				Even = f;

			// Determine the range of samples (from the original rate). Resampling happens in real-time when
			// calling the GetFrame() method. So this method only needs to redistribute the original samples with
			// the original sample rate.
			int64_t end_samples_frame = start_samples_frame;
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
					end_samples_position += remaining_samples - 1;
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

MappedFrame FrameMapper::GetMappedFrame(int64_t TargetFrameNumber)
{
	// Check if mappings are dirty (and need to be recalculated)
	if (is_dirty)
		// Recalculate mappings
		Init();

	// Ignore mapping on single image readers
	if (info.has_video and !info.has_audio and info.has_single_image) {
		// Return the same number
		MappedFrame frame;
		frame.Even.Frame = TargetFrameNumber;
		frame.Odd.Frame = TargetFrameNumber;
		frame.Samples.frame_start = 0;
		frame.Samples.frame_end = 0;
		frame.Samples.sample_start = 0;
		frame.Samples.sample_end = 0;
		frame.Samples.total = 0;
		return frame;
	}

	// Check if frame number is valid
	if(TargetFrameNumber < 1 || frames.size() == 0)
		// frame too small, return error
		throw OutOfBoundsFrame("An invalid frame was requested.", TargetFrameNumber, frames.size());

	else if (TargetFrameNumber > (int64_t)frames.size())
		// frame too large, set to end frame
		TargetFrameNumber = frames.size();

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::GetMappedFrame", "TargetFrameNumber", TargetFrameNumber, "frames.size()", frames.size(), "frames[...].Odd", frames[TargetFrameNumber - 1].Odd.Frame, "frames[...].Even", frames[TargetFrameNumber - 1].Even.Frame);

	// Return frame
	return frames[TargetFrameNumber - 1];
}

// Get or generate a blank frame
std::shared_ptr<Frame> FrameMapper::GetOrCreateFrame(int64_t number)
{
	std::shared_ptr<Frame> new_frame;

	// Init some basic properties about this frame (keep sample rate and # channels the same as the original reader for now)
	int samples_in_frame = Frame::GetSamplesPerFrame(number, target, reader->info.sample_rate, reader->info.channels);

	try {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::GetOrCreateFrame (from reader)", "number", number, "samples_in_frame", samples_in_frame);

		// Attempt to get a frame (but this could fail if a reader has just been closed)
		new_frame = reader->GetFrame(number);

		// Return real frame
		return new_frame;

	} catch (const ReaderClosed & e) {
		// ...
	} catch (const TooManySeeks & e) {
		// ...
	} catch (const OutOfBoundsFrame & e) {
		// ...
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::GetOrCreateFrame (create blank)", "number", number, "samples_in_frame", samples_in_frame);

	// Create blank frame
	new_frame = std::make_shared<Frame>(number, info.width, info.height, "#000000", samples_in_frame, reader->info.channels);
	new_frame->SampleRate(reader->info.sample_rate);
	new_frame->ChannelsLayout(info.channel_layout);
	new_frame->AddAudioSilence(samples_in_frame);
	return new_frame;
}

// Get an openshot::Frame object for a specific frame number of this reader.
std::shared_ptr<Frame> FrameMapper::GetFrame(int64_t requested_frame)
{
	// Check final cache, and just return the frame (if it's available)
	std::shared_ptr<Frame> final_frame = final_cache.GetFrame(requested_frame);
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
	// Dialing this down to 1 for now, as it seems to improve performance, and reduce export crashes
	int minimum_frames = 1;

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::GetFrame (Loop through frames)", "requested_frame", requested_frame, "minimum_frames", minimum_frames);

	// Loop through all requested frames
	for (int64_t frame_number = requested_frame; frame_number < requested_frame + minimum_frames; frame_number++)
	{

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::GetFrame (inside omp for loop)", "frame_number", frame_number, "minimum_frames", minimum_frames, "requested_frame", requested_frame);

		// Get the mapped frame
		MappedFrame mapped = GetMappedFrame(frame_number);
		std::shared_ptr<Frame> mapped_frame;

		// Get the mapped frame (keeping the sample rate and channels the same as the original... for the moment)
		mapped_frame = GetOrCreateFrame(mapped.Odd.Frame);

		// Get # of channels in the actual frame
		int channels_in_frame = mapped_frame->GetAudioChannelsCount();
		int samples_in_frame = Frame::GetSamplesPerFrame(frame_number, target, mapped_frame->SampleRate(), channels_in_frame);

		// Determine if mapped frame is identical to source frame
		// including audio sample distribution according to mapped.Samples,
		// and frame_number. In some cases such as end of stream, the reader
		// will return a frame with a different frame number. In these cases,
		// we cannot use the frame as is, nor can we modify the frame number,
		// otherwise the reader's cache object internals become invalid.
		if (info.sample_rate == mapped_frame->SampleRate() &&
			info.channels == mapped_frame->GetAudioChannelsCount() &&
			info.channel_layout == mapped_frame->ChannelsLayout() &&
			mapped.Samples.total == mapped_frame->GetAudioSamplesCount() &&
			mapped.Samples.frame_start == mapped.Odd.Frame &&
			mapped.Samples.sample_start == 0 &&
			mapped_frame->number == frame_number &&// in some conditions (e.g. end of stream)
			info.fps.num == reader->info.fps.num &&
			info.fps.den == reader->info.fps.den) {
				// Add original frame to cache, and skip the rest (for performance reasons)
				final_cache.Add(mapped_frame);
				continue;
		}

		// Create a new frame
		std::shared_ptr<Frame> frame = std::make_shared<Frame>(frame_number, 1, 1, "#000000", samples_in_frame, channels_in_frame);
		frame->SampleRate(mapped_frame->SampleRate());
		frame->ChannelsLayout(mapped_frame->ChannelsLayout());


		// Copy the image from the odd field
		std::shared_ptr<Frame> odd_frame;
		odd_frame = GetOrCreateFrame(mapped.Odd.Frame);

		if (odd_frame)
			frame->AddImage(std::shared_ptr<QImage>(new QImage(*odd_frame->GetImage())), true);
		if (mapped.Odd.Frame != mapped.Even.Frame) {
			// Add even lines (if different than the previous image)
			std::shared_ptr<Frame> even_frame;
			even_frame = GetOrCreateFrame(mapped.Even.Frame);
			if (even_frame)
				frame->AddImage(std::shared_ptr<QImage>(new QImage(*even_frame->GetImage())), false);
		}

		// Resample audio on frame (if needed)
		bool need_resampling = false;
		if (info.has_audio &&
			(info.sample_rate != frame->SampleRate() ||
			 info.channels != frame->GetAudioChannelsCount() ||
			 info.channel_layout != frame->ChannelsLayout()))
			// Resample audio and correct # of channels if needed
			need_resampling = true;

		// create a copy of mapped.Samples that will be used by copy loop
		SampleRange copy_samples = mapped.Samples;

		if (need_resampling)
		{
			// Resampling needed, modify copy of SampleRange object that
			// includes some additional input samples on first iteration,
			// and continues the offset to ensure that the sample rate
			// converter isn't input limited.
			const int EXTRA_INPUT_SAMPLES = 20;

			// Extend end sample count by an additional EXTRA_INPUT_SAMPLES samples
			copy_samples.sample_end += EXTRA_INPUT_SAMPLES;
			int samples_per_end_frame =
				Frame::GetSamplesPerFrame(copy_samples.frame_end, original,
										  reader->info.sample_rate, reader->info.channels);
			if (copy_samples.sample_end >= samples_per_end_frame)
			{
				// check for wrapping
				copy_samples.frame_end++;
				copy_samples.sample_end -= samples_per_end_frame;
			}
			copy_samples.total += EXTRA_INPUT_SAMPLES;

			if (avr) {
				// Sample rate conversion has been allocated on this clip, so
				// this is not the first iteration. Extend start position by
				// EXTRA_INPUT_SAMPLES to keep step with previous frame
				copy_samples.sample_start += EXTRA_INPUT_SAMPLES;
				int samples_per_start_frame =
					Frame::GetSamplesPerFrame(copy_samples.frame_start, original,
											  reader->info.sample_rate, reader->info.channels);
				if (copy_samples.sample_start >= samples_per_start_frame)
				{
					// check for wrapping
					copy_samples.frame_start++;
					copy_samples.sample_start -= samples_per_start_frame;
				}
				copy_samples.total -= EXTRA_INPUT_SAMPLES;
			}
		}

		// Copy the samples
		int samples_copied = 0;
		int64_t starting_frame = copy_samples.frame_start;
		while (info.has_audio && samples_copied < copy_samples.total)
		{
			// Init number of samples to copy this iteration
			int remaining_samples = copy_samples.total - samples_copied;
			int number_to_copy = 0;

			// number of original samples on this frame
			std::shared_ptr<Frame> original_frame = GetOrCreateFrame(starting_frame);
			int original_samples = original_frame->GetAudioSamplesCount();

			// Loop through each channel
			for (int channel = 0; channel < channels_in_frame; channel++)
			{
				if (starting_frame == copy_samples.frame_start)
				{
					// Starting frame (take the ending samples)
					number_to_copy = original_samples - copy_samples.sample_start;
					if (number_to_copy > remaining_samples)
						number_to_copy = remaining_samples;

					// Add samples to new frame
					frame->AddAudio(true, channel, samples_copied, original_frame->GetAudioSamples(channel) + copy_samples.sample_start, number_to_copy, 1.0);
				}
				else if (starting_frame > copy_samples.frame_start && starting_frame < copy_samples.frame_end)
				{
					// Middle frame (take all samples)
					number_to_copy = original_samples;
					if (number_to_copy > remaining_samples)
						number_to_copy = remaining_samples;

					// Add samples to new frame
					frame->AddAudio(true, channel, samples_copied, original_frame->GetAudioSamples(channel), number_to_copy, 1.0);
				}
				else
				{
					// Ending frame (take the beginning samples)
					number_to_copy = copy_samples.sample_end + 1;
					if (number_to_copy > remaining_samples)
						number_to_copy = remaining_samples;

					// Add samples to new frame
					frame->AddAudio(false, channel, samples_copied, original_frame->GetAudioSamples(channel), number_to_copy, 1.0);
				}
			}

			// increment frame
			samples_copied += number_to_copy;
			starting_frame++;
		}

		// Resample audio on frame (if needed)
		if (need_resampling)
			// Resample audio and correct # of channels if needed
			ResampleMappedAudio(frame, mapped.Odd.Frame);

		// Add frame to final cache
		final_cache.Add(frame);

	} // for loop

	// Return processed openshot::Frame
	return final_cache.GetFrame(requested_frame);
}

void FrameMapper::PrintMapping()
{
	// Check if mappings are dirty (and need to be recalculated)
	if (is_dirty)
		// Recalculate mappings
		Init();

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
void FrameMapper::Open()
{
	if (reader)
	{
		ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::Open");

		// Open the reader
		reader->Open();
	}
}

// Close the internal reader
void FrameMapper::Close()
{
	if (reader)
	{
		// Create a scoped lock, allowing only a single thread to run the following code at one time
		const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

		ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::Close");

		// Close internal reader
		reader->Close();

		// Clear the fields & frames lists
		fields.clear();
		frames.clear();

		// Mark as dirty
		is_dirty = true;

		// Clear cache
		final_cache.Clear();

		// Deallocate resample buffer
		if (avr) {
			SWR_CLOSE(avr);
			SWR_FREE(&avr);
			avr = NULL;
		}
	}
}


// Generate JSON string of this object
std::string FrameMapper::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value FrameMapper::JsonValue() const {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "FrameMapper";

	// return JsonValue
	return root;
}

// Load JSON string into this object
void FrameMapper::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void FrameMapper::SetJsonValue(const Json::Value root) {

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
	ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::ChangeMapping", "target_fps.num", target_fps.num, "target_fps.den", target_fps.den, "target_pulldown", target_pulldown, "target_sample_rate", target_sample_rate, "target_channels", target_channels, "target_channel_layout", target_channel_layout);

	// Mark as dirty
	is_dirty = true;

	// Update mapping details
	target.num = target_fps.num;
	target.den = target_fps.den;
	info.fps.num = target_fps.num;
	info.fps.den = target_fps.den;
	info.video_timebase.num = target_fps.den;
	info.video_timebase.den = target_fps.num;
	pulldown = target_pulldown;
	info.sample_rate = target_sample_rate;
	info.channels = target_channels;
	info.channel_layout = target_channel_layout;

	// Clear cache
	final_cache.Clear();

	// Adjust cache size based on size of frame and audio
	final_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);

	// Deallocate resample buffer
	if (avr) {
		SWR_CLOSE(avr);
		SWR_FREE(&avr);
		avr = NULL;
	}
}

// Resample audio and map channels (if needed)
void FrameMapper::ResampleMappedAudio(std::shared_ptr<Frame> frame, int64_t original_frame_number)
{
	// Check if mappings are dirty (and need to be recalculated)
	if (is_dirty)
		// Recalculate mappings
		Init();

	// Init audio buffers / variables
	int total_frame_samples = 0;
	int channels_in_frame = frame->GetAudioChannelsCount();
	int sample_rate_in_frame = frame->SampleRate();
	int samples_in_frame = frame->GetAudioSamplesCount();
	ChannelLayout channel_layout_in_frame = frame->ChannelsLayout();

	ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::ResampleMappedAudio", "frame->number", frame->number, "original_frame_number", original_frame_number, "channels_in_frame", channels_in_frame, "samples_in_frame", samples_in_frame, "sample_rate_in_frame", sample_rate_in_frame);

	// Get audio sample array
	float* frame_samples_float = NULL;
	// Get samples interleaved together (c1 c2 c1 c2 c1 c2)
	frame_samples_float = frame->GetInterleavedAudioSamples(sample_rate_in_frame, NULL, &samples_in_frame);

	// Calculate total samples
	total_frame_samples = samples_in_frame * channels_in_frame;

	// Create a new array (to hold all S16 audio samples for the current queued frames)
 	int16_t* frame_samples = (int16_t*) av_malloc(sizeof(int16_t)*total_frame_samples);

	// Translate audio sample values back to 16 bit integers with saturation
	float valF;
	int16_t conv;
	const int16_t max16 = 32767;
	const int16_t min16 = -32768;
	for (int s = 0; s < total_frame_samples; s++) {
		valF = frame_samples_float[s] * (1 << 15);
		if (valF > max16)
			conv = max16;
		else if (valF < min16)
			conv = min16;
		else
			conv = int(valF + 32768.5) - 32768; // +0.5 is for rounding

		// Copy into buffer
		frame_samples[s] = conv;
	}


	// Deallocate float array
	delete[] frame_samples_float;
	frame_samples_float = NULL;

	ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::ResampleMappedAudio (got sample data from frame)", "frame->number", frame->number, "total_frame_samples", total_frame_samples, "target channels", info.channels, "channels_in_frame", channels_in_frame, "target sample_rate", info.sample_rate, "samples_in_frame", samples_in_frame);


	// Create input frame (and allocate arrays)
	AVFrame *audio_frame = AV_ALLOCATE_FRAME();
	AV_RESET_FRAME(audio_frame);
	audio_frame->nb_samples = total_frame_samples / channels_in_frame;

	int error_code = avcodec_fill_audio_frame(audio_frame, channels_in_frame, AV_SAMPLE_FMT_S16, (uint8_t *) frame_samples,
			audio_frame->nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * channels_in_frame, 1);

	if (error_code < 0)
	{
		ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::ResampleMappedAudio ERROR [" + (std::string)av_err2str(error_code) + "]", "error_code", error_code);
		throw ErrorEncodingVideo("Error while resampling audio in frame mapper", frame->number);
	}

	// Update total samples & input frame size (due to bigger or smaller data types)
	total_frame_samples = Frame::GetSamplesPerFrame(frame->number, target, info.sample_rate, info.channels);

	ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::ResampleMappedAudio (adjust # of samples)", "total_frame_samples", total_frame_samples, "info.sample_rate", info.sample_rate, "sample_rate_in_frame", sample_rate_in_frame, "info.channels", info.channels, "channels_in_frame", channels_in_frame, "original_frame_number", original_frame_number);

	// Create output frame (and allocate arrays)
	AVFrame *audio_converted = AV_ALLOCATE_FRAME();
	AV_RESET_FRAME(audio_converted);
	audio_converted->nb_samples = total_frame_samples;
	av_samples_alloc(audio_converted->data, audio_converted->linesize, info.channels, total_frame_samples, AV_SAMPLE_FMT_S16, 0);

	ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::ResampleMappedAudio (preparing for resample)", "in_sample_fmt", AV_SAMPLE_FMT_S16, "out_sample_fmt", AV_SAMPLE_FMT_S16, "in_sample_rate", sample_rate_in_frame, "out_sample_rate", info.sample_rate, "in_channels", channels_in_frame, "out_channels", info.channels);

	int nb_samples = 0;

    // setup resample context
    if (!avr) {
        avr = SWR_ALLOC();
        av_opt_set_int(avr,  "in_channel_layout", channel_layout_in_frame, 0);
        av_opt_set_int(avr, "out_channel_layout", info.channel_layout, 0);
        av_opt_set_int(avr,  "in_sample_fmt",     AV_SAMPLE_FMT_S16,     0);
        av_opt_set_int(avr, "out_sample_fmt",     AV_SAMPLE_FMT_S16,     0);
        av_opt_set_int(avr,  "in_sample_rate",    sample_rate_in_frame,    0);
        av_opt_set_int(avr, "out_sample_rate",    info.sample_rate,    0);
        av_opt_set_int(avr,  "in_channels",       channels_in_frame,    0);
        av_opt_set_int(avr, "out_channels",       info.channels,    0);
        SWR_INIT(avr);
    }

    // Convert audio samples
    nb_samples = SWR_CONVERT(avr, 	// audio resample context
            audio_converted->data, 			// output data pointers
            audio_converted->linesize[0], 	// output plane size, in bytes. (0 if unknown)
            audio_converted->nb_samples,	// maximum number of samples that the output buffer can hold
            audio_frame->data,				// input data pointers
            audio_frame->linesize[0],		// input plane size, in bytes (0 if unknown)
            audio_frame->nb_samples);		// number of input samples to convert

	// Create a new array (to hold all resampled S16 audio samples)
	int16_t* resampled_samples = new int16_t[(nb_samples * info.channels)];

	// Copy audio samples over original samples
	memcpy(resampled_samples, audio_converted->data[0], (nb_samples * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * info.channels));

	// Free frames
	av_freep(&audio_frame->data[0]);
	AV_FREE_FRAME(&audio_frame);
	av_freep(&audio_converted->data[0]);
	AV_FREE_FRAME(&audio_converted);
	frame_samples = NULL;

	// Resize the frame to hold the right # of channels and samples
	int channel_buffer_size = nb_samples;
	frame->ResizeAudio(info.channels, channel_buffer_size, info.sample_rate, info.channel_layout);

	ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::ResampleMappedAudio (Audio successfully resampled)", "nb_samples", nb_samples, "total_frame_samples", total_frame_samples, "info.sample_rate", info.sample_rate, "channels_in_frame", channels_in_frame, "info.channels", info.channels, "info.channel_layout", info.channel_layout);

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
		frame->AddAudio(true, channel_filter, 0, channel_buffer, position, 1.0f);

		ZmqLogger::Instance()->AppendDebugMethod("FrameMapper::ResampleMappedAudio (Add audio to channel)", "number of samples", position, "channel_filter", channel_filter);
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
