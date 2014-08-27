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

FrameMapper::FrameMapper(ReaderBase *reader, Fraction target, PulldownType pulldown) :
		reader(reader), target(target), pulldown(pulldown), final_cache(820 * 1024)
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
	info.sample_rate = reader->info.sample_rate;
	info.channels = reader->info.channels;
	info.width = reader->info.width;
	info.height = reader->info.height;

	// Used to toggle odd / even fields
	field_toggle = true;

	// init mapping between original and target frames
	Init();
}

void FrameMapper::AddField(int frame)
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
	// Clear the fields & frames lists
	fields.clear();
	frames.clear();

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
		int frame = 1;
		int number_of_fields = reader->info.video_length * 2;

		// Loop through all fields in the original video file
		for (int field = 1; field <= number_of_fields; field++)
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
		int new_length = reader->info.video_length * rate_diff;

		// Build curve for framerate mapping
		Keyframe rate_curve;
		rate_curve.AddPoint(1, 1, LINEAR);
		rate_curve.AddPoint(new_length, reader->info.video_length, LINEAR);

		// Loop through curve, and build list of frames
		for (int frame_num = 1; frame_num <= new_length; frame_num++)
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

	for (unsigned int field = 1; field <= fields.size(); field++)
	{
		// Get the current field
		Field f = fields[field - 1];

		// Is field divisible by 2?
		if (field % 2 == 0 && field > 0)
		{
			// New frame number
			int frame_number = field / 2;

			// Set the bottom frame
			if (f.isOdd)
				Odd = f;
			else
				Even = f;

			// Determine the range of samples (from the original rate, to the new rate)
			int end_samples_frame = start_samples_frame;
			int end_samples_position = start_samples_position;
			int remaining_samples = Frame::GetSamplesPerFrame(frame_number, target, info.sample_rate);

			while (remaining_samples > 0)
			{
				// get original samples
				int original_samples = Frame::GetSamplesPerFrame(end_samples_frame, original, info.sample_rate) - end_samples_position;

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
			SampleRange Samples = {start_samples_frame, start_samples_position, end_samples_frame, end_samples_position, Frame::GetSamplesPerFrame(frame_number, target, info.sample_rate)};

			// Reset the audio variables
			start_samples_frame = end_samples_frame;
			start_samples_position = end_samples_position + 1;
			if (start_samples_position >= Frame::GetSamplesPerFrame(start_samples_frame, original, info.sample_rate))
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

MappedFrame FrameMapper::GetMappedFrame(int TargetFrameNumber) throw(OutOfBoundsFrame)
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
tr1::shared_ptr<Frame> FrameMapper::GetFrame(int requested_frame) throw(ReaderClosed)
{
	// Check final cache, and just return the frame (if it's available)
	if (final_cache.Exists(requested_frame))
		return final_cache.GetFrame(requested_frame);

	// Get the mapped frame
	MappedFrame mapped = GetMappedFrame(requested_frame);

	// Init some basic properties about this frame
	int samples_in_frame = Frame::GetSamplesPerFrame(requested_frame, target, info.sample_rate);

	// Create a new frame
	tr1::shared_ptr<Frame> frame(new Frame(requested_frame, 1, 1, "#000000", samples_in_frame, info.channels));

	// Copy the image from the odd field (TODO: make this copy each field from the correct frames)
	frame->AddImage(reader->GetFrame(mapped.Odd.Frame)->GetImage(), true);
	if (mapped.Odd.Frame != mapped.Even.Frame)
		// Add even lines (if different than the previous image)
		frame->AddImage(reader->GetFrame(mapped.Even.Frame)->GetImage(), false);

	// Copy the samples
	int samples_copied = 0;
	int starting_frame = mapped.Samples.frame_start;
	while (samples_copied < mapped.Samples.total)
	{
		// init number of samples to copy this iteration
		int remaining_samples = mapped.Samples.total - samples_copied;
		int number_to_copy = 0;

		// Loop through each channel
		for (int channel = 0; channel < info.channels; channel++)
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
				frame->AddAudio(true, channel, samples_copied, original_frame->GetAudioSamples(channel) + mapped.Samples.sample_start, number_to_copy, 1.0);
			}
			else if (starting_frame > mapped.Samples.frame_start && starting_frame < mapped.Samples.frame_end)
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
				number_to_copy = mapped.Samples.sample_end;
				if (number_to_copy > remaining_samples)
					number_to_copy = remaining_samples;

				// Add samples to new frame
				frame->AddAudio(true, channel, samples_copied, original_frame->GetAudioSamples(channel), number_to_copy, 1.0);
			}
		}

		// increment frame
		samples_copied += number_to_copy;
		starting_frame++;
	}

	// Add frame to final cache
	final_cache.Add(frame->number, frame);

	// Return processed 'frame'
	return final_cache.GetFrame(frame->number);
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

	// print header
	cout << "Convert " << original.ToInt() << " fps to " << target.ToInt() << " fps" << endl;
	cout << "Difference of " << difference << " frames per second" << endl;
	cout << "Field Interval: " << field_interval << "th field" << endl;
	cout << "Frame Interval: " << frame_interval << "th field" << endl << endl;

	// Loop through frame mappings
	for (float map = 1; map <= frames.size(); map++)
	{
		MappedFrame frame = frames[map - 1];
		//cout << "Target frame #: " << map << " mapped to original frame #:\t(" << frame.Odd.Frame << " odd, " << frame.Even.Frame << " even)" << endl;
		//cout << "  - Audio samples mapped to frame " << frame.Samples.frame_start << ":" << frame.Samples.sample_start << " to frame " << frame.Samples.frame_end << ":" << frame.Samples.sample_end << endl;
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
		// Open the reader
		reader->Open();
	}
}

// Close the internal reader
void FrameMapper::Close()
{
	if (reader)
		reader->Close();
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
