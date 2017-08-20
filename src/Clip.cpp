/**
 * @file
 * @brief Source file for Clip class
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

#include "../include/Clip.h"

using namespace openshot;

// Init default settings for a clip
void Clip::init_settings()
{
	// Init clip settings
	Position(0.0);
	Layer(0);
	Start(0.0);
	End(0.0);
	gravity = GRAVITY_CENTER;
	scale = SCALE_FIT;
	anchor = ANCHOR_CANVAS;
	display = FRAME_DISPLAY_NONE;
	waveform = false;
	previous_properties = "";

	// Init scale curves
	scale_x = Keyframe(1.0);
	scale_y = Keyframe(1.0);

	// Init location curves
	location_x = Keyframe(0.0);
	location_y = Keyframe(0.0);

	// Init alpha & rotation
	alpha = Keyframe(1.0);
	rotation = Keyframe(0.0);

	// Init time & volume
	time = Keyframe(1.0);
	volume = Keyframe(1.0);

	// Init audio waveform color
	wave_color = Color((unsigned char)0, (unsigned char)123, (unsigned char)255, (unsigned char)255);

	// Init crop settings
	crop_gravity = GRAVITY_CENTER;
	crop_width = Keyframe(-1.0);
	crop_height = Keyframe(-1.0);
	crop_x = Keyframe(0.0);
	crop_y = Keyframe(0.0);

	// Init shear and perspective curves
	shear_x = Keyframe(0.0);
	shear_y = Keyframe(0.0);
	perspective_c1_x = Keyframe(-1.0);
	perspective_c1_y = Keyframe(-1.0);
	perspective_c2_x = Keyframe(-1.0);
	perspective_c2_y = Keyframe(-1.0);
	perspective_c3_x = Keyframe(-1.0);
	perspective_c3_y = Keyframe(-1.0);
	perspective_c4_x = Keyframe(-1.0);
	perspective_c4_y = Keyframe(-1.0);

	// Init audio channel filter and mappings
	channel_filter = Keyframe(-1.0);
	channel_mapping = Keyframe(-1.0);

	// Init audio and video overrides
	has_audio = Keyframe(-1.0);
	has_video = Keyframe(-1.0);

	// Default pointers
	reader = NULL;
	resampler = NULL;
	audio_cache = NULL;
	manage_reader = false;
}

// Default Constructor for a clip
Clip::Clip()
{
	// Init all default settings
	init_settings();
}

// Constructor with reader
Clip::Clip(ReaderBase* new_reader)
{
	// Init all default settings
	init_settings();

	// Set the reader
	reader = new_reader;

	// Open and Close the reader (to set the duration of the clip)
	Open();
	Close();

	// Update duration
	End(reader->info.duration);
}

// Constructor with filepath
Clip::Clip(string path)
{
	// Init all default settings
	init_settings();

	// Get file extension (and convert to lower case)
	string ext = get_file_extension(path);
	transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	// Determine if common video formats
	if (ext=="avi" || ext=="mov" || ext=="mkv" ||  ext=="mpg" || ext=="mpeg" || ext=="mp3" || ext=="mp4" || ext=="mts" ||
		ext=="ogg" || ext=="wav" || ext=="wmv" || ext=="webm" || ext=="vob")
	{
		try
		{
			// Open common video format
			reader = new FFmpegReader(path);

		} catch(...) { }
	}

	// If no video found, try each reader
	if (!reader)
	{
		try
		{
			// Try an image reader
			reader = new QtImageReader(path);

		} catch(...) {
			try
			{
				// Try a video reader
				reader = new FFmpegReader(path);

			} catch(...) { }
		}
	}

	// Update duration
	if (reader) {
		End(reader->info.duration);
		manage_reader = true;
	}
}

// Destructor
Clip::~Clip()
{
	// Delete the reader if clip created it
	if (manage_reader && reader) {
		delete reader;
		reader = NULL;
	}

	// Close the resampler
	if (resampler) {
		delete resampler;
		resampler = NULL;
	}
}

/// Set the current reader
void Clip::Reader(ReaderBase* new_reader)
{
	// set reader pointer
	reader = new_reader;
}

/// Get the current reader
ReaderBase* Clip::Reader() throw(ReaderClosed)
{
	if (reader)
		return reader;
	else
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.", "");
}

// Open the internal reader
void Clip::Open() throw(InvalidFile, ReaderClosed)
{
	if (reader)
	{
		// Open the reader
		reader->Open();

		// Set some clip properties from the file reader
		if (end == 0.0)
			End(reader->info.duration);
	}
	else
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.", "");
}

// Close the internal reader
void Clip::Close() throw(ReaderClosed)
{
	if (reader) {
		ZmqLogger::Instance()->AppendDebugMethod("Clip::Close", "", -1, "", -1, "", -1, "", -1, "", -1, "", -1);

		// Close the reader
		reader->Close();
	}
	else
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.", "");
}

// Get end position of clip (trim end of video), which can be affected by the time curve.
float Clip::End() throw(ReaderClosed)
{
	// if a time curve is present, use it's length
	if (time.Points.size() > 1)
	{
		// Determine the FPS fo this clip
		float fps = 24.0;
		if (reader)
			// file reader
			fps = reader->info.fps.ToFloat();
		else
			// Throw error if reader not initialized
			throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.", "");

		return float(time.GetLength()) / fps;
	}
	else
		// just use the duration (as detected by the reader)
		return end;
}

// Get an openshot::Frame object for a specific frame number of this reader.
std::shared_ptr<Frame> Clip::GetFrame(long int requested_frame) throw(ReaderClosed)
{
	if (reader)
	{
		// Adjust out of bounds frame number
		requested_frame = adjust_frame_number_minimum(requested_frame);

		// Adjust has_video and has_audio overrides
		int enabled_audio = has_audio.GetInt(requested_frame);
		if (enabled_audio == -1 && reader && reader->info.has_audio)
			enabled_audio = 1;
		else if (enabled_audio == -1 && reader && !reader->info.has_audio)
			enabled_audio = 0;
		int enabled_video = has_video.GetInt(requested_frame);
		if (enabled_video == -1 && reader && reader->info.has_video)
			enabled_video = 1;
		else if (enabled_video == -1 && reader && !reader->info.has_audio)
			enabled_video = 0;

		// Is a time map detected
		long int new_frame_number = requested_frame;
		long int time_mapped_number = adjust_frame_number_minimum(time.GetLong(requested_frame));
		if (time.Values.size() > 1)
            new_frame_number = time_mapped_number;

		// Now that we have re-mapped what frame number is needed, go and get the frame pointer
		std::shared_ptr<Frame> original_frame = GetOrCreateFrame(new_frame_number);

		// Create a new frame
		std::shared_ptr<Frame> frame(new Frame(new_frame_number, 1, 1, "#000000", original_frame->GetAudioSamplesCount(), original_frame->GetAudioChannelsCount()));
		frame->SampleRate(original_frame->SampleRate());
		frame->ChannelsLayout(original_frame->ChannelsLayout());

		// Copy the image from the odd field
		if (enabled_video)
			frame->AddImage(std::shared_ptr<QImage>(new QImage(*original_frame->GetImage())));

		// Loop through each channel, add audio
		if (enabled_audio && reader->info.has_audio)
			for (int channel = 0; channel < original_frame->GetAudioChannelsCount(); channel++)
				frame->AddAudio(true, channel, 0, original_frame->GetAudioSamples(channel), original_frame->GetAudioSamplesCount(), 1.0);

		// Get time mapped frame number (used to increase speed, change direction, etc...)
		std::shared_ptr<Frame> new_frame = get_time_mapped_frame(frame, requested_frame);

		// Apply effects to the frame (if any)
		apply_effects(new_frame);

		// Return processed 'frame'
		return new_frame;
	}
	else
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.", "");
}

// Get file extension
string Clip::get_file_extension(string path)
{
	// return last part of path
	return path.substr(path.find_last_of(".") + 1);
}

// Reverse an audio buffer
void Clip::reverse_buffer(juce::AudioSampleBuffer* buffer)
{
	int number_of_samples = buffer->getNumSamples();
	int channels = buffer->getNumChannels();

	// Reverse array (create new buffer to hold the reversed version)
	AudioSampleBuffer *reversed = new juce::AudioSampleBuffer(channels, number_of_samples);
	reversed->clear();

	for (int channel = 0; channel < channels; channel++)
	{
		int n=0;
		for (int s = number_of_samples - 1; s >= 0; s--, n++)
			reversed->getWritePointer(channel)[n] = buffer->getWritePointer(channel)[s];
	}

	// Copy the samples back to the original array
	buffer->clear();
	// Loop through channels, and get audio samples
	for (int channel = 0; channel < channels; channel++)
		// Get the audio samples for this channel
		buffer->addFrom(channel, 0, reversed->getReadPointer(channel), number_of_samples, 1.0f);

	delete reversed;
	reversed = NULL;
}

// Adjust the audio and image of a time mapped frame
std::shared_ptr<Frame> Clip::get_time_mapped_frame(std::shared_ptr<Frame> frame, long int frame_number) throw(ReaderClosed)
{
	// Check for valid reader
	if (!reader)
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.", "");

	// Check for a valid time map curve
	if (time.Values.size() > 1)
	{
		const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);
		std::shared_ptr<Frame> new_frame;

		// create buffer and resampler
		juce::AudioSampleBuffer *samples = NULL;
		if (!resampler)
			resampler = new AudioResampler();

		// Get new frame number
		int new_frame_number = adjust_frame_number_minimum(round(time.GetValue(frame_number)));

		// Create a new frame
		int samples_in_frame = Frame::GetSamplesPerFrame(new_frame_number, reader->info.fps, reader->info.sample_rate, frame->GetAudioChannelsCount());
		new_frame = std::make_shared<Frame>(new_frame_number, 1, 1, "#000000", samples_in_frame, frame->GetAudioChannelsCount());

		// Copy the image from the new frame
		new_frame->AddImage(GetOrCreateFrame(new_frame_number)->GetImage());


		// Get delta (difference in previous Y value)
		int delta = int(round(time.GetDelta(frame_number)));

		// Init audio vars
		int sample_rate = reader->info.sample_rate;
		int channels = reader->info.channels;
		int number_of_samples = GetOrCreateFrame(new_frame_number)->GetAudioSamplesCount();

		// Only resample audio if needed
		if (reader->info.has_audio) {
			// Determine if we are speeding up or slowing down
			if (time.GetRepeatFraction(frame_number).den > 1) {
				// SLOWING DOWN AUDIO
				// Resample data, and return new buffer pointer
				AudioSampleBuffer *resampled_buffer = NULL;
				int resampled_buffer_size = 0;

				// SLOW DOWN audio (split audio)
				samples = new juce::AudioSampleBuffer(channels, number_of_samples);
				samples->clear();

				// Loop through channels, and get audio samples
				for (int channel = 0; channel < channels; channel++)
					// Get the audio samples for this channel
					samples->addFrom(channel, 0, GetOrCreateFrame(new_frame_number)->GetAudioSamples(channel),
									 number_of_samples, 1.0f);

				// Reverse the samples (if needed)
				if (!time.IsIncreasing(frame_number))
					reverse_buffer(samples);

				// Resample audio to be X times slower (where X is the denominator of the repeat fraction)
				resampler->SetBuffer(samples, 1.0 / time.GetRepeatFraction(frame_number).den);

				// Resample the data (since it's the 1st slice)
				resampled_buffer = resampler->GetResampledBuffer();

				// Get the length of the resampled buffer (if one exists)
				resampled_buffer_size = resampled_buffer->getNumSamples();

				// Just take the samples we need for the requested frame
				int start = (number_of_samples * (time.GetRepeatFraction(frame_number).num - 1));
				if (start > 0)
					start -= 1;
				for (int channel = 0; channel < channels; channel++)
					// Add new (slower) samples, to the frame object
					new_frame->AddAudio(true, channel, 0, resampled_buffer->getReadPointer(channel, start),
										number_of_samples, 1.0f);

				// Clean up
				resampled_buffer = NULL;

			}
			else if (abs(delta) > 1 && abs(delta) < 100) {
				int start = 0;
				if (delta > 0) {
					// SPEED UP (multiple frames of audio), as long as it's not more than X frames
					int total_delta_samples = 0;
					for (int delta_frame = new_frame_number - (delta - 1);
						 delta_frame <= new_frame_number; delta_frame++)
						total_delta_samples += Frame::GetSamplesPerFrame(delta_frame, reader->info.fps,
																		 reader->info.sample_rate,
																		 reader->info.channels);

					// Allocate a new sample buffer for these delta frames
					samples = new juce::AudioSampleBuffer(channels, total_delta_samples);
					samples->clear();

					// Loop through each frame in this delta
					for (int delta_frame = new_frame_number - (delta - 1);
						 delta_frame <= new_frame_number; delta_frame++) {
						// buffer to hold detal samples
						int number_of_delta_samples = GetOrCreateFrame(delta_frame)->GetAudioSamplesCount();
						AudioSampleBuffer *delta_samples = new juce::AudioSampleBuffer(channels,
																					   number_of_delta_samples);
						delta_samples->clear();

						for (int channel = 0; channel < channels; channel++)
							delta_samples->addFrom(channel, 0, GetOrCreateFrame(delta_frame)->GetAudioSamples(channel),
												   number_of_delta_samples, 1.0f);

						// Reverse the samples (if needed)
						if (!time.IsIncreasing(frame_number))
							reverse_buffer(delta_samples);

						// Copy the samples to
						for (int channel = 0; channel < channels; channel++)
							// Get the audio samples for this channel
							samples->addFrom(channel, start, delta_samples->getReadPointer(channel),
											 number_of_delta_samples, 1.0f);

						// Clean up
						delete delta_samples;
						delta_samples = NULL;

						// Increment start position
						start += number_of_delta_samples;
					}
				}
				else {
					// SPEED UP (multiple frames of audio), as long as it's not more than X frames
					int total_delta_samples = 0;
					for (int delta_frame = new_frame_number - (delta + 1);
						 delta_frame >= new_frame_number; delta_frame--)
						total_delta_samples += Frame::GetSamplesPerFrame(delta_frame, reader->info.fps,
																		 reader->info.sample_rate,
																		 reader->info.channels);

					// Allocate a new sample buffer for these delta frames
					samples = new juce::AudioSampleBuffer(channels, total_delta_samples);
					samples->clear();

					// Loop through each frame in this delta
					for (int delta_frame = new_frame_number - (delta + 1);
						 delta_frame >= new_frame_number; delta_frame--) {
						// buffer to hold delta samples
						int number_of_delta_samples = GetOrCreateFrame(delta_frame)->GetAudioSamplesCount();
						AudioSampleBuffer *delta_samples = new juce::AudioSampleBuffer(channels,
																					   number_of_delta_samples);
						delta_samples->clear();

						for (int channel = 0; channel < channels; channel++)
							delta_samples->addFrom(channel, 0, GetOrCreateFrame(delta_frame)->GetAudioSamples(channel),
												   number_of_delta_samples, 1.0f);

						// Reverse the samples (if needed)
						if (!time.IsIncreasing(frame_number))
							reverse_buffer(delta_samples);

						// Copy the samples to
						for (int channel = 0; channel < channels; channel++)
							// Get the audio samples for this channel
							samples->addFrom(channel, start, delta_samples->getReadPointer(channel),
											 number_of_delta_samples, 1.0f);

						// Clean up
						delete delta_samples;
						delta_samples = NULL;

						// Increment start position
						start += number_of_delta_samples;
					}
				}

				// Resample audio to be X times faster (where X is the delta of the repeat fraction)
				resampler->SetBuffer(samples, float(start) / float(number_of_samples));

				// Resample data, and return new buffer pointer
				AudioSampleBuffer *buffer = resampler->GetResampledBuffer();
				int resampled_buffer_size = buffer->getNumSamples();

				// Add the newly resized audio samples to the current frame
				for (int channel = 0; channel < channels; channel++)
					// Add new (slower) samples, to the frame object
					new_frame->AddAudio(true, channel, 0, buffer->getReadPointer(channel), number_of_samples, 1.0f);

				// Clean up
				buffer = NULL;
			}
			else {
				// Use the samples on this frame (but maybe reverse them if needed)
				samples = new juce::AudioSampleBuffer(channels, number_of_samples);
				samples->clear();

				// Loop through channels, and get audio samples
				for (int channel = 0; channel < channels; channel++)
					// Get the audio samples for this channel
					samples->addFrom(channel, 0, frame->GetAudioSamples(channel), number_of_samples, 1.0f);

				// reverse the samples
				if (!time.IsIncreasing(frame_number))
					reverse_buffer(samples);

				// Add reversed samples to the frame object
				for (int channel = 0; channel < channels; channel++)
					new_frame->AddAudio(true, channel, 0, samples->getReadPointer(channel), number_of_samples, 1.0f);


			}

			delete samples;
			samples = NULL;
		}

		// Return new time mapped frame
		return new_frame;

	} else
		// Use original frame
		return frame;
}

// Adjust frame number minimum value
long int Clip::adjust_frame_number_minimum(long int frame_number)
{
	// Never return a frame number 0 or below
	if (frame_number < 1)
		return 1;
	else
		return frame_number;

}

// Get or generate a blank frame
std::shared_ptr<Frame> Clip::GetOrCreateFrame(long int number)
{
	std::shared_ptr<Frame> new_frame;

	// Init some basic properties about this frame
	int samples_in_frame = Frame::GetSamplesPerFrame(number, reader->info.fps, reader->info.sample_rate, reader->info.channels);

	try {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Clip::GetOrCreateFrame (from reader)", "number", number, "samples_in_frame", samples_in_frame, "", -1, "", -1, "", -1, "", -1);

		// Determine the max size of this clips source image (based on the timeline's size, the scaling mode,
		// and the scaling keyframes). This is a performance improvement, to keep the images as small as possible,
		// without loosing quality. NOTE: We cannot go smaller than the timeline itself, or the add_layer timeline
        // method will scale it back to timeline size before scaling it smaller again. This needs to be fixed in
        // the future.
		if (scale == SCALE_FIT || scale == SCALE_STRETCH) {
			// Best fit or Stretch scaling (based on max timeline size * scaling keyframes)
			float max_scale_x = scale_x.GetMaxPoint().co.Y;
			float max_scale_y = scale_y.GetMaxPoint().co.Y;
			reader->SetMaxSize(max(float(max_width), max_width * max_scale_x), max(float(max_height), max_height * max_scale_y));

		} else if (scale == SCALE_CROP) {
			// Cropping scale mode (based on max timeline size * cropped size * scaling keyframes)
			float max_scale_x = scale_x.GetMaxPoint().co.Y;
			float max_scale_y = scale_y.GetMaxPoint().co.Y;
			QSize width_size(max_width * max_scale_x, round(max_width / (float(reader->info.width) / float(reader->info.height))));
			QSize height_size(round(max_height / (float(reader->info.height) / float(reader->info.width))), max_height * max_scale_y);

			// respect aspect ratio
			if (width_size.width() >= max_width && width_size.height() >= max_height)
				reader->SetMaxSize(max(max_width, width_size.width()), max(max_height, width_size.height()));
			else
				reader->SetMaxSize(max(max_width, height_size.width()), max(max_height, height_size.height()));

		} else {
			// No scaling, use original image size (slower)
			reader->SetMaxSize(0, 0);
		}

		// Attempt to get a frame (but this could fail if a reader has just been closed)
		new_frame = reader->GetFrame(number);

		// Return real frame
		if (new_frame)
			return new_frame;

	} catch (const ReaderClosed & e) {
		// ...
	} catch (const TooManySeeks & e) {
		// ...
	} catch (const OutOfBoundsFrame & e) {
		// ...
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Clip::GetOrCreateFrame (create blank)", "number", number, "samples_in_frame", samples_in_frame, "", -1, "", -1, "", -1, "", -1);

	// Create blank frame
	new_frame = std::make_shared<Frame>(number, reader->info.width, reader->info.height, "#000000", samples_in_frame, reader->info.channels);
	new_frame->SampleRate(reader->info.sample_rate);
	new_frame->ChannelsLayout(reader->info.channel_layout);
	return new_frame;
}

// Generate JSON string of this object
string Clip::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Get all properties for a specific frame
string Clip::PropertiesJSON(long int requested_frame) {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 30 * 60 * 60 * 48, true, requested_frame);
	root["gravity"] = add_property_json("Gravity", gravity, "int", "", NULL, 0, 8, false, requested_frame);
	root["scale"] = add_property_json("Scale", scale, "int", "", NULL, 0, 3, false, requested_frame);
	root["anchor"] = add_property_json("Anchor", anchor, "int", "", NULL, 0, 1, false, requested_frame);
	root["display"] = add_property_json("Frame Number", display, "int", "", NULL, 0, 3, false, requested_frame);
	root["waveform"] = add_property_json("Waveform", waveform, "int", "", NULL, 0, 1, false, requested_frame);

	// Add gravity choices (dropdown style)
	root["gravity"]["choices"].append(add_property_choice_json("Top Left", GRAVITY_TOP_LEFT, gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Top Center", GRAVITY_TOP, gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Top Right", GRAVITY_TOP_RIGHT, gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Left", GRAVITY_LEFT, gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Center", GRAVITY_CENTER, gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Right", GRAVITY_RIGHT, gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Bottom Left", GRAVITY_BOTTOM_LEFT, gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Bottom Center", GRAVITY_BOTTOM, gravity));
	root["gravity"]["choices"].append(add_property_choice_json("Bottom Right", GRAVITY_BOTTOM_RIGHT, gravity));

	// Add scale choices (dropdown style)
	root["scale"]["choices"].append(add_property_choice_json("Crop", SCALE_CROP, scale));
	root["scale"]["choices"].append(add_property_choice_json("Best Fit", SCALE_FIT, scale));
	root["scale"]["choices"].append(add_property_choice_json("Stretch", SCALE_STRETCH, scale));
	root["scale"]["choices"].append(add_property_choice_json("None", SCALE_NONE, scale));

	// Add anchor choices (dropdown style)
	root["anchor"]["choices"].append(add_property_choice_json("Canvas", ANCHOR_CANVAS, anchor));
	root["anchor"]["choices"].append(add_property_choice_json("Viewport", ANCHOR_VIEWPORT, anchor));

	// Add frame number display choices (dropdown style)
	root["display"]["choices"].append(add_property_choice_json("None", FRAME_DISPLAY_NONE, display));
	root["display"]["choices"].append(add_property_choice_json("Clip", FRAME_DISPLAY_CLIP, display));
	root["display"]["choices"].append(add_property_choice_json("Timeline", FRAME_DISPLAY_TIMELINE, display));
	root["display"]["choices"].append(add_property_choice_json("Both", FRAME_DISPLAY_BOTH, display));

	// Add waveform choices (dropdown style)
	root["waveform"]["choices"].append(add_property_choice_json("Yes", true, waveform));
	root["waveform"]["choices"].append(add_property_choice_json("No", false, waveform));

	// Keyframes
	root["location_x"] = add_property_json("Location X", location_x.GetValue(requested_frame), "float", "", &location_x, -1.0, 1.0, false, requested_frame);
	root["location_y"] = add_property_json("Location Y", location_y.GetValue(requested_frame), "float", "", &location_y, -1.0, 1.0, false, requested_frame);
	root["scale_x"] = add_property_json("Scale X", scale_x.GetValue(requested_frame), "float", "", &scale_x, 0.0, 1.0, false, requested_frame);
	root["scale_y"] = add_property_json("Scale Y", scale_y.GetValue(requested_frame), "float", "", &scale_y, 0.0, 1.0, false, requested_frame);
	root["alpha"] = add_property_json("Alpha", alpha.GetValue(requested_frame), "float", "", &alpha, 0.0, 1.0, false, requested_frame);
	root["shear_x"] = add_property_json("Shear X", shear_x.GetValue(requested_frame), "float", "", &shear_x, -1.0, 1.0, false, requested_frame);
	root["shear_y"] = add_property_json("Shear Y", shear_y.GetValue(requested_frame), "float", "", &shear_y, -1.0, 1.0, false, requested_frame);
	root["rotation"] = add_property_json("Rotation", rotation.GetValue(requested_frame), "float", "", &rotation, -360, 360, false, requested_frame);
	root["volume"] = add_property_json("Volume", volume.GetValue(requested_frame), "float", "", &volume, 0.0, 1.0, false, requested_frame);
	root["time"] = add_property_json("Time", time.GetValue(requested_frame), "float", "", &time, 0.0, 30 * 60 * 60 * 48, false, requested_frame);
	root["channel_filter"] = add_property_json("Channel Filter", channel_filter.GetValue(requested_frame), "int", "", &channel_filter, -1, 10, false, requested_frame);
	root["channel_mapping"] = add_property_json("Channel Mapping", channel_mapping.GetValue(requested_frame), "int", "", &channel_mapping, -1, 10, false, requested_frame);
	root["has_audio"] = add_property_json("Enable Audio", has_audio.GetValue(requested_frame), "int", "", &has_audio, -1, 1.0, false, requested_frame);
	root["has_video"] = add_property_json("Enable Video", has_video.GetValue(requested_frame), "int", "", &has_video, -1, 1.0, false, requested_frame);

	root["wave_color"] = add_property_json("Wave Color", 0.0, "color", "", &wave_color.red, 0, 255, false, requested_frame);
	root["wave_color"]["red"] = add_property_json("Red", wave_color.red.GetValue(requested_frame), "float", "", &wave_color.red, 0, 255, false, requested_frame);
	root["wave_color"]["blue"] = add_property_json("Blue", wave_color.blue.GetValue(requested_frame), "float", "", &wave_color.blue, 0, 255, false, requested_frame);
	root["wave_color"]["green"] = add_property_json("Green", wave_color.green.GetValue(requested_frame), "float", "", &wave_color.green, 0, 255, false, requested_frame);


	// Return formatted string
	return root.toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Clip::JsonValue() {

	// Create root json object
	Json::Value root = ClipBase::JsonValue(); // get parent properties
	root["gravity"] = gravity;
	root["scale"] = scale;
	root["anchor"] = anchor;
	root["display"] = display;
	root["waveform"] = waveform;
	root["scale_x"] = scale_x.JsonValue();
	root["scale_y"] = scale_y.JsonValue();
	root["location_x"] = location_x.JsonValue();
	root["location_y"] = location_y.JsonValue();
	root["alpha"] = alpha.JsonValue();
	root["rotation"] = rotation.JsonValue();
	root["time"] = time.JsonValue();
	root["volume"] = volume.JsonValue();
	root["wave_color"] = wave_color.JsonValue();
	root["crop_width"] = crop_width.JsonValue();
	root["crop_height"] = crop_height.JsonValue();
	root["crop_x"] = crop_x.JsonValue();
	root["crop_y"] = crop_y.JsonValue();
	root["shear_x"] = shear_x.JsonValue();
	root["shear_y"] = shear_y.JsonValue();
	root["channel_filter"] = channel_filter.JsonValue();
	root["channel_mapping"] = channel_mapping.JsonValue();
	root["has_audio"] = has_audio.JsonValue();
	root["has_video"] = has_video.JsonValue();
	root["perspective_c1_x"] = perspective_c1_x.JsonValue();
	root["perspective_c1_y"] = perspective_c1_y.JsonValue();
	root["perspective_c2_x"] = perspective_c2_x.JsonValue();
	root["perspective_c2_y"] = perspective_c2_y.JsonValue();
	root["perspective_c3_x"] = perspective_c3_x.JsonValue();
	root["perspective_c3_y"] = perspective_c3_y.JsonValue();
	root["perspective_c4_x"] = perspective_c4_x.JsonValue();
	root["perspective_c4_y"] = perspective_c4_y.JsonValue();

	// Add array of effects
	root["effects"] = Json::Value(Json::arrayValue);

	// loop through effects
	list<EffectBase*>::iterator effect_itr;
	for (effect_itr=effects.begin(); effect_itr != effects.end(); ++effect_itr)
	{
		// Get clip object from the iterator
		EffectBase *existing_effect = (*effect_itr);
		root["effects"].append(existing_effect->JsonValue());
	}

	if (reader)
		root["reader"] = reader->JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Clip::SetJson(string value) throw(InvalidJSON) {

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
void Clip::SetJsonValue(Json::Value root) {

	// Set parent data
	ClipBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["gravity"].isNull())
		gravity = (GravityType) root["gravity"].asInt();
	if (!root["scale"].isNull())
		scale = (ScaleType) root["scale"].asInt();
	if (!root["anchor"].isNull())
		anchor = (AnchorType) root["anchor"].asInt();
	if (!root["display"].isNull())
		display = (FrameDisplayType) root["display"].asInt();
	if (!root["waveform"].isNull())
		waveform = root["waveform"].asBool();
	if (!root["scale_x"].isNull())
		scale_x.SetJsonValue(root["scale_x"]);
	if (!root["scale_y"].isNull())
		scale_y.SetJsonValue(root["scale_y"]);
	if (!root["location_x"].isNull())
		location_x.SetJsonValue(root["location_x"]);
	if (!root["location_y"].isNull())
		location_y.SetJsonValue(root["location_y"]);
	if (!root["alpha"].isNull())
		alpha.SetJsonValue(root["alpha"]);
	if (!root["rotation"].isNull())
		rotation.SetJsonValue(root["rotation"]);
	if (!root["time"].isNull())
		time.SetJsonValue(root["time"]);
	if (!root["volume"].isNull())
		volume.SetJsonValue(root["volume"]);
	if (!root["wave_color"].isNull())
		wave_color.SetJsonValue(root["wave_color"]);
	if (!root["crop_width"].isNull())
		crop_width.SetJsonValue(root["crop_width"]);
	if (!root["crop_height"].isNull())
		crop_height.SetJsonValue(root["crop_height"]);
	if (!root["crop_x"].isNull())
		crop_x.SetJsonValue(root["crop_x"]);
	if (!root["crop_y"].isNull())
		crop_y.SetJsonValue(root["crop_y"]);
	if (!root["shear_x"].isNull())
		shear_x.SetJsonValue(root["shear_x"]);
	if (!root["shear_y"].isNull())
		shear_y.SetJsonValue(root["shear_y"]);
	if (!root["channel_filter"].isNull())
		channel_filter.SetJsonValue(root["channel_filter"]);
	if (!root["channel_mapping"].isNull())
		channel_mapping.SetJsonValue(root["channel_mapping"]);
	if (!root["has_audio"].isNull())
		has_audio.SetJsonValue(root["has_audio"]);
	if (!root["has_video"].isNull())
		has_video.SetJsonValue(root["has_video"]);
	if (!root["perspective_c1_x"].isNull())
		perspective_c1_x.SetJsonValue(root["perspective_c1_x"]);
	if (!root["perspective_c1_y"].isNull())
		perspective_c1_y.SetJsonValue(root["perspective_c1_y"]);
	if (!root["perspective_c2_x"].isNull())
		perspective_c2_x.SetJsonValue(root["perspective_c2_x"]);
	if (!root["perspective_c2_y"].isNull())
		perspective_c2_y.SetJsonValue(root["perspective_c2_y"]);
	if (!root["perspective_c3_x"].isNull())
		perspective_c3_x.SetJsonValue(root["perspective_c3_x"]);
	if (!root["perspective_c3_y"].isNull())
		perspective_c3_y.SetJsonValue(root["perspective_c3_y"]);
	if (!root["perspective_c4_x"].isNull())
		perspective_c4_x.SetJsonValue(root["perspective_c4_x"]);
	if (!root["perspective_c4_y"].isNull())
		perspective_c4_y.SetJsonValue(root["perspective_c4_y"]);
	if (!root["effects"].isNull()) {

		// Clear existing effects
		effects.clear();

		// loop through effects
		for (int x = 0; x < root["effects"].size(); x++) {
			// Get each effect
			Json::Value existing_effect = root["effects"][x];

			// Create Effect
			EffectBase *e = NULL;

			if (!existing_effect["type"].isNull()) {
				// Create instance of effect
				e = EffectInfo().CreateEffect(existing_effect["type"].asString());

				// Load Json into Effect
				e->SetJsonValue(existing_effect);

				// Add Effect to Timeline
				AddEffect(e);
			}
		}
	}
	if (!root["reader"].isNull()) // does Json contain a reader?
	{
		if (!root["reader"]["type"].isNull()) // does the reader Json contain a 'type'?
		{
			// Close previous reader (if any)
			bool already_open = false;
			if (reader)
			{
				// Track if reader was open
				already_open = reader->IsOpen();

				// Close and delete existing reader (if any)
				reader->Close();
				delete reader;
				reader = NULL;
			}

			// Create new reader (and load properties)
			string type = root["reader"]["type"].asString();

			if (type == "FFmpegReader") {

				// Create new reader
				reader = new FFmpegReader(root["reader"]["path"].asString(), false);
				reader->SetJsonValue(root["reader"]);

			} else if (type == "QtImageReader") {

				// Create new reader
				reader = new QtImageReader(root["reader"]["path"].asString(), false);
				reader->SetJsonValue(root["reader"]);

#ifdef USE_IMAGEMAGICK
			} else if (type == "ImageReader") {

				// Create new reader
				reader = new ImageReader(root["reader"]["path"].asString(), false);
				reader->SetJsonValue(root["reader"]);

			} else if (type == "TextReader") {

				// Create new reader
				reader = new TextReader();
				reader->SetJsonValue(root["reader"]);
#endif

			} else if (type == "ChunkReader") {

				// Create new reader
				reader = new ChunkReader(root["reader"]["path"].asString(), (ChunkVersion) root["reader"]["chunk_version"].asInt());
				reader->SetJsonValue(root["reader"]);

			} else if (type == "DummyReader") {

				// Create new reader
				reader = new DummyReader();
				reader->SetJsonValue(root["reader"]);
			}

			// mark as managed reader
			if (reader)
				manage_reader = true;

			// Re-Open reader (if needed)
			if (already_open)
				reader->Open();

		}
	}
}

// Sort effects by order
void Clip::sort_effects()
{
	// sort clips
	effects.sort(CompareClipEffects());
}

// Add an effect to the clip
void Clip::AddEffect(EffectBase* effect)
{
	// Add effect to list
	effects.push_back(effect);

	// Sort effects
	sort_effects();
}

// Remove an effect from the clip
void Clip::RemoveEffect(EffectBase* effect)
{
	effects.remove(effect);
}

// Apply effects to the source frame (if any)
std::shared_ptr<Frame> Clip::apply_effects(std::shared_ptr<Frame> frame)
{
	// Find Effects at this position and layer
	list<EffectBase*>::iterator effect_itr;
	for (effect_itr=effects.begin(); effect_itr != effects.end(); ++effect_itr)
	{
		// Get clip object from the iterator
		EffectBase *effect = (*effect_itr);

		// Apply the effect to this frame
		frame = effect->GetFrame(frame, frame->number);

	} // end effect loop

	// Return modified frame
	return frame;
}
