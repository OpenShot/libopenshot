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
	waveform = false;
	previous_properties = "";

	// Init scale curves
	scale_x = Keyframe(1.0);
	scale_y = Keyframe(1.0);

	// Init location curves
	location_x = Keyframe(0.0);
	location_y = Keyframe(0.0);

	// Init alpha & rotation
	alpha = Keyframe(0.0);
	rotation = Keyframe(0.0);

	// Init time & volume
	time = Keyframe(0.0);
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
tr1::shared_ptr<Frame> Clip::GetFrame(long int requested_frame) throw(ReaderClosed)
{
	if (reader)
	{
		// Adjust out of bounds frame number
		requested_frame = adjust_frame_number_minimum(requested_frame);

		// Is a time map detected
		long int new_frame_number = requested_frame;
		if (time.Values.size() > 1)
			new_frame_number = time.GetLong(requested_frame);


		// Now that we have re-mapped what frame number is needed, go and get the frame pointer
		tr1::shared_ptr<Frame> original_frame = reader->GetFrame(new_frame_number);

		// Create a new frame
		tr1::shared_ptr<Frame> frame(new Frame(new_frame_number, 1, 1, "#000000", original_frame->GetAudioSamplesCount(), original_frame->GetAudioChannelsCount()));
		frame->SampleRate(original_frame->SampleRate());
		frame->ChannelsLayout(original_frame->ChannelsLayout());

		// Copy the image from the odd field
		frame->AddImage(tr1::shared_ptr<QImage>(new QImage(*original_frame->GetImage())));

		// Loop through each channel, add audio
		for (int channel = 0; channel < original_frame->GetAudioChannelsCount(); channel++)
			frame->AddAudio(true, channel, 0, original_frame->GetAudioSamples(channel), original_frame->GetAudioSamplesCount(), 1.0);

		// Get time mapped frame number (used to increase speed, change direction, etc...)
		tr1::shared_ptr<Frame> new_frame = get_time_mapped_frame(frame, requested_frame);

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
tr1::shared_ptr<Frame> Clip::get_time_mapped_frame(tr1::shared_ptr<Frame> frame, long int frame_number) throw(ReaderClosed)
{
	// Check for valid reader
	if (!reader)
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.", "");

	tr1::shared_ptr<Frame> new_frame;

	// Check for a valid time map curve
	if (time.Values.size() > 1)
	{
		// create buffer and resampler
		juce::AudioSampleBuffer *samples = NULL;
		if (!resampler)
			resampler = new AudioResampler();

		// Get new frame number
		int new_frame_number = round(time.GetValue(frame_number));

		// Create a new frame
		int samples_in_frame = Frame::GetSamplesPerFrame(new_frame_number, reader->info.fps, reader->info.sample_rate, frame->GetAudioChannelsCount());
		new_frame = tr1::shared_ptr<Frame>(new Frame(new_frame_number, 1, 1, "#000000", samples_in_frame, frame->GetAudioChannelsCount()));

		// Copy the image from the new frame
		new_frame->AddImage(reader->GetFrame(new_frame_number)->GetImage());


		// Get delta (difference in previous Y value)
		int delta = int(round(time.GetDelta(frame_number)));

		// Init audio vars
		int sample_rate = reader->info.sample_rate;
		int channels = reader->info.channels;
		int number_of_samples = reader->GetFrame(new_frame_number)->GetAudioSamplesCount();

		// Determine if we are speeding up or slowing down
		if (time.GetRepeatFraction(frame_number).den > 1)
		{
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
				samples->addFrom(channel, 0, reader->GetFrame(new_frame_number)->GetAudioSamples(channel), number_of_samples, 1.0f);

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
				new_frame->AddAudio(true, channel, 0, resampled_buffer->getReadPointer(channel, start), number_of_samples, 1.0f);

			// Clean up
			resampled_buffer = NULL;

		}
		else if (abs(delta) > 1 && abs(delta) < 100)
		{
			// SPEED UP (multiple frames of audio), as long as it's not more than X frames
			samples = new juce::AudioSampleBuffer(channels, number_of_samples * abs(delta));
			samples->clear();
			int start = 0;

			if (delta > 0)
			{
				// Loop through each frame in this delta
				for (int delta_frame = new_frame_number - (delta - 1); delta_frame <= new_frame_number; delta_frame++)
				{
					// buffer to hold detal samples
					int number_of_delta_samples = reader->GetFrame(delta_frame)->GetAudioSamplesCount();
					AudioSampleBuffer* delta_samples = new juce::AudioSampleBuffer(channels, number_of_delta_samples);
					delta_samples->clear();

					for (int channel = 0; channel < channels; channel++)
						delta_samples->addFrom(channel, 0, reader->GetFrame(delta_frame)->GetAudioSamples(channel), number_of_delta_samples, 1.0f);

					// Reverse the samples (if needed)
					if (!time.IsIncreasing(frame_number))
						reverse_buffer(delta_samples);

					// Copy the samples to
					for (int channel = 0; channel < channels; channel++)
						// Get the audio samples for this channel
						samples->addFrom(channel, start, delta_samples->getReadPointer(channel), number_of_delta_samples, 1.0f);

					// Clean up
					delete delta_samples;
					delta_samples = NULL;

					// Increment start position
					start += number_of_delta_samples;
				}
			}
			else
			{
				// Loop through each frame in this delta
				for (int delta_frame = new_frame_number - (delta + 1); delta_frame >= new_frame_number; delta_frame--)
				{
					// buffer to hold delta samples
					int number_of_delta_samples = reader->GetFrame(delta_frame)->GetAudioSamplesCount();
					AudioSampleBuffer* delta_samples = new juce::AudioSampleBuffer(channels, number_of_delta_samples);
					delta_samples->clear();

					for (int channel = 0; channel < channels; channel++)
						delta_samples->addFrom(channel, 0, reader->GetFrame(delta_frame)->GetAudioSamples(channel), number_of_delta_samples, 1.0f);

					// Reverse the samples (if needed)
					if (!time.IsIncreasing(frame_number))
						reverse_buffer(delta_samples);

					// Copy the samples to
					for (int channel = 0; channel < channels; channel++)
						// Get the audio samples for this channel
						samples->addFrom(channel, start, delta_samples->getReadPointer(channel), number_of_delta_samples, 1.0f);

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
		else
		{
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

	} else
		// Use original frame
		return frame;

	// Return new time mapped frame
	return new_frame;
}

// Adjust frame number minimum value
int Clip::adjust_frame_number_minimum(long int frame_number)
{
	// Never return a frame number 0 or below
	if (frame_number < 1)
		return 1;
	else
		return frame_number;

}

// Generate JSON string of this object
string Clip::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Get all properties for a specific frame
string Clip::PropertiesJSON(long int requested_frame) {

	// Requested Point
	Point requested_point(requested_frame, requested_frame);

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), false, 0, -1, -1, CONSTANT, -1, true);
	root["position"] = add_property_json("Position", Position(), "float", "", false, 0, 0, 1000 * 60 * 30, CONSTANT, -1, false);
	root["layer"] = add_property_json("Layer", Layer(), "int", "", false, 0, 0, 1000, CONSTANT, -1, false);
	root["start"] = add_property_json("Start", Start(), "float", "", false, 0, 0, 1000 * 60 * 30, CONSTANT, -1, false);
	root["end"] = add_property_json("End", End(), "float", "", false, 0, 0, 1000 * 60 * 30, CONSTANT, -1, false);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", false, 0, 0, 1000 * 60 * 30, CONSTANT, -1, true);
	root["gravity"] = add_property_json("Gravity", gravity, "int", "", false, 0, -1, -1, CONSTANT, -1, false);
	root["scale"] = add_property_json("Scale", scale, "int", "", false, 0, -1, -1, CONSTANT, -1, false);
	root["anchor"] = add_property_json("Anchor", anchor, "int", "", false, 0, -1, -1, CONSTANT, -1, false);
	root["waveform"] = add_property_json("Waveform", waveform, "bool", "", false, 0, -1, -1, CONSTANT, -1, false);

	// Keyframes
	root["location_x"] = add_property_json("Location X", location_x.GetValue(requested_frame), "float", "", location_x.Contains(requested_point), location_x.GetCount(), -10000, 10000, location_x.GetClosestPoint(requested_point).interpolation, location_x.GetClosestPoint(requested_point).co.X, false);
	root["location_y"] = add_property_json("Location Y", location_y.GetValue(requested_frame), "float", "", location_y.Contains(requested_point), location_y.GetCount(), -10000, 10000, location_y.GetClosestPoint(requested_point).interpolation, location_y.GetClosestPoint(requested_point).co.X, false);
	root["scale_x"] = add_property_json("Scale X", scale_x.GetValue(requested_frame), "float", "", scale_x.Contains(requested_point), scale_x.GetCount(), 0.0, 100.0, scale_x.GetClosestPoint(requested_point).interpolation, scale_x.GetClosestPoint(requested_point).co.X, false);
	root["scale_y"] = add_property_json("Scale Y", scale_y.GetValue(requested_frame), "float", "", scale_y.Contains(requested_point), scale_y.GetCount(), 0.0, 100.0, scale_y.GetClosestPoint(requested_point).interpolation, scale_y.GetClosestPoint(requested_point).co.X, false);
	root["alpha"] = add_property_json("Alpha", alpha.GetValue(requested_frame), "float", "", alpha.Contains(requested_point), alpha.GetCount(), 0.0, 1.0, alpha.GetClosestPoint(requested_point).interpolation, alpha.GetClosestPoint(requested_point).co.X, false);
	root["rotation"] = add_property_json("Rotation", rotation.GetValue(requested_frame), "float", "", rotation.Contains(requested_point), rotation.GetCount(), -10000, 10000, rotation.GetClosestPoint(requested_point).interpolation, rotation.GetClosestPoint(requested_point).co.X, false);
	root["volume"] = add_property_json("Volume", volume.GetValue(requested_frame), "float", "", volume.Contains(requested_point), volume.GetCount(), 0.0, 1.0, volume.GetClosestPoint(requested_point).interpolation, volume.GetClosestPoint(requested_point).co.X, false);
	root["time"] = add_property_json("Time", time.GetValue(requested_frame), "float", "", time.Contains(requested_point), time.GetCount(), 0.0, 1000 * 60 * 30, time.GetClosestPoint(requested_point).interpolation, time.GetClosestPoint(requested_point).co.X, false);

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

			if (!existing_effect["type"].isNull())
				// Init the matching effect object
				if (existing_effect["type"].asString() == "Brightness")
					e = new Brightness();

				else if (existing_effect["type"].asString() == "ChromaKey")
					e = new ChromaKey();

				else if (existing_effect["type"].asString() == "Deinterlace")
					e = new Deinterlace();

				else if (existing_effect["type"].asString() == "Mask")
					e = new Mask();

				else if (existing_effect["type"].asString() == "Negate")
					e = new Negate();

				else if (existing_effect["type"].asString() == "Saturation")
					e = new Saturation();

			// Load Json into Effect
			e->SetJsonValue(existing_effect);

			// Add Effect to Timeline
			AddEffect(e);
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
				reader = new FFmpegReader(root["reader"]["path"].asString());
				reader->SetJsonValue(root["reader"]);

			} else if (type == "QtImageReader") {

				// Create new reader
				reader = new QtImageReader(root["reader"]["path"].asString());
				reader->SetJsonValue(root["reader"]);

			} else if (type == "ImageReader") {

				// Create new reader
				reader = new ImageReader(root["reader"]["path"].asString());
				reader->SetJsonValue(root["reader"]);

			} else if (type == "TextReader") {

				// Create new reader
				reader = new TextReader();
				reader->SetJsonValue(root["reader"]);

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
tr1::shared_ptr<Frame> Clip::apply_effects(tr1::shared_ptr<Frame> frame)
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
