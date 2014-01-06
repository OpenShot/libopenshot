/**
 * @file
 * @brief Source file for Clip class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
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
	wave_color = (Color){Keyframe(0), Keyframe(28672), Keyframe(65280)};

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
}

// Default Constructor for a clip
Clip::Clip()
{
	// Init all default settings
	init_settings();
}

// Constructor with reader
Clip::Clip(ReaderBase* reader) : reader(reader)
{
	// Init all default settings
	init_settings();

	// Open and Close the reader (to set the duration of the clip)
	Open();
	Close();
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
			cout << "READER FOUND: FFmpegReader" << endl;
		} catch(...) { }
	}

	// If no video found, try each reader
	if (!reader)
	{
		try
		{
			// Try an image reader
			reader = new ImageReader(path);
			cout << "READER FOUND: ImageReader" << endl;

		} catch(...) {
			try
			{
				// Try a video reader
				reader = new FFmpegReader(path);
				cout << "READER FOUND: FFmpegReader" << endl;

			} catch(BaseException ex) {
				// No Reader Found, Throw an exception
				cout << "READER NOT FOUND" << endl;
				throw ex;
			}
		}
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
	if (reader)
		reader->Close();
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
tr1::shared_ptr<Frame> Clip::GetFrame(int requested_frame) throw(ReaderClosed)
{
	if (reader)
	{
		// Adjust out of bounds frame number
		requested_frame = adjust_frame_number_minimum(requested_frame);

		// Is a time map detected
		int new_frame_number = requested_frame;
		if (time.Values.size() > 1)
			new_frame_number = time.GetInt(requested_frame);

		// Now that we have re-mapped what frame number is needed, go and get the frame pointer
		tr1::shared_ptr<Frame> frame = reader->GetFrame(new_frame_number);

		// Get time mapped frame number (used to increase speed, change direction, etc...)
		tr1::shared_ptr<Frame> new_frame = get_time_mapped_frame(frame, requested_frame);

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
			reversed->getSampleData(channel)[n] = buffer->getSampleData(channel)[s];
	}

	// Copy the samples back to the original array
	buffer->clear();
	// Loop through channels, and get audio samples
	for (int channel = 0; channel < channels; channel++)
		// Get the audio samples for this channel
		buffer->addFrom(channel, 0, reversed->getSampleData(channel), number_of_samples, 1.0f);

	delete reversed;
	reversed = NULL;
}

// Adjust the audio and image of a time mapped frame
tr1::shared_ptr<Frame> Clip::get_time_mapped_frame(tr1::shared_ptr<Frame> frame, int frame_number) throw(ReaderClosed)
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
		int new_frame_number = time.GetInt(frame_number);

		// Create a new frame
		int samples_in_frame = GetSamplesPerFrame(new_frame_number, reader->info.fps);
		new_frame = tr1::shared_ptr<Frame>(new Frame(new_frame_number, 1, 1, "#000000", samples_in_frame, frame->GetAudioChannelsCount()));

		// Copy the image from the new frame
		new_frame->AddImage(reader->GetFrame(new_frame_number)->GetImage());


		// Get delta (difference in previous Y value)
		int delta = int(round(time.GetDelta(frame_number)));

		// Init audio vars
		int sample_rate = reader->GetFrame(new_frame_number)->GetAudioSamplesRate();
		int channels = reader->info.channels;
		int number_of_samples = reader->GetFrame(new_frame_number)->GetAudioSamplesCount();

		// Determine if we are speeding up or slowing down
		if (time.GetRepeatFraction(frame_number).den > 1)
		{
			// Resample data, and return new buffer pointer
			AudioSampleBuffer *buffer = NULL;
			int resampled_buffer_size = 0;

			if (time.GetRepeatFraction(frame_number).num == 1)
			{
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
				buffer = resampler->GetResampledBuffer();

				// Save the resampled data in the cache
				audio_cache = new juce::AudioSampleBuffer(channels, buffer->getNumSamples());
				audio_cache->clear();
				for (int channel = 0; channel < channels; channel++)
					// Get the audio samples for this channel
					audio_cache->addFrom(channel, 0, buffer->getSampleData(channel), buffer->getNumSamples(), 1.0f);
			}

			// Get the length of the resampled buffer
			resampled_buffer_size = audio_cache->getNumSamples();

			// Just take the samples we need for the requested frame
			int start = (number_of_samples * (time.GetRepeatFraction(frame_number).num - 1));
			if (start > 0)
				start -= 1;
			for (int channel = 0; channel < channels; channel++)
				// Add new (slower) samples, to the frame object
				new_frame->AddAudio(true, channel, 0, audio_cache->getSampleData(channel, start), number_of_samples, 1.0f);

			// Clean up if the final section
			if (time.GetRepeatFraction(frame_number).num == time.GetRepeatFraction(frame_number).den)
			{
				// Clear, since we don't want it maintain state yet
				delete audio_cache;
				audio_cache = NULL;
			}

			// Clean up
			buffer = NULL;


			// Determine next unique frame (after these repeating frames)
			//int next_unique_frame = time.GetInt(frame_number + (time.GetRepeatFraction(frame_number).den - time.GetRepeatFraction(frame_number).num) + 1);
			//if (next_unique_frame != new_frame_number)
			//	// Overlay the next frame on top of this frame (to create a smoother slow motion effect)
			//	new_frame->AddImage(reader->GetFrame(next_unique_frame)->GetImage(), float(time.GetRepeatFraction(frame_number).num) / float(time.GetRepeatFraction(frame_number).den));

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
						samples->addFrom(channel, start, delta_samples->getSampleData(channel), number_of_delta_samples, 1.0f);

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
						samples->addFrom(channel, start, delta_samples->getSampleData(channel), number_of_delta_samples, 1.0f);

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
				new_frame->AddAudio(true, channel, 0, buffer->getSampleData(channel), number_of_samples, 1.0f);

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
				new_frame->AddAudio(true, channel, 0, samples->getSampleData(channel), number_of_samples, 1.0f);


		}

		// clean up
		//delete resampler;
		//resampler = NULL;

		delete samples;
		samples = NULL;

	} else
		// Use original frame
		return frame;

	// Return new time mapped frame
	return new_frame;
}

// Adjust frame number minimum value
int Clip::adjust_frame_number_minimum(int frame_number)
{
	// Never return a frame number 0 or below
	if (frame_number < 1)
		return 1;
	else
		return frame_number;

}

// Calculate the # of samples per video frame (for a specific frame number)
int Clip::GetSamplesPerFrame(int frame_number, Fraction rate) throw(ReaderClosed)
{
	// Check for valid reader
	if (!reader)
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.", "");

	// Get the total # of samples for the previous frame, and the current frame (rounded)
	double fps = rate.Reciprocal().ToDouble();
	double previous_samples = round((reader->info.sample_rate * fps) * (frame_number - 1));
	double total_samples = round((reader->info.sample_rate * fps) * frame_number);

	// Subtract the previous frame's total samples with this frame's total samples.  Not all sample rates can
	// be evenly divided into frames, so each frame can have have different # of samples.
	double samples_per_frame = total_samples - previous_samples;
	return samples_per_frame;
}

// Generate JSON string of this object
string Clip::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
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
	if (root["gravity"] != Json::nullValue)
		gravity = (GravityType) root["gravity"].asInt();
	if (root["scale"] != Json::nullValue)
		scale = (ScaleType) root["scale"].asInt();
	if (root["anchor"] != Json::nullValue)
		anchor = (AnchorType) root["anchor"].asInt();
	if (root["waveform"] != Json::nullValue)
		waveform = root["waveform"].asBool();
	if (root["scale_x"] != Json::nullValue)
		scale_x.SetJsonValue(root["scale_x"]);
	if (root["scale_y"] != Json::nullValue)
		scale_y.SetJsonValue(root["scale_y"]);
	if (root["location_x"] != Json::nullValue)
		location_x.SetJsonValue(root["location_x"]);
	if (root["location_y"] != Json::nullValue)
		location_y.SetJsonValue(root["location_y"]);
	if (root["alpha"] != Json::nullValue)
		alpha.SetJsonValue(root["alpha"]);
	if (root["rotation"] != Json::nullValue)
		rotation.SetJsonValue(root["rotation"]);
	if (root["time"] != Json::nullValue)
		time.SetJsonValue(root["time"]);
	if (root["volume"] != Json::nullValue)
		volume.SetJsonValue(root["volume"]);
	if (root["wave_color"] != Json::nullValue)
		wave_color.SetJsonValue(root["wave_color"]);
	if (root["crop_width"] != Json::nullValue)
		crop_width.SetJsonValue(root["crop_width"]);
	if (root["crop_height"] != Json::nullValue)
		crop_height.SetJsonValue(root["crop_height"]);
	if (root["crop_x"] != Json::nullValue)
		crop_x.SetJsonValue(root["crop_x"]);
	if (root["crop_y"] != Json::nullValue)
		crop_y.SetJsonValue(root["crop_y"]);
	if (root["shear_x"] != Json::nullValue)
		shear_x.SetJsonValue(root["shear_x"]);
	if (root["shear_y"] != Json::nullValue)
		shear_y.SetJsonValue(root["shear_y"]);
	if (root["perspective_c1_x"] != Json::nullValue)
		perspective_c1_x.SetJsonValue(root["perspective_c1_x"]);
	if (root["perspective_c1_y"] != Json::nullValue)
		perspective_c1_y.SetJsonValue(root["perspective_c1_y"]);
	if (root["perspective_c2_x"] != Json::nullValue)
		perspective_c2_x.SetJsonValue(root["perspective_c2_x"]);
	if (root["perspective_c2_y"] != Json::nullValue)
		perspective_c2_y.SetJsonValue(root["perspective_c2_y"]);
	if (root["perspective_c3_x"] != Json::nullValue)
		perspective_c3_x.SetJsonValue(root["perspective_c3_x"]);
	if (root["perspective_c3_y"] != Json::nullValue)
		perspective_c3_y.SetJsonValue(root["perspective_c3_y"]);
	if (root["perspective_c4_x"] != Json::nullValue)
		perspective_c4_x.SetJsonValue(root["perspective_c4_x"]);
	if (root["perspective_c4_y"] != Json::nullValue)
		perspective_c4_y.SetJsonValue(root["perspective_c4_y"]);
	if (root["reader"] != Json::nullValue) // does Json contain a reader?
	{
		if (root["reader"]["type"] != Json::nullValue) // does the reader Json contain a 'type'?
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

			// Re-Open reader (if needed)
			if (already_open)
				reader->Open();

		}
	}
}
