/**
 * @file
 * @brief Source file for Clip class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Clip.h"

#include "AudioResampler.h"
#include "Exceptions.h"
#include "FFmpegReader.h"
#include "FrameMapper.h"
#include "QtImageReader.h"
#include "ChunkReader.h"
#include "DummyReader.h"
#include "Timeline.h"
#include "ZmqLogger.h"

#ifdef USE_IMAGEMAGICK
	#include "MagickUtilities.h"
	#include "ImageReader.h"
	#include "TextReader.h"
#endif

#include <Qt>

using namespace openshot;

// Init default settings for a clip
void Clip::init_settings()
{
	// Init clip settings
	Position(0.0);
	Layer(0);
	Start(0.0);
	ClipBase::End(0.0);
	gravity = GRAVITY_CENTER;
	scale = SCALE_FIT;
	anchor = ANCHOR_CANVAS;
	display = FRAME_DISPLAY_NONE;
	mixing = VOLUME_MIX_NONE;
	waveform = false;
	previous_properties = "";
	parentObjectId = "";

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

	// Initialize the attached object and attached clip as null pointers
	parentTrackedObject = nullptr;
	parentClipObject = NULL;

	// Init reader info struct
	init_reader_settings();
}

// Init reader info details
void Clip::init_reader_settings() {
	if (reader) {
		// Init rotation (if any)
		init_reader_rotation();

		// Initialize info struct
		info = reader->info;

		// Init cache
		final_cache.SetMaxBytesFromInfo(8, info.width, info.height, info.sample_rate, info.channels);
	}
}

// Init reader's rotation (if any)
void Clip::init_reader_rotation() {
	// Dont init rotation if clip has keyframes
	if (rotation.GetCount() > 0)
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
		ClipBase::End(reader->info.duration);
		reader->ParentClip(this);
		// Init reader info struct
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

	// Determine if common video formats (or image sequences)
	if (ext=="avi" || ext=="mov" || ext=="mkv" ||  ext=="mpg" || ext=="mpeg" || ext=="mp3" || ext=="mp4" || ext=="mts" ||
		ext=="ogg" || ext=="wav" || ext=="wmv" || ext=="webm" || ext=="vob" || path.find("%") != std::string::npos)
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
		ClipBase::End(reader->info.duration);
		reader->ParentClip(this);
		allocated_reader = reader;
		// Init reader info struct
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
		reader = NULL;
	}

	// Close the resampler
	if (resampler) {
		delete resampler;
		resampler = NULL;
	}

	// Close clip
	Close();
}

// Attach clip to bounding box
void Clip::AttachToObject(std::string object_id)
{
	// Search for the tracked object on the timeline
	Timeline* parentTimeline = static_cast<Timeline *>(ParentTimeline());

	if (parentTimeline) {
		// Create a smart pointer to the tracked object from the timeline
		std::shared_ptr<openshot::TrackedObjectBase> trackedObject = parentTimeline->GetTrackedObject(object_id);
		Clip* clipObject = parentTimeline->GetClip(object_id);

		// Check for valid tracked object
		if (trackedObject){
			SetAttachedObject(trackedObject);
            parentClipObject = NULL;
		}
		else if (clipObject) {
			SetAttachedClip(clipObject);
            parentTrackedObject = nullptr;
		}
	}
}

// Set the pointer to the trackedObject this clip is attached to
void Clip::SetAttachedObject(std::shared_ptr<openshot::TrackedObjectBase> trackedObject){
	parentTrackedObject = trackedObject;
}

// Set the pointer to the clip this clip is attached to
void Clip::SetAttachedClip(Clip* clipObject){
	parentClipObject = clipObject;
}

/// Set the current reader
void Clip::Reader(ReaderBase* new_reader)
{
	// Delete previously allocated reader (if not related to new reader)
	// FrameMappers that point to the same allocated reader are ignored
	bool is_same_reader = false;
	if (new_reader && allocated_reader) {
		if (new_reader->Name() == "FrameMapper") {
			// Determine if FrameMapper is pointing at the same allocated ready
			FrameMapper* clip_mapped_reader = static_cast<FrameMapper*>(new_reader);
			if (allocated_reader == clip_mapped_reader->Reader()) {
				is_same_reader = true;
			}
		}
	}
	// Clear existing allocated reader (if different)
	if (allocated_reader && !is_same_reader) {
		reader->Close();
		allocated_reader->Close();
		delete allocated_reader;
		reader = NULL;
		allocated_reader = NULL;
	}

	// set reader pointer
	reader = new_reader;

	// set parent
	if (reader) {
		reader->ParentClip(this);

		// Init reader info struct
		init_reader_settings();
	}
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
			ClipBase::End(reader->info.duration);
	}
	else
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.");
}

// Close the internal reader
void Clip::Close()
{
	if (is_open && reader) {
		ZmqLogger::Instance()->AppendDebugMethod("Clip::Close");

		// Close the reader
		reader->Close();
	}

	// Clear cache
	final_cache.Clear();
	is_open = false;
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

// Override End() position
void Clip::End(float value) {
	ClipBase::End(value);
}

// Set associated Timeline pointer
void Clip::ParentTimeline(openshot::TimelineBase* new_timeline) {
	timeline = new_timeline;

	// Clear cache (it might have changed)
	final_cache.Clear();
}

// Create an openshot::Frame object for a specific frame number of this reader.
std::shared_ptr<Frame> Clip::GetFrame(int64_t clip_frame_number)
{
	// Call override of GetFrame
	return GetFrame(NULL, clip_frame_number, NULL);
}

// Create an openshot::Frame object for a specific frame number of this reader.
// NOTE: background_frame is ignored in this method (this method is only used by Effect classes)
std::shared_ptr<Frame> Clip::GetFrame(std::shared_ptr<openshot::Frame> background_frame, int64_t clip_frame_number)
{
	// Call override of GetFrame
	return GetFrame(background_frame, clip_frame_number, NULL);
}

// Use an existing openshot::Frame object and draw this Clip's frame onto it
std::shared_ptr<Frame> Clip::GetFrame(std::shared_ptr<openshot::Frame> background_frame, int64_t clip_frame_number, openshot::TimelineInfoStruct* options)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The Clip is closed.  Call Open() before calling this method.");

	if (reader)
	{
		// Get frame object
		std::shared_ptr<Frame> frame = NULL;

		// Check cache
		frame = final_cache.GetFrame(clip_frame_number);
		if (!frame) {
            // Generate clip frame
            frame = GetOrCreateFrame(clip_frame_number);

            // Get frame size and frame #
            int64_t timeline_frame_number = clip_frame_number;
            QSize timeline_size(frame->GetWidth(), frame->GetHeight());
            if (background_frame) {
                // If a background frame is provided, use it instead
                timeline_frame_number = background_frame->number;
                timeline_size.setWidth(background_frame->GetWidth());
                timeline_size.setHeight(background_frame->GetHeight());
            }

            // Get time mapped frame object (used to increase speed, change direction, etc...)
            apply_timemapping(frame);

            // Apply waveform image (if any)
            apply_waveform(frame, timeline_size);

            // Apply effects BEFORE applying keyframes (if any local or global effects are used)
            apply_effects(frame, timeline_frame_number, options, true);

            // Apply keyframe / transforms to current clip image
            apply_keyframes(frame, timeline_size);

            // Apply effects AFTER applying keyframes (if any local or global effects are used)
            apply_effects(frame, timeline_frame_number, options, false);

            // Add final frame to cache (before flattening into background_frame)
            final_cache.Add(frame);
        }

        if (!background_frame) {
            // Create missing background_frame w/ transparent color (if needed)
            background_frame = std::make_shared<Frame>(frame->number, frame->GetWidth(), frame->GetHeight(),
                                                       "#00000000",  frame->GetAudioSamplesCount(),
                                                       frame->GetAudioChannelsCount());
        }

		// Apply background canvas (i.e. flatten this image onto previous layer image)
		apply_background(frame, background_frame);

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

// Return the associated ParentClip (if any)
openshot::Clip* Clip::GetParentClip() {
    if (!parentObjectId.empty() && (!parentClipObject && !parentTrackedObject)) {
        // Attach parent clip OR object to this clip
        AttachToObject(parentObjectId);
    }
    return parentClipObject;
}

// Return the associated Parent Tracked Object (if any)
std::shared_ptr<openshot::TrackedObjectBase> Clip::GetParentTrackedObject() {
    if (!parentObjectId.empty() && (!parentClipObject && !parentTrackedObject)) {
        // Attach parent clip OR object to this clip
        AttachToObject(parentObjectId);
    }
    return parentTrackedObject;
}

// Get file extension
std::string Clip::get_file_extension(std::string path)
{
	// return last part of path
	return path.substr(path.find_last_of(".") + 1);
}

// Adjust the audio and image of a time mapped frame
void Clip::apply_timemapping(std::shared_ptr<Frame> frame)
{
	// Check for valid reader
	if (!reader)
		// Throw error if reader not initialized
		throw ReaderClosed("No Reader has been initialized for this Clip.  Call Reader(*reader) before calling this method.");

	// Check for a valid time map curve
	if (time.GetLength() > 1)
	{
		const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

		int64_t clip_frame_number = frame->number;
		int64_t new_frame_number = adjust_frame_number_minimum(time.GetLong(clip_frame_number));

		// create buffer
		juce::AudioBuffer<float> *source_samples = nullptr;

		// Get delta (difference from this frame to the next time mapped frame: Y value)
		double delta = time.GetDelta(clip_frame_number + 1);
		bool is_increasing = time.IsIncreasing(clip_frame_number + 1);

		// Determine length of source audio (in samples)
		// A delta of 1.0 == normal expected samples
		// A delta of 0.5 == 50% of normal expected samples
		// A delta of 2.0 == 200% of normal expected samples
		int target_sample_count = Frame::GetSamplesPerFrame(adjust_timeline_framenumber(clip_frame_number), Reader()->info.fps,
															  Reader()->info.sample_rate,
															  Reader()->info.channels);
		int source_sample_count = round(target_sample_count * fabs(delta));

		// Determine starting audio location
		AudioLocation location;
		if (previous_location.frame == 0 || abs(new_frame_number - previous_location.frame) > 2) {
			// No previous location OR gap detected
			location.frame = new_frame_number;
			location.sample_start = 0;

			// Create / Reset resampler
			// We don't want to interpolate between unrelated audio data
			if (resampler) {
				delete resampler;
			}
			// Init resampler with # channels from Reader (should match the timeline)
			resampler = new AudioResampler(Reader()->info.channels);

			// Allocate buffer of silence to initialize some data inside the resampler
			// To prevent it from becoming input limited
			juce::AudioBuffer<float> init_samples(Reader()->info.channels, 64);
			init_samples.clear();
			resampler->SetBuffer(&init_samples, 1.0);
			resampler->GetResampledBuffer();

		} else {
			// Use previous location
			location = previous_location;
		}

		if (source_sample_count <= 0) {
			// Add silence and bail (we don't need any samples)
			frame->AddAudioSilence(target_sample_count);
			return;
		}

		// Allocate a new sample buffer for these delta frames
		source_samples = new juce::AudioBuffer<float>(Reader()->info.channels, source_sample_count);
		source_samples->clear();

		// Determine ending audio location
		int remaining_samples = source_sample_count;
		int source_pos = 0;
		while (remaining_samples > 0) {
			std::shared_ptr<Frame> source_frame = GetOrCreateFrame(location.frame, false);
			int frame_sample_count = source_frame->GetAudioSamplesCount() - location.sample_start;

			if (frame_sample_count == 0) {
				// No samples found in source frame (fill with silence)
				if (is_increasing) {
					location.frame++;
				} else {
					location.frame--;
				}
				location.sample_start = 0;
				break;
			}
			if (remaining_samples - frame_sample_count >= 0) {
				// Use all frame samples & increment location
				for (int channel = 0; channel < source_frame->GetAudioChannelsCount(); channel++) {
					source_samples->addFrom(channel, source_pos, source_frame->GetAudioSamples(channel) + location.sample_start, frame_sample_count, 1.0f);
				}
				if (is_increasing) {
					location.frame++;
				} else {
					location.frame--;   
				}
				location.sample_start = 0;
				remaining_samples -= frame_sample_count;
				source_pos += frame_sample_count;

			} else {
				// Use just what is needed (and reverse samples)
				for (int channel = 0; channel < source_frame->GetAudioChannelsCount(); channel++) {
					source_samples->addFrom(channel, source_pos, source_frame->GetAudioSamples(channel) + location.sample_start, remaining_samples, 1.0f);
				}
				location.sample_start += remaining_samples;
				remaining_samples = 0;
				source_pos += remaining_samples;
			}

		}

		// Resize audio for current frame object + fill with silence
		// We are fixing to clobber this with actual audio data (possibly resampled)
		frame->AddAudioSilence(target_sample_count);

		if (source_sample_count != target_sample_count) {
			// Resample audio (if needed)
			double resample_ratio = double(source_sample_count) / double(target_sample_count);
			resampler->SetBuffer(source_samples, resample_ratio);

			// Resample the data
			juce::AudioBuffer<float> *resampled_buffer = resampler->GetResampledBuffer();

			// Fill the frame with resampled data
			for (int channel = 0; channel < Reader()->info.channels; channel++) {
				// Add new (slower) samples, to the frame object
				frame->AddAudio(true, channel, 0, resampled_buffer->getReadPointer(channel, 0), std::min(resampled_buffer->getNumSamples(), target_sample_count), 1.0f);
			}
		} else {
			// Fill the frame
			for (int channel = 0; channel < Reader()->info.channels; channel++) {
				// Add new (slower) samples, to the frame object
				frame->AddAudio(true, channel, 0, source_samples->getReadPointer(channel, 0), target_sample_count, 1.0f);
			}
		}

		// Clean up
		delete source_samples;

		// Set previous location
		previous_location = location;
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
std::shared_ptr<Frame> Clip::GetOrCreateFrame(int64_t number, bool enable_time)
{
	try {
		// Init to requested frame
		int64_t clip_frame_number = adjust_frame_number_minimum(number);

		// Adjust for time-mapping (if any)
		if (enable_time && time.GetLength() > 1) {
			clip_frame_number = adjust_frame_number_minimum(time.GetLong(clip_frame_number));
		}

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod(
				"Clip::GetOrCreateFrame (from reader)",
				"number", number, "clip_frame_number", clip_frame_number);

		// Attempt to get a frame (but this could fail if a reader has just been closed)
		auto reader_frame = reader->GetFrame(clip_frame_number);
		reader_frame->number = number; // Override frame # (due to time-mapping might change it)

		// Return real frame
		if (reader_frame) {
			// Create a new copy of reader frame
			// This allows a clip to modify the pixels and audio of this frame without
			// changing the underlying reader's frame data
			auto reader_copy = std::make_shared<Frame>(*reader_frame.get());
			if (has_video.GetInt(number) == 0) {
				// No video, so add transparent pixels
				reader_copy->AddColor(QColor(Qt::transparent));
			}
			if (has_audio.GetInt(number) == 0 || number > reader->info.video_length) {
				// No audio, so include silence (also, mute audio if past end of reader)
				reader_copy->AddAudioSilence(reader_copy->GetAudioSamplesCount());
			}
			return reader_copy;
		}

	} catch (const ReaderClosed & e) {
		// ...
	} catch (const OutOfBoundsFrame & e) {
		// ...
	}

	// Estimate # of samples needed for this frame
	int estimated_samples_in_frame = Frame::GetSamplesPerFrame(number, reader->info.fps, reader->info.sample_rate, reader->info.channels);

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod(
		"Clip::GetOrCreateFrame (create blank)",
		"number", number,
		"estimated_samples_in_frame", estimated_samples_in_frame);

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
	root["parentObjectId"] = add_property_json("Parent", 0.0, "string", parentObjectId, NULL, -1, -1, false, requested_frame);

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

	// Add the parentClipObject's properties
	if (parentClipObject)
	{
		// Convert Clip's frame position to Timeline's frame position
		long clip_start_position = round(Position() * info.fps.ToDouble()) + 1;
		long clip_start_frame = (Start() * info.fps.ToDouble()) + 1;
		double timeline_frame_number = requested_frame + clip_start_position - clip_start_frame;

		// Correct the parent Clip Object properties by the clip's reference system
		float parentObject_location_x = parentClipObject->location_x.GetValue(timeline_frame_number);
		float parentObject_location_y = parentClipObject->location_y.GetValue(timeline_frame_number);
		float parentObject_scale_x = parentClipObject->scale_x.GetValue(timeline_frame_number);
		float parentObject_scale_y = parentClipObject->scale_y.GetValue(timeline_frame_number);
		float parentObject_shear_x = parentClipObject->shear_x.GetValue(timeline_frame_number);
		float parentObject_shear_y = parentClipObject->shear_y.GetValue(timeline_frame_number);
		float parentObject_rotation = parentClipObject->rotation.GetValue(timeline_frame_number);

		// Add the parent Clip Object properties to JSON
		root["location_x"] = add_property_json("Location X", parentObject_location_x, "float", "", &location_x, -1.0, 1.0, false, requested_frame);
		root["location_y"] = add_property_json("Location Y", parentObject_location_y, "float", "", &location_y, -1.0, 1.0, false, requested_frame);
		root["scale_x"] = add_property_json("Scale X", parentObject_scale_x, "float", "", &scale_x, 0.0, 1.0, false, requested_frame);
		root["scale_y"] = add_property_json("Scale Y", parentObject_scale_y, "float", "", &scale_y, 0.0, 1.0, false, requested_frame);
		root["rotation"] = add_property_json("Rotation", parentObject_rotation, "float", "", &rotation, -360, 360, false, requested_frame);
		root["shear_x"] = add_property_json("Shear X", parentObject_shear_x, "float", "", &shear_x, -1.0, 1.0, false, requested_frame);
		root["shear_y"] = add_property_json("Shear Y", parentObject_shear_y, "float", "", &shear_y, -1.0, 1.0, false, requested_frame);
	}
	else
	{
		// Add this own clip's properties to JSON
		root["location_x"] = add_property_json("Location X", location_x.GetValue(requested_frame), "float", "", &location_x, -1.0, 1.0, false, requested_frame);
		root["location_y"] = add_property_json("Location Y", location_y.GetValue(requested_frame), "float", "", &location_y, -1.0, 1.0, false, requested_frame);
		root["scale_x"] = add_property_json("Scale X", scale_x.GetValue(requested_frame), "float", "", &scale_x, 0.0, 1.0, false, requested_frame);
		root["scale_y"] = add_property_json("Scale Y", scale_y.GetValue(requested_frame), "float", "", &scale_y, 0.0, 1.0, false, requested_frame);
		root["rotation"] = add_property_json("Rotation", rotation.GetValue(requested_frame), "float", "", &rotation, -360, 360, false, requested_frame);
		root["shear_x"] = add_property_json("Shear X", shear_x.GetValue(requested_frame), "float", "", &shear_x, -1.0, 1.0, false, requested_frame);
		root["shear_y"] = add_property_json("Shear Y", shear_y.GetValue(requested_frame), "float", "", &shear_y, -1.0, 1.0, false, requested_frame);
	}

	// Keyframes
	root["alpha"] = add_property_json("Alpha", alpha.GetValue(requested_frame), "float", "", &alpha, 0.0, 1.0, false, requested_frame);
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
	root["parentObjectId"] = parentObjectId;
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

	// Set data from Json (if key is found)
	if (!root["parentObjectId"].isNull()){
		parentObjectId = root["parentObjectId"].asString();
		if (parentObjectId.size() > 0 && parentObjectId != ""){
			AttachToObject(parentObjectId);
		} else{
			parentTrackedObject = nullptr;
			parentClipObject = NULL;
		}
	}
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
			// Skip NULL nodes
			if (existing_effect.isNull()) {
				continue;
			}

			// Create Effect
			EffectBase *e = NULL;
			if (!existing_effect["type"].isNull()) {

				// Create instance of effect
				if ( (e = EffectInfo().CreateEffect(existing_effect["type"].asString()))) {

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

				// Close and delete existing allocated reader (if any)
				Reader(NULL);
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
			if (already_open) {
				reader->Open();
			}
		}
	}

	// Clear cache (it might have changed)
	final_cache.Clear();
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

	// Get the parent timeline of this clip
	Timeline* parentTimeline = static_cast<Timeline *>(ParentTimeline());

	if (parentTimeline)
		effect->ParentTimeline(parentTimeline);

	#ifdef USE_OPENCV
	// Add Tracked Object to Timeline
	if (effect->info.has_tracked_object){

		// Check if this clip has a parent timeline
		if (parentTimeline){

			effect->ParentTimeline(parentTimeline);

			// Iterate through effect's vector of Tracked Objects
			for (auto const& trackedObject : effect->trackedObjects){

				// Cast the Tracked Object as TrackedObjectBBox
				std::shared_ptr<TrackedObjectBBox> trackedObjectBBox = std::static_pointer_cast<TrackedObjectBBox>(trackedObject.second);

				// Set the Tracked Object's parent clip to this
				trackedObjectBBox->ParentClip(this);

				// Add the Tracked Object to the timeline
				parentTimeline->AddTrackedObject(trackedObjectBBox);
			}
		}
	}
	#endif

	// Clear cache (it might have changed)
	final_cache.Clear();
}

// Remove an effect from the clip
void Clip::RemoveEffect(EffectBase* effect)
{
	effects.remove(effect);

	// Clear cache (it might have changed)
	final_cache.Clear();
}

// Apply background image to the current clip image (i.e. flatten this image onto previous layer)
void Clip::apply_background(std::shared_ptr<openshot::Frame> frame, std::shared_ptr<openshot::Frame> background_frame) {
	// Add background canvas
	std::shared_ptr<QImage> background_canvas = background_frame->GetImage();
	QPainter painter(background_canvas.get());
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing, true);

	// Composite a new layer onto the image
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.drawImage(0, 0, *frame->GetImage());
	painter.end();

	// Add new QImage to frame
	frame->AddImage(background_canvas);
}

// Apply effects to the source frame (if any)
void Clip::apply_effects(std::shared_ptr<Frame> frame, int64_t timeline_frame_number, TimelineInfoStruct* options, bool before_keyframes)
{
	for (auto effect : effects)
	{
		// Apply the effect to this frame
		if (effect->info.apply_before_clip && before_keyframes) {
			effect->GetFrame(frame, frame->number);
		} else if (!effect->info.apply_before_clip && !before_keyframes) {
			effect->GetFrame(frame, frame->number);
		}
	}

	if (timeline != NULL && options != NULL) {
		// Apply global timeline effects (i.e. transitions & masks... if any)
		Timeline* timeline_instance = static_cast<Timeline*>(timeline);
		options->is_before_clip_keyframes = before_keyframes;
		timeline_instance->apply_effects(frame, timeline_frame_number, Layer(), options);
	}
}

// Compare 2 floating point numbers for equality
bool Clip::isNear(double a, double b)
{
	return fabs(a - b) < 0.000001;
}

// Apply keyframes to the source frame (if any)
void Clip::apply_keyframes(std::shared_ptr<Frame> frame, QSize timeline_size) {
	// Skip out if video was disabled or only an audio frame (no visualisation in use)
	if (!frame->has_image_data) {
		// Skip the rest of the image processing for performance reasons
		return;
	}

	// Get image from clip, and create transparent background image
	std::shared_ptr<QImage> source_image = frame->GetImage();
	std::shared_ptr<QImage> background_canvas = std::make_shared<QImage>(timeline_size.width(),
                                                                         timeline_size.height(),
																		 QImage::Format_RGBA8888_Premultiplied);
	background_canvas->fill(QColor(Qt::transparent));

	// Get transform from clip's keyframes
	QTransform transform = get_transform(frame, background_canvas->width(), background_canvas->height());

	// Load timeline's new frame image into a QPainter
	QPainter painter(background_canvas.get());
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing, true);

	// Apply transform (translate, rotate, scale)
	painter.setTransform(transform);

	// Composite a new layer onto the image
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.drawImage(0, 0, *source_image);

	if (timeline) {
		Timeline *t = static_cast<Timeline *>(timeline);

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
					frame_number_str << round((Position() - Start()) * t->info.fps.ToFloat()) + frame->number;
					break;

				case (FRAME_DISPLAY_BOTH):
					frame_number_str << round((Position() - Start()) * t->info.fps.ToFloat()) + frame->number << " (" << frame->number << ")";
					break;
			}

			// Draw frame number on top of image
			painter.setPen(QColor("#ffffff"));
			painter.drawText(20, 20, QString(frame_number_str.str().c_str()));
		}
	}
	painter.end();

	// Add new QImage to frame
	frame->AddImage(background_canvas);
}

// Apply apply_waveform image to the source frame (if any)
void Clip::apply_waveform(std::shared_ptr<Frame> frame, QSize timeline_size) {

	if (!Waveform()) {
		// Exit if no waveform is needed
		return;
	}

	// Get image from clip
	std::shared_ptr<QImage> source_image = frame->GetImage();

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Clip::apply_waveform (Generate Waveform Image)",
			"frame->number", frame->number,
			"Waveform()", Waveform(),
			"width", timeline_size.width(),
			"height", timeline_size.height());

	// Get the color of the waveform
	int red = wave_color.red.GetInt(frame->number);
	int green = wave_color.green.GetInt(frame->number);
	int blue = wave_color.blue.GetInt(frame->number);
	int alpha = wave_color.alpha.GetInt(frame->number);

	// Generate Waveform Dynamically (the size of the timeline)
	source_image = frame->GetWaveform(timeline_size.width(), timeline_size.height(), red, green, blue, alpha);
	frame->AddImage(source_image);
}

// Scale a source size to a target size (given a specific scale-type)
QSize Clip::scale_size(QSize source_size, ScaleType source_scale, int target_width, int target_height) {
    switch (source_scale)
    {
        case (SCALE_FIT): {
            source_size.scale(target_width, target_height, Qt::KeepAspectRatio);
            break;
        }
        case (SCALE_STRETCH): {
            source_size.scale(target_width, target_height, Qt::IgnoreAspectRatio);
            break;
        }
        case (SCALE_CROP): {
            source_size.scale(target_width, target_height, Qt::KeepAspectRatioByExpanding);;
            break;
        }
    }

    return source_size;
}

// Get QTransform from keyframes
QTransform Clip::get_transform(std::shared_ptr<Frame> frame, int width, int height)
{
	// Get image from clip
	std::shared_ptr<QImage> source_image = frame->GetImage();

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
		ZmqLogger::Instance()->AppendDebugMethod("Clip::get_transform (Set Alpha & Opacity)",
			"alpha_value", alpha_value,
			"frame->number", frame->number);
	}

	/* RESIZE SOURCE IMAGE - based on scale type */
	QSize source_size = scale_size(source_image->size(), scale, width, height);

	// Initialize parent object's properties (Clip or Tracked Object)
	float parentObject_location_x = 0.0;
	float parentObject_location_y = 0.0;
	float parentObject_scale_x = 1.0;
	float parentObject_scale_y = 1.0;
	float parentObject_shear_x = 0.0;
	float parentObject_shear_y = 0.0;
	float parentObject_rotation = 0.0;

	// Get the parentClipObject properties
	if (GetParentClip()){
        // Get the start trim position of the parent clip
        long parent_start_offset = parentClipObject->Start() * info.fps.ToDouble();
        long parent_frame_number = frame->number + parent_start_offset;

		// Get parent object's properties (Clip)
		parentObject_location_x = parentClipObject->location_x.GetValue(parent_frame_number);
		parentObject_location_y = parentClipObject->location_y.GetValue(parent_frame_number);
		parentObject_scale_x = parentClipObject->scale_x.GetValue(parent_frame_number);
		parentObject_scale_y = parentClipObject->scale_y.GetValue(parent_frame_number);
		parentObject_shear_x = parentClipObject->shear_x.GetValue(parent_frame_number);
		parentObject_shear_y = parentClipObject->shear_y.GetValue(parent_frame_number);
		parentObject_rotation = parentClipObject->rotation.GetValue(parent_frame_number);
	}

    // Get the parentTrackedObject properties
    if (GetParentTrackedObject()){
        // Get the attached object's parent clip's properties
        Clip* parentClip = (Clip*) parentTrackedObject->ParentClip();
        if (parentClip)
        {
            // Get the start trim position of the parent clip
            long parent_start_offset = parentClip->Start() * info.fps.ToDouble();
            long parent_frame_number = frame->number + parent_start_offset;

            // Access the parentTrackedObject's properties
            std::map<std::string, float> trackedObjectProperties = parentTrackedObject->GetBoxValues(parent_frame_number);

            // Get actual scaled parent size
            QSize parent_size = scale_size(QSize(parentClip->info.width, parentClip->info.height),
                                           parentClip->scale, width, height);

            // Get actual scaled tracked object size
            int trackedWidth = trackedObjectProperties["w"] * trackedObjectProperties["sx"] * parent_size.width() *
                    parentClip->scale_x.GetValue(parent_frame_number);
            int trackedHeight = trackedObjectProperties["h"] * trackedObjectProperties["sy"] * parent_size.height() *
                    parentClip->scale_y.GetValue(parent_frame_number);

            // Scale the clip source_size based on the actual tracked object size
            source_size = scale_size(source_size, scale, trackedWidth, trackedHeight);

            // Update parentObject's properties based on the tracked object's properties and parent clip's scale
            parentObject_location_x = parentClip->location_x.GetValue(parent_frame_number) + ((trackedObjectProperties["cx"] - 0.5) * parentClip->scale_x.GetValue(parent_frame_number));
            parentObject_location_y = parentClip->location_y.GetValue(parent_frame_number) + ((trackedObjectProperties["cy"] - 0.5) * parentClip->scale_y.GetValue(parent_frame_number));
            parentObject_rotation = trackedObjectProperties["r"] + parentClip->rotation.GetValue(parent_frame_number);
        }
    }

	/* GRAVITY LOCATION - Initialize X & Y to the correct values (before applying location curves) */
	float x = 0.0; // left
	float y = 0.0; // top

	// Adjust size for scale x and scale y
	float sx = scale_x.GetValue(frame->number); // percentage X scale
	float sy = scale_y.GetValue(frame->number); // percentage Y scale

	// Change clip's scale to parentObject's scale
	if(parentObject_scale_x != 0.0 && parentObject_scale_y != 0.0){
		sx*= parentObject_scale_x;
		sy*= parentObject_scale_y;
	}

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
	ZmqLogger::Instance()->AppendDebugMethod(
		"Clip::get_transform (Gravity)",
		"frame->number", frame->number,
		"source_clip->gravity", gravity,
		"scaled_source_width", scaled_source_width,
		"scaled_source_height", scaled_source_height);

	QTransform transform;

	/* LOCATION, ROTATION, AND SCALE */
	float r = rotation.GetValue(frame->number) + parentObject_rotation; // rotate in degrees
	x += width * (location_x.GetValue(frame->number) + parentObject_location_x); // move in percentage of final width
	y += height * (location_y.GetValue(frame->number) + parentObject_location_y); // move in percentage of final height
	float shear_x_value = shear_x.GetValue(frame->number) + parentObject_shear_x;
	float shear_y_value = shear_y.GetValue(frame->number) + parentObject_shear_y;
	float origin_x_value = origin_x.GetValue(frame->number);
	float origin_y_value = origin_y.GetValue(frame->number);

	// Transform source image (if needed)
	ZmqLogger::Instance()->AppendDebugMethod(
		"Clip::get_transform (Build QTransform - if needed)",
		"frame->number", frame->number,
		"x", x, "y", y,
		"r", r,
		"sx", sx, "sy", sy);

	if (!isNear(x, 0) || !isNear(y, 0)) {
		// TRANSLATE/MOVE CLIP
		transform.translate(x, y);
	}
	if (!isNear(r, 0) || !isNear(shear_x_value, 0) || !isNear(shear_y_value, 0)) {
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
	if (!isNear(source_width_scale, 1.0) || !isNear(source_height_scale, 1.0)) {
		transform.scale(source_width_scale, source_height_scale);
	}

	return transform;
}

// Adjust frame number for Clip position and start (which can result in a different number)
int64_t Clip::adjust_timeline_framenumber(int64_t clip_frame_number) {

	// Get clip position from parent clip (if any)
	float position = 0.0;
	float start = 0.0;
	Clip *parent = static_cast<Clip *>(ParentClip());
	if (parent) {
		position = parent->Position();
		start = parent->Start();
	}

	// Adjust start frame and position based on parent clip.
	// This ensures the same frame # is used by mapped readers and clips,
	// when calculating samples per frame.
	// Thus, this prevents gaps and mismatches in # of samples.
	int64_t clip_start_frame = (start * info.fps.ToDouble()) + 1;
	int64_t clip_start_position = round(position * info.fps.ToDouble()) + 1;
	int64_t frame_number = clip_frame_number + clip_start_position - clip_start_frame;

	return frame_number;
}
