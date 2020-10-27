/**
 * @file
 * @brief Source file for Clip class
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

#include "Clip.h"
#include "FFmpegReader.h"
#include "FrameMapper.h"
#ifdef USE_IMAGEMAGICK
	#include "ImageReader.h"
	#include "TextReader.h"
#endif
#include "QtImageReader.h"
#include "ChunkReader.h"
#include "DummyReader.h"
#include "Timeline.h"

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
	mixing = VOLUME_MIX_NONE;
	waveform = false;
	previous_properties = "";

	// Init scale curves
	scale_x = Keyframe(1.0);
	scale_y = Keyframe(1.0);

	// Init location curves
	location_x = Keyframe(0.0);
	location_y = Keyframe(0.0);

	// Init alpha
	alpha = Keyframe(1.0);

	// Init time & volume
	time = Keyframe(1.0);
	volume = Keyframe(1.0);

	// Init audio waveform color
	wave_color = Color((unsigned char)0, (unsigned char)123, (unsigned char)255, (unsigned char)255);

	// Init shear and perspective curves
	shear_x = Keyframe(0.0);
	shear_y = Keyframe(0.0);
	origin_x = Keyframe(0.5);
	origin_y = Keyframe(0.5);
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

	// Init reader info struct and cache size
	init_reader_settings();
}

// Init reader info details
void Clip::init_reader_settings() {
	if (reader) {
		// Init rotation (if any)
		init_reader_rotation();

		// Initialize info struct
		info = reader->info;

		// Initialize Clip cache
		cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);
	}
}

// Init reader's rotation (if any)
void Clip::init_reader_rotation() {
	// Only init rotation from reader when needed
	if (rotation.GetCount() > 1)
		// Do nothing if more than 1 rotation Point
		return;
	else if (rotation.GetCount() == 1 && rotation.GetValue(1) != 0.0)
		// Do nothing if 1 Point, and it's not the default value
		return;

	// Init rotation
	if (reader && reader->info.metadata.count("rotate") > 0) {
		// Use reader metadata rotation (if any)
		// This is typical with cell phone videos filmed in different orientations
		try {
			float rotate_metadata = strtof(reader->info.metadata["rotate"].c_str(), 0);
			rotation = Keyframe(rotate_metadata);
		} catch (const std::exception& e) {}
	}
	else
		// Default no rotation
		rotation = Keyframe(0.0);
}

// Default Constructor for a clip
Clip::Clip() : resampler(NULL), reader(NULL), allocated_reader(NULL), is_open(false)
{
	// Init all default settings
	init_settings();
}

// Constructor with reader
Clip::Clip(ReaderBase* new_reader) : resampler(NULL), reader(new_reader), allocated_reader(NULL), is_open(false)
{
	// Init all default settings
	init_settings();

	// Open and Close the reader (to set the duration of the clip)
	Open();
	Close();

	// Update duration and set parent
	if (reader) {
		End(reader->info.duration);
		reader->ParentClip(this);
		// Init reader info struct and cache size
		init_reader_settings();
	}
}

// Constructor with filepath
Clip::Clip(std::string path) : resampler(NULL), reader(NULL), allocated_reader(NULL), is_open(false)
{
	// Init all default settings
	init_settings();

	// Get file extension (and convert to lower case)
	std::string ext = get_file_extension(path);
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	// Determine if common video formats
	if (ext=="avi" || ext=="mov" || ext=="mkv" ||  ext=="mpg" || ext=="mpeg" || ext=="mp3" || ext=="mp4" || ext=="mts" ||
		ext=="ogg" || ext=="wav" || ext=="wmv" || ext=="webm" || ext=="vob")
	{
		try
		{
			// Open common video format
			reader = new openshot::FFmpegReader(path);

		} catch(...) { }
	}
	if (ext=="osp")
	{
		try
		{
			// Open common video format
			reader = new openshot::Timeline(path, true);

		} catch(...) { }
	}


	// If no video found, try each reader
	if (!reader)
	{
		try
		{
			// Try an image reader
			reader = new openshot::QtImageReader(path);

		} catch(...) {
			try
			{
				// Try a video reader
				reader = new openshot::FFmpegReader(path);

			} catch(...) { }
		}
	}

	// Update duration and set parent
	if (reader) {
		End(reader->info.duration);
		reader->ParentClip(this);
		allocated_reader = reader;
		// Init reader info struct and cache size
		init_reader_settings();
	}
}

// Destructor
Clip::~Clip()
{
	// Delete the reader if clip created it
	if (allocated_reader) {
		delete allocated_reader;
		allocated_reader = NULL;
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

	// set parent
	reader->ParentClip(this);

	// Init reader info struct and cache size
	init_reader_settings();
}

/// Get the current reader
ReaderBase* Clip::Reader()
{
	if (reader)
		return reader;
	else
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.");
}

// Open the internal reader
void Clip::Open()
{
	if (reader)
	{
		// Open the reader
		reader->Open();
		is_open = true;

		// Copy Reader info to Clip
		info = reader->info;

		// Set some clip properties from the file reader
		if (end == 0.0)
			End(reader->info.duration);
	}
	else
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.");
}

// Close the internal reader
void Clip::Close()
{
	is_open = false;
	if (reader) {
		ZmqLogger::Instance()->AppendDebugMethod("Clip::Close");

		// Close the reader
		reader->Close();
	}
	else
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.");
}

// Get end position of clip (trim end of video), which can be affected by the time curve.
float Clip::End() const
{
	// if a time curve is present, use its length
	if (time.GetCount() > 1)
	{
		// Determine the FPS fo this clip
		float fps = 24.0;
		if (reader)
			// file reader
			fps = reader->info.fps.ToFloat();
		else
			// Throw error if reader not initialized
			throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.");

		return float(time.GetLength()) / fps;
	}
	else
		// just use the duration (as detected by the reader)
		return end;
}

// Create an openshot::Frame object for a specific frame number of this reader.
std::shared_ptr<Frame> Clip::GetFrame(int64_t frame_number)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The Clip is closed.  Call Open() before calling this method.");

	if (reader)
	{
		// Adjust out of bounds frame number
		frame_number = adjust_frame_number_minimum(frame_number);

		// Get the original frame and pass it to GetFrame overload
		std::shared_ptr<Frame> original_frame = GetOrCreateFrame(frame_number);
		return GetFrame(original_frame, frame_number);
	}
	else
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.");
}

// Use an existing openshot::Frame object and draw this Clip's frame onto it
std::shared_ptr<Frame> Clip::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The Clip is closed.  Call Open() before calling this method.");

	if (reader)
	{
		// Adjust out of bounds frame number
		frame_number = adjust_frame_number_minimum(frame_number);

		// Check the cache for this frame
		std::shared_ptr<Frame> cached_frame = cache.GetFrame(frame_number);
		if (cached_frame) {
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("Clip::GetFrame", "returned cached frame", frame_number);

			// Return the cached frame
			return cached_frame;
		}

		// Adjust has_video and has_audio overrides
		int enabled_audio = has_audio.GetInt(frame_number);
		if (enabled_audio == -1 && reader && reader->info.has_audio)
			enabled_audio = 1;
		else if (enabled_audio == -1 && reader && !reader->info.has_audio)
			enabled_audio = 0;
		int enabled_video = has_video.GetInt(frame_number);
		if (enabled_video == -1 && reader && reader->info.has_video)
			enabled_video = 1;
		else if (enabled_video == -1 && reader && !reader->info.has_audio)
			enabled_video = 0;

		// Is a time map detected
		int64_t new_frame_number = frame_number;
		int64_t time_mapped_number = adjust_frame_number_minimum(time.GetLong(frame_number));
		if (time.GetLength() > 1)
			new_frame_number = time_mapped_number;

		// Now that we have re-mapped what frame number is needed, go and get the frame pointer
		std::shared_ptr<Frame> original_frame;
		original_frame = GetOrCreateFrame(new_frame_number);

		// Copy the image from the odd field
		if (enabled_video)
			frame->AddImage(std::make_shared<QImage>(*original_frame->GetImage()));

		// Loop through each channel, add audio
		if (enabled_audio && reader->info.has_audio)
			for (int channel = 0; channel < original_frame->GetAudioChannelsCount(); channel++)
				frame->AddAudio(true, channel, 0, original_frame->GetAudioSamples(channel), original_frame->GetAudioSamplesCount(), 1.0);

		// Get time mapped frame number (used to increase speed, change direction, etc...)
		// TODO: Handle variable # of samples, since this resamples audio for different speeds (only when time curve is set)
		get_time_mapped_frame(frame, new_frame_number);

		// Adjust # of samples to match requested (the interaction with time curves will make this tricky)
		// TODO: Implement move samples to/from next frame

		// Apply effects to the frame (if any)
		apply_effects(frame);

		// Determine size of image (from Timeline or Reader)
		int width = 0;
		int height = 0;
		if (timeline) {
			// Use timeline size (if available)
			width = timeline->preview_width;
			height = timeline->preview_height;
		} else {
			// Fallback to clip size
			width = reader->info.width;
			height = reader->info.height;
		}

		// Apply keyframe / transforms
		apply_keyframes(frame, width, height);

		// Cache frame
		cache.Add(frame);

		// Return processed 'frame'
		return frame;
	}
	else
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.");
}

// Look up an effect by ID
openshot::EffectBase* Clip::GetEffect(const std::string& id)
{
	// Find the matching effect (if any)
	for (const auto& effect : effects) {
		if (effect->Id() == id) {
			return effect;
		}
	}
	return nullptr;
}

// Get file extension
std::string Clip::get_file_extension(std::string path)
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
	juce::AudioSampleBuffer *reversed = new juce::AudioSampleBuffer(channels, number_of_samples);
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
void Clip::get_time_mapped_frame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
	// Check for valid reader
	if (!reader)
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.");

	// Check for a valid time map curve
	if (time.GetLength() > 1)
	{
		const GenericScopedLock<juce::CriticalSection> lock(getFrameCriticalSection);

		// create buffer and resampler
		juce::AudioSampleBuffer *samples = NULL;
		if (!resampler)
			resampler = new AudioResampler();

		// Get new frame number
		int new_frame_number = frame->number;

		// Get delta (difference in previous Y value)
		int delta = int(round(time.GetDelta(frame_number)));

		// Init audio vars
		int channels = reader->info.channels;
		int number_of_samples = GetOrCreateFrame(new_frame_number)->GetAudioSamplesCount();

		// Only resample audio if needed
		if (reader->info.has_audio) {
			// Determine if we are speeding up or slowing down
			if (time.GetRepeatFraction(frame_number).den > 1) {
				// SLOWING DOWN AUDIO
				// Resample data, and return new buffer pointer
				juce::AudioSampleBuffer *resampled_buffer = NULL;

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

				// Just take the samples we need for the requested frame
				int start = (number_of_samples * (time.GetRepeatFraction(frame_number).num - 1));
				if (start > 0)
					start -= 1;
				for (int channel = 0; channel < channels; channel++)
					// Add new (slower) samples, to the frame object
					frame->AddAudio(true, channel, 0, resampled_buffer->getReadPointer(channel, start),
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
						juce::AudioSampleBuffer *delta_samples = new juce::AudioSampleBuffer(channels,
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
						juce::AudioSampleBuffer *delta_samples = new juce::AudioSampleBuffer(channels,
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
				juce::AudioSampleBuffer *buffer = resampler->GetResampledBuffer();

				// Add the newly resized audio samples to the current frame
				for (int channel = 0; channel < channels; channel++)
					// Add new (slower) samples, to the frame object
					frame->AddAudio(true, channel, 0, buffer->getReadPointer(channel), number_of_samples, 1.0f);

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
					frame->AddAudio(true, channel, 0, samples->getReadPointer(channel), number_of_samples, 1.0f);


			}

			delete samples;
			samples = NULL;
		}
	}
}

// Adjust frame number minimum value
int64_t Clip::adjust_frame_number_minimum(int64_t frame_number)
{
	// Never return a frame number 0 or below
	if (frame_number < 1)
		return 1;
	else
		return frame_number;

}

// Get or generate a blank frame
std::shared_ptr<Frame> Clip::GetOrCreateFrame(int64_t number)
{
	try {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Clip::GetOrCreateFrame (from reader)", "number", number);

		// Attempt to get a frame (but this could fail if a reader has just been closed)
		auto reader_frame = reader->GetFrame(number);

		// Return real frame
		if (reader_frame) {
			// Create a new copy of reader frame
			// This allows a clip to modify the pixels and audio of this frame without
			// changing the underlying reader's frame data
			//std::shared_ptr<Frame> reader_copy(new Frame(number, 1, 1, "#000000", reader_frame->GetAudioSamplesCount(), reader_frame->GetAudioChannelsCount()));
			auto reader_copy = std::make_shared<Frame>(*reader_frame.get());
			reader_copy->SampleRate(reader_frame->SampleRate());
			reader_copy->ChannelsLayout(reader_frame->ChannelsLayout());
			return reader_copy;
		}

	} catch (const ReaderClosed & e) {
		// ...
	} catch (const TooManySeeks & e) {
		// ...
	} catch (const OutOfBoundsFrame & e) {
		// ...
	}

	// Estimate # of samples needed for this frame
	int estimated_samples_in_frame = Frame::GetSamplesPerFrame(number, reader->info.fps, reader->info.sample_rate, reader->info.channels);

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Clip::GetOrCreateFrame (create blank)", "number", number, "estimated_samples_in_frame", estimated_samples_in_frame);

	// Create blank frame
	auto new_frame = std::make_shared<Frame>(
		number, reader->info.width, reader->info.height,
		"#000000", estimated_samples_in_frame, reader->info.channels);
	new_frame->SampleRate(reader->info.sample_rate);
	new_frame->ChannelsLayout(reader->info.channel_layout);
	new_frame->AddAudioSilence(estimated_samples_in_frame);
	return new_frame;
}

// Generate JSON string of this object
std::string Clip::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Get all properties for a specific frame
std::string Clip::PropertiesJSON(int64_t requested_frame) const {

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
	root["display"] = add_property_json("Frame Number", display, "int", "", NULL, 0, 3, false, requested_frame);
	root["mixing"] = add_property_json("Volume Mixing", mixing, "int", "", NULL, 0, 2, false, requested_frame);
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

	// Add frame number display choices (dropdown style)
	root["display"]["choices"].append(add_property_choice_json("None", FRAME_DISPLAY_NONE, display));
	root["display"]["choices"].append(add_property_choice_json("Clip", FRAME_DISPLAY_CLIP, display));
	root["display"]["choices"].append(add_property_choice_json("Timeline", FRAME_DISPLAY_TIMELINE, display));
	root["display"]["choices"].append(add_property_choice_json("Both", FRAME_DISPLAY_BOTH, display));

	// Add volume mixing choices (dropdown style)
	root["mixing"]["choices"].append(add_property_choice_json("None", VOLUME_MIX_NONE, mixing));
	root["mixing"]["choices"].append(add_property_choice_json("Average", VOLUME_MIX_AVERAGE, mixing));
	root["mixing"]["choices"].append(add_property_choice_json("Reduce", VOLUME_MIX_REDUCE, mixing));

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
	root["origin_x"] = add_property_json("Origin X", origin_x.GetValue(requested_frame), "float", "", &origin_x, 0.0, 1.0, false, requested_frame);
	root["origin_y"] = add_property_json("Origin Y", origin_y.GetValue(requested_frame), "float", "", &origin_y, 0.0, 1.0, false, requested_frame);
	root["volume"] = add_property_json("Volume", volume.GetValue(requested_frame), "float", "", &volume, 0.0, 1.0, false, requested_frame);
	root["time"] = add_property_json("Time", time.GetValue(requested_frame), "float", "", &time, 0.0, 30 * 60 * 60 * 48, false, requested_frame);
	root["channel_filter"] = add_property_json("Channel Filter", channel_filter.GetValue(requested_frame), "int", "", &channel_filter, -1, 10, false, requested_frame);
	root["channel_mapping"] = add_property_json("Channel Mapping", channel_mapping.GetValue(requested_frame), "int", "", &channel_mapping, -1, 10, false, requested_frame);
	root["has_audio"] = add_property_json("Enable Audio", has_audio.GetValue(requested_frame), "int", "", &has_audio, -1, 1.0, false, requested_frame);
	root["has_video"] = add_property_json("Enable Video", has_video.GetValue(requested_frame), "int", "", &has_video, -1, 1.0, false, requested_frame);

	// Add enable audio/video choices (dropdown style)
	root["has_audio"]["choices"].append(add_property_choice_json("Auto", -1, has_audio.GetValue(requested_frame)));
	root["has_audio"]["choices"].append(add_property_choice_json("Off", 0, has_audio.GetValue(requested_frame)));
	root["has_audio"]["choices"].append(add_property_choice_json("On", 1, has_audio.GetValue(requested_frame)));
	root["has_video"]["choices"].append(add_property_choice_json("Auto", -1, has_video.GetValue(requested_frame)));
	root["has_video"]["choices"].append(add_property_choice_json("Off", 0, has_video.GetValue(requested_frame)));
	root["has_video"]["choices"].append(add_property_choice_json("On", 1, has_video.GetValue(requested_frame)));

	root["wave_color"] = add_property_json("Wave Color", 0.0, "color", "", &wave_color.red, 0, 255, false, requested_frame);
	root["wave_color"]["red"] = add_property_json("Red", wave_color.red.GetValue(requested_frame), "float", "", &wave_color.red, 0, 255, false, requested_frame);
	root["wave_color"]["blue"] = add_property_json("Blue", wave_color.blue.GetValue(requested_frame), "float", "", &wave_color.blue, 0, 255, false, requested_frame);
	root["wave_color"]["green"] = add_property_json("Green", wave_color.green.GetValue(requested_frame), "float", "", &wave_color.green, 0, 255, false, requested_frame);


	// Return formatted string
	return root.toStyledString();
}

// Generate Json::Value for this object
Json::Value Clip::JsonValue() const {

	// Create root json object
	Json::Value root = ClipBase::JsonValue(); // get parent properties
	root["gravity"] = gravity;
	root["scale"] = scale;
	root["anchor"] = anchor;
	root["display"] = display;
	root["mixing"] = mixing;
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
	root["shear_x"] = shear_x.JsonValue();
	root["shear_y"] = shear_y.JsonValue();
	root["origin_x"] = origin_x.JsonValue();
	root["origin_y"] = origin_y.JsonValue();
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
	for (auto existing_effect : effects)
	{
		root["effects"].append(existing_effect->JsonValue());
	}

	if (reader)
		root["reader"] = reader->JsonValue();
	else
		root["reader"] = Json::Value(Json::objectValue);

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Clip::SetJson(const std::string value) {

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
void Clip::SetJsonValue(const Json::Value root) {

	// Set parent data
	ClipBase::SetJsonValue(root);

	// Clear cache
	cache.Clear();

	// Set data from Json (if key is found)
	if (!root["gravity"].isNull())
		gravity = (GravityType) root["gravity"].asInt();
	if (!root["scale"].isNull())
		scale = (ScaleType) root["scale"].asInt();
	if (!root["anchor"].isNull())
		anchor = (AnchorType) root["anchor"].asInt();
	if (!root["display"].isNull())
		display = (FrameDisplayType) root["display"].asInt();
	if (!root["mixing"].isNull())
		mixing = (VolumeMixType) root["mixing"].asInt();
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
	if (!root["shear_x"].isNull())
		shear_x.SetJsonValue(root["shear_x"]);
	if (!root["shear_y"].isNull())
		shear_y.SetJsonValue(root["shear_y"]);
	if (!root["origin_x"].isNull())
		origin_x.SetJsonValue(root["origin_x"]);
	if (!root["origin_y"].isNull())
		origin_y.SetJsonValue(root["origin_y"]);
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
		for (const auto existing_effect : root["effects"]) {
			// Create Effect
			EffectBase *e = NULL;

			if (!existing_effect["type"].isNull()) {
				// Create instance of effect
				if ( (e = EffectInfo().CreateEffect(existing_effect["type"].asString())) ) {

					// Load Json into Effect
					e->SetJsonValue(existing_effect);

					// Add Effect to Timeline
					AddEffect(e);
				}
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
			std::string type = root["reader"]["type"].asString();

			if (type == "FFmpegReader") {

				// Create new reader
				reader = new openshot::FFmpegReader(root["reader"]["path"].asString(), false);
				reader->SetJsonValue(root["reader"]);

			} else if (type == "QtImageReader") {

				// Create new reader
				reader = new openshot::QtImageReader(root["reader"]["path"].asString(), false);
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
				reader = new openshot::ChunkReader(root["reader"]["path"].asString(), (ChunkVersion) root["reader"]["chunk_version"].asInt());
				reader->SetJsonValue(root["reader"]);

			} else if (type == "DummyReader") {

				// Create new reader
				reader = new openshot::DummyReader();
				reader->SetJsonValue(root["reader"]);

			} else if (type == "Timeline") {

				// Create new reader (always load from file again)
				// This prevents FrameMappers from being loaded on accident
				reader = new openshot::Timeline(root["reader"]["path"].asString(), true);
			}

			// mark as managed reader and set parent
			if (reader) {
				reader->ParentClip(this);
				allocated_reader = reader;
			}

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
	// Set parent clip pointer
	effect->ParentClip(this);

	// Add effect to list
	effects.push_back(effect);

	// Sort effects
	sort_effects();

	// Clear cache
	cache.Clear();
}

// Remove an effect from the clip
void Clip::RemoveEffect(EffectBase* effect)
{
	effects.remove(effect);

	// Clear cache
	cache.Clear();
}

// Apply effects to the source frame (if any)
void Clip::apply_effects(std::shared_ptr<Frame> frame)
{
	// Find Effects at this position and layer
	for (auto effect : effects)
	{
		// Apply the effect to this frame
		frame = effect->GetFrame(frame, frame->number);

	} // end effect loop
}

// Compare 2 floating point numbers for equality
bool Clip::isEqual(double a, double b)
{
	return fabs(a - b) < 0.000001;
}


// Apply keyframes to the source frame (if any)
void Clip::apply_keyframes(std::shared_ptr<Frame> frame, int width, int height)
{
	// Get actual frame image data
	std::shared_ptr<QImage> source_image = frame->GetImage();

	/* REPLACE IMAGE WITH WAVEFORM IMAGE (IF NEEDED) */
	if (Waveform())
	{
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Clip::apply_keyframes (Generate Waveform Image)", "frame->number", frame->number, "Waveform()", Waveform());

		// Get the color of the waveform
		int red = wave_color.red.GetInt(frame->number);
		int green = wave_color.green.GetInt(frame->number);
		int blue = wave_color.blue.GetInt(frame->number);
		int alpha = wave_color.alpha.GetInt(frame->number);

		// Generate Waveform Dynamically (the size of the timeline)
		source_image = frame->GetWaveform(width, height, red, green, blue, alpha);
		frame->AddImage(std::shared_ptr<QImage>(source_image));
	}

	/* ALPHA & OPACITY */
	if (alpha.GetValue(frame->number) != 1.0)
	{
		float alpha_value = alpha.GetValue(frame->number);

		// Get source image's pixels
		unsigned char *pixels = source_image->bits();

		// Loop through pixels
		for (int pixel = 0, byte_index=0; pixel < source_image->width() * source_image->height(); pixel++, byte_index+=4)
		{
			// Apply alpha to pixel values (since we use a premultiplied value, we must
			// multiply the alpha with all colors).
			pixels[byte_index + 0] *= alpha_value;
			pixels[byte_index + 1] *= alpha_value;
			pixels[byte_index + 2] *= alpha_value;
			pixels[byte_index + 3] *= alpha_value;
		}

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Clip::apply_keyframes (Set Alpha & Opacity)", "alpha_value", alpha_value, "frame->number", frame->number);
	}

	/* RESIZE SOURCE IMAGE - based on scale type */
	QSize source_size = source_image->size();
	switch (scale)
	{
		case (SCALE_FIT): {
			source_size.scale(width, height, Qt::KeepAspectRatio);

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("Clip::apply_keyframes (Scale: SCALE_FIT)", "frame->number", frame->number, "source_width", source_size.width(), "source_height", source_size.height());
			break;
		}
		case (SCALE_STRETCH): {
			source_size.scale(width, height, Qt::IgnoreAspectRatio);

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("Clip::apply_keyframes (Scale: SCALE_STRETCH)", "frame->number", frame->number, "source_width", source_size.width(), "source_height", source_size.height());
			break;
		}
		case (SCALE_CROP): {
			source_size.scale(width, height, Qt::KeepAspectRatioByExpanding);

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("Clip::apply_keyframes (Scale: SCALE_CROP)", "frame->number", frame->number, "source_width", source_size.width(), "source_height", source_size.height());
			break;
		}
		case (SCALE_NONE): {
			// Calculate ratio of source size to project size
			// Even with no scaling, previews need to be adjusted correctly
			// (otherwise NONE scaling draws the frame image outside of the preview)
			float source_width_ratio = source_size.width() / float(width);
			float source_height_ratio = source_size.height() / float(height);
			source_size.scale(width * source_width_ratio, height * source_height_ratio, Qt::KeepAspectRatio);

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("Clip::apply_keyframes (Scale: SCALE_NONE)", "frame->number", frame->number, "source_width", source_size.width(), "source_height", source_size.height());
			break;
		}
	}

	/* GRAVITY LOCATION - Initialize X & Y to the correct values (before applying location curves) */
	float x = 0.0; // left
	float y = 0.0; // top

	// Adjust size for scale x and scale y
	float sx = scale_x.GetValue(frame->number); // percentage X scale
	float sy = scale_y.GetValue(frame->number); // percentage Y scale
	float scaled_source_width = source_size.width() * sx;
	float scaled_source_height = source_size.height() * sy;

	switch (gravity)
	{
		case (GRAVITY_TOP_LEFT):
			// This is only here to prevent unused-enum warnings
			break;
		case (GRAVITY_TOP):
			x = (width - scaled_source_width) / 2.0; // center
			break;
		case (GRAVITY_TOP_RIGHT):
			x = width - scaled_source_width; // right
			break;
		case (GRAVITY_LEFT):
			y = (height - scaled_source_height) / 2.0; // center
			break;
		case (GRAVITY_CENTER):
			x = (width - scaled_source_width) / 2.0; // center
			y = (height - scaled_source_height) / 2.0; // center
			break;
		case (GRAVITY_RIGHT):
			x = width - scaled_source_width; // right
			y = (height - scaled_source_height) / 2.0; // center
			break;
		case (GRAVITY_BOTTOM_LEFT):
			y = (height - scaled_source_height); // bottom
			break;
		case (GRAVITY_BOTTOM):
			x = (width - scaled_source_width) / 2.0; // center
			y = (height - scaled_source_height); // bottom
			break;
		case (GRAVITY_BOTTOM_RIGHT):
			x = width - scaled_source_width; // right
			y = (height - scaled_source_height); // bottom
			break;
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Clip::apply_keyframes (Gravity)", "frame->number", frame->number, "source_clip->gravity", gravity, "scaled_source_width", scaled_source_width, "scaled_source_height", scaled_source_height);

	/* LOCATION, ROTATION, AND SCALE */
	float r = rotation.GetValue(frame->number); // rotate in degrees
	x += (width * location_x.GetValue(frame->number)); // move in percentage of final width
	y += (height * location_y.GetValue(frame->number)); // move in percentage of final height
	float shear_x_value = shear_x.GetValue(frame->number);
	float shear_y_value = shear_y.GetValue(frame->number);
	float origin_x_value = origin_x.GetValue(frame->number);
	float origin_y_value = origin_y.GetValue(frame->number);

	QTransform transform;

	// Transform source image (if needed)
	ZmqLogger::Instance()->AppendDebugMethod("Clip::apply_keyframes (Build QTransform - if needed)", "frame->number", frame->number, "x", x, "y", y, "r", r, "sx", sx, "sy", sy);

	if (!isEqual(x, 0) || !isEqual(y, 0)) {
		// TRANSLATE/MOVE CLIP
		transform.translate(x, y);
	}

	if (!isEqual(r, 0) || !isEqual(shear_x_value, 0) || !isEqual(shear_y_value, 0)) {
		// ROTATE CLIP (around origin_x, origin_y)
		float origin_x_offset = (scaled_source_width * origin_x_value);
		float origin_y_offset = (scaled_source_height * origin_y_value);
		transform.translate(origin_x_offset, origin_y_offset);
		transform.rotate(r);
		transform.shear(shear_x_value, shear_y_value);
		transform.translate(-origin_x_offset,-origin_y_offset);
	}

	// SCALE CLIP (if needed)
	float source_width_scale = (float(source_size.width()) / float(source_image->width())) * sx;
	float source_height_scale = (float(source_size.height()) / float(source_image->height())) * sy;

	if (!isEqual(source_width_scale, 1.0) || !isEqual(source_height_scale, 1.0)) {
		transform.scale(source_width_scale, source_height_scale);
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Clip::apply_keyframes (Transform: Composite Image Layer: Prepare)", "frame->number", frame->number);

	/* COMPOSITE SOURCE IMAGE (LAYER) ONTO FINAL IMAGE */
	auto new_image = std::make_shared<QImage>(QSize(width, height), source_image->format());
	new_image->fill(QColor(QString::fromStdString("#00000000")));

	// Load timeline's new frame image into a QPainter
	QPainter painter(new_image.get());
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing, true);

	// Apply transform (translate, rotate, scale)
	painter.setTransform(transform);

	// Composite a new layer onto the image
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.drawImage(0, 0, *source_image);

	if (timeline) {
		Timeline *t = (Timeline *) timeline;

		// Draw frame #'s on top of image (if needed)
		if (display != FRAME_DISPLAY_NONE) {
			std::stringstream frame_number_str;
			switch (display) {
				case (FRAME_DISPLAY_NONE):
					// This is only here to prevent unused-enum warnings
					break;

				case (FRAME_DISPLAY_CLIP):
					frame_number_str << frame->number;
					break;

				case (FRAME_DISPLAY_TIMELINE):
					frame_number_str << (position * t->info.fps.ToFloat()) + frame->number;
					break;

				case (FRAME_DISPLAY_BOTH):
					frame_number_str << (position * t->info.fps.ToFloat()) + frame->number << " (" << frame->number << ")";
					break;
			}

			// Draw frame number on top of image
			painter.setPen(QColor("#ffffff"));
			painter.drawText(20, 20, QString(frame_number_str.str().c_str()));
		}
	}

	painter.end();

	// Add new QImage to frame
	frame->AddImage(new_image);
}
