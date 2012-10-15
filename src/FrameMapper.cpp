/**
 * \file
 * \brief Source code for the FrameMapper class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "../include/FrameMapper.h"

using namespace std;
using namespace openshot;

FrameMapper::FrameMapper(FileReaderBase *reader, Framerate target, Pulldown_Method pulldown) :
		reader(reader), target(target), pulldown(pulldown)
{
	// Set the original frame rate from the reader
	original = Framerate(reader->info.fps.num, reader->info.fps.den);

	// Set length from reader
	m_length = reader->info.video_length;

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
	// Get the difference (in frames) between the original and target frame rates
	float difference = target.GetRoundedFPS() - original.GetRoundedFPS();

	// Find the number (i.e. interval) of fields that need to be skipped or repeated
	int field_interval = 0;
	int frame_interval = 0;

	if (difference != 0)
	{
		field_interval = round(fabs(original.GetRoundedFPS() / difference));

		// Get frame interval (2 fields per frame)
		frame_interval = field_interval * 2.0f;
	}

	// Clear the fields & frames lists
	fields.clear();
	frames.clear();

	// Loop through all fields in the original video file
	int frame = 1;
	int number_of_fields = m_length * 2;
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

				if (frame + 1 <= m_length)
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
			int remaining_samples = GetSamplesPerFrame(frame_number, target.GetFraction());

			while (remaining_samples > 0)
			{
				// get original samples
				int original_samples = GetSamplesPerFrame(end_samples_frame, original.GetFraction()) - end_samples_position;

				// Enough samples
				if (original_samples >= remaining_samples)
				{
					// Take all that we need, and break loop
					end_samples_position = remaining_samples;
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
			SampleRange Samples = {start_samples_frame, start_samples_position, end_samples_frame, end_samples_position};

			// Reset the audio variables
			start_samples_frame = end_samples_frame;
			start_samples_position = end_samples_position + 1;
			if (start_samples_position >= GetSamplesPerFrame(start_samples_frame, original.GetFraction()))
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

MappedFrame FrameMapper::GetFrame(int TargetFrameNumber) throw(OutOfBoundsFrame)
{
	// Check if frame number is valid
	if(TargetFrameNumber < 1 || TargetFrameNumber > frames.size())
	{
		throw OutOfBoundsFrame("An invalid frame was requested.", TargetFrameNumber, frames.size());
	}

	// Return frame
	return frames[TargetFrameNumber - 1];
}

void FrameMapper::MapTime(Keyframe new_time) throw(OutOfBoundsFrame)
{
	// New time-mapped frames vector
	vector<MappedFrame> time_mapped_frames;

	// Loop through each coordinate of the Keyframe
	for (int keyframe_number = 1; keyframe_number < new_time.Values.size(); keyframe_number++) {
		// Get the current coordinate from the Keyframe
		Coordinate c = new_time.Values[keyframe_number];

		// Get the requested time-mapped frame number... and make it zero based, to match
		// the existing frame mapper vector.
		int requested_frame = round(c.Y) - 1;

		// Check if frame number is valid
		if(requested_frame < 0 || requested_frame >= frames.size())
		{
			throw OutOfBoundsFrame("An invalid frame was requested.", requested_frame + 1, frames.size());
		}

		// Add the Keyframe requested frame to the new "time-mapped" frames vector
		time_mapped_frames.push_back(frames[requested_frame]);
	}

	// Now that we have a new vector of frames that match the Keyframe, we need
	// to replace the internal frames vector with this new one.
	frames.clear();
	for (int new_frame = 0; new_frame < time_mapped_frames.size(); new_frame++)
	{
		// Add new frames to this frame mapper instance
		frames.push_back(time_mapped_frames[new_frame]);
	}

}

// Calculate the # of samples per video frame (for a specific frame number)
int FrameMapper::GetSamplesPerFrame(int frame_number, Fraction rate)
{
	// Get the total # of samples for the previous frame, and the current frame (rounded)
	double fps = rate.Reciprocal().ToDouble();
	double previous_samples = round((reader->info.sample_rate * fps) * (frame_number - 1));
	double total_samples = round((reader->info.sample_rate * fps) * frame_number);

	// Subtract the previous frame's total samples with this frame's total samples.  Not all sample rates can
	// be evenly divided into frames, so each frame can have have different # of samples.
	double samples_per_frame = total_samples - previous_samples;
	return samples_per_frame;
}

void FrameMapper::PrintMapping()
{
	// Get the difference (in frames) between the original and target frame rates
	float difference = target.GetRoundedFPS() - original.GetRoundedFPS();

	int field_interval = 0;
	int frame_interval = 0;

	if (difference != 0)
	{
		// Find the number (i.e. interval) of fields that need to be skipped or repeated
		field_interval = round(fabs(original.GetRoundedFPS() / difference));

		// Get frame interval (2 fields per frame)
		frame_interval = field_interval * 2.0f;
	}

	// print header
	cout << "Convert " << original.GetRoundedFPS() << " fps to " << target.GetRoundedFPS() << " fps" << endl;
	cout << "Difference of " << difference << " frames per second" << endl;
	cout << "Field Interval: " << field_interval << "th field" << endl;
	cout << "Frame Interval: " << frame_interval << "th field" << endl << endl;

	// Loop through frame mappings
	for (float map = 1; map <= frames.size(); map++)
	{
		MappedFrame frame = frames[map - 1];
		cout << "Target frame #: " << map << " mapped to original frame #:\t(" << frame.Odd.Frame << " odd, " << frame.Even.Frame << " even)" << endl;
		cout << "  - Audio samples mapped to frame " << frame.Samples.frame_start << ":" << frame.Samples.sample_start << " to frame " << frame.Samples.frame_end << ":" << frame.Samples.sample_end << endl;
	}

}
