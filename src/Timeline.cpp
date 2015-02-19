/**
 * @file
 * @brief Source file for Timeline class
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

#include "../include/Timeline.h"

using namespace openshot;

// Default Constructor for the timeline (which sets the canvas width and height)
Timeline::Timeline(int width, int height, Fraction fps, int sample_rate, int channels) : is_open(false)
{
	// Init viewport size (curve based, because it can be animated)
	viewport_scale = Keyframe(100.0);
	viewport_x = Keyframe(0.0);
	viewport_y = Keyframe(0.0);

	// Init background color
	color.red = Keyframe(0.0);
	color.green = Keyframe(0.0);
	color.blue = Keyframe(0.0);

	// Init cache
	int64 bytes = height * width * 4 + (44100 * 2 * 4);
	final_cache = Cache(2 * bytes);  // 20 frames, 4 colors of chars, 2 audio channels of 4 byte floats

	// Init FileInfo struct (clear all values)
	info.width = width;
	info.height = height;
	info.fps = fps;
	info.sample_rate = sample_rate;
	info.channels = channels;
	info.video_timebase = fps.Reciprocal();
	info.duration = 60 * 30; // 30 minute default duration
}

// Add an openshot::Clip to the timeline
void Timeline::AddClip(Clip* clip) throw(ReaderClosed)
{
	// All clips must be converted to the frame rate of this timeline,
	// so assign the same frame rate to each clip.
	clip->Reader()->info.fps = info.fps;

	// Add clip to list
	clips.push_back(clip);

	// Sort clips
	SortClips();
}

// Add an effect to the timeline
void Timeline::AddEffect(EffectBase* effect)
{
	// Add effect to list
	effects.push_back(effect);

	// Sort effects
	SortEffects();
}

// Remove an effect from the timeline
void Timeline::RemoveEffect(EffectBase* effect)
{
	effects.remove(effect);
}

// Remove an openshot::Clip to the timeline
void Timeline::RemoveClip(Clip* clip)
{
	clips.remove(clip);
}

// Calculate time of a frame number, based on a framerate
float Timeline::calculate_time(int number, Fraction rate)
{
	// Get float version of fps fraction
	float raw_fps = rate.ToFloat();

	// Return the time (in seconds) of this frame
	return float(number - 1) / raw_fps;
}

// Apply effects to the source frame (if any)
tr1::shared_ptr<Frame> Timeline::apply_effects(tr1::shared_ptr<Frame> frame, int timeline_frame_number, int layer)
{
	// Calculate time of frame
	float requested_time = calculate_time(timeline_frame_number, info.fps);

	// Find Effects at this position and layer
	list<EffectBase*>::iterator effect_itr;
	for (effect_itr=effects.begin(); effect_itr != effects.end(); ++effect_itr)
	{
		// Get clip object from the iterator
		EffectBase *effect = (*effect_itr);

		// Does clip intersect the current requested time
		float effect_duration = effect->End() - effect->Start();
		bool does_effect_intersect = (effect->Position() <= requested_time && effect->Position() + effect_duration >= requested_time && effect->Layer() == layer);

		// Clip is visible
		if (does_effect_intersect)
		{
			// Determine the frame needed for this clip (based on the position on the timeline)
			float time_diff = (requested_time - effect->Position()) + effect->Start();
			int effect_frame_number = round(time_diff * info.fps.ToFloat()) + 1;

			// Apply the effect to this frame
			frame = effect->GetFrame(frame, effect_frame_number);
		}

	} // end effect loop

	// Return modified frame
	return frame;
}

// Process a new layer of video or audio
void Timeline::add_layer(tr1::shared_ptr<Frame> new_frame, Clip* source_clip, int clip_frame_number, int timeline_frame_number)
{
	// Get the clip's frame & image
	tr1::shared_ptr<Frame> source_frame;

	#pragma omp critical (reader_lock)
	source_frame = tr1::shared_ptr<Frame>(source_clip->GetFrame(clip_frame_number));

	// No frame found... so bail
	if (!source_frame)
		return;

	/* Apply effects to the source frame (if any) */
	source_frame = apply_effects(source_frame, timeline_frame_number, source_clip->Layer());

	tr1::shared_ptr<Magick::Image> source_image;

	/* COPY AUDIO - with correct volume */
	if (source_clip->Reader()->info.has_audio)
		for (int channel = 0; channel < source_frame->GetAudioChannelsCount(); channel++)
		{
			float initial_volume = 1.0f;
			float previous_volume = source_clip->volume.GetValue(clip_frame_number - 1); // previous frame's percentage of volume (0 to 1)
			float volume = source_clip->volume.GetValue(clip_frame_number); // percentage of volume (0 to 1)

			// If no ramp needed, set initial volume = clip's volume
			if (isEqual(previous_volume, volume))
				initial_volume = volume;

			// Apply ramp to source frame (if needed)
			if (!isEqual(previous_volume, volume))
				source_frame->ApplyGainRamp(channel, 0, source_frame->GetAudioSamplesCount(), previous_volume, volume);

			// Copy audio samples (and set initial volume).  Mix samples with existing audio samples.  The gains are added together, to
			// be sure to set the gain's correctly, so the sum does not exceed 1.0 (of audio distortion will happen).
			new_frame->AddAudio(false, channel, 0, source_frame->GetAudioSamples(channel), source_frame->GetAudioSamplesCount(), initial_volume);
		}

	/* GET IMAGE DATA - OR GENERATE IT */
	if (!source_clip->Waveform())
		// Get actual frame image data
		source_image = source_frame->GetImage();
	else
	{
		// Get the color of the waveform
		int red = source_clip->wave_color.red.GetInt(timeline_frame_number);
		int green = source_clip->wave_color.green.GetInt(timeline_frame_number);
		int blue = source_clip->wave_color.blue.GetInt(timeline_frame_number);

		// Generate Waveform Dynamically (the size of the timeline)
		source_image = source_frame->GetWaveform(info.width, info.height, red, green, blue);
	}

	// Get some basic image properties
	int source_width = source_image->columns();
	int source_height = source_image->rows();

	/* ALPHA & OPACITY */
	if (source_clip->alpha.GetValue(clip_frame_number) != 0)
	{
		float alpha = 1.0 - source_clip->alpha.GetValue(clip_frame_number);
		source_image->quantumOperator(Magick::OpacityChannel, Magick::MultiplyEvaluateOperator, alpha);
	}

	/* RESIZE SOURCE IMAGE - based on scale type */
	Magick::Geometry new_size(info.width, info.height);
	switch (source_clip->scale)
	{
	case (SCALE_FIT):
		new_size.aspect(false); // respect aspect ratio
		source_image->resize(new_size);
		source_width = source_image->size().width();
		source_height = source_image->size().height();
		break;
	case (SCALE_STRETCH):
		new_size.aspect(true); // ignore aspect ratio
		source_image->resize(new_size);
		source_width = source_image->size().width();
		source_height = source_image->size().height();
		break;
	case (SCALE_CROP):
		Magick::Geometry width_size(info.width, round(info.width / (float(source_width) / float(source_height))));
		Magick::Geometry height_size(round(info.height / (float(source_height) / float(source_width))), info.height);
		new_size.aspect(false); // respect aspect ratio
		if (width_size.width() >= info.width && width_size.height() >= info.height)
			source_image->resize(width_size); // width is larger, so resize to it
		else
			source_image->resize(height_size); // height is larger, so resize to it
		source_width = source_image->size().width();
		source_height = source_image->size().height();
		break;
	}

	/* GRAVITY LOCATION - Initialize X & Y to the correct values (before applying location curves) */
	float x = 0.0; // left
	float y = 0.0; // top
	switch (source_clip->gravity)
	{
	case (GRAVITY_TOP):
		x = (info.width - source_width) / 2.0; // center
		break;
	case (GRAVITY_TOP_RIGHT):
		x = info.width - source_width; // right
		break;
	case (GRAVITY_LEFT):
		y = (info.height - source_height) / 2.0; // center
		break;
	case (GRAVITY_CENTER):
		x = (info.width - source_width) / 2.0; // center
		y = (info.height - source_height) / 2.0; // center
		break;
	case (GRAVITY_RIGHT):
		x = info.width - source_width; // right
		y = (info.height - source_height) / 2.0; // center
		break;
	case (GRAVITY_BOTTOM_LEFT):
		y = (info.height - source_height); // bottom
		break;
	case (GRAVITY_BOTTOM):
		x = (info.width - source_width) / 2.0; // center
		y = (info.height - source_height); // bottom
		break;
	case (GRAVITY_BOTTOM_RIGHT):
		x = info.width - source_width; // right
		y = (info.height - source_height); // bottom
		break;
	}

	/* LOCATION, ROTATION, AND SCALE */
	float r = source_clip->rotation.GetValue(clip_frame_number); // rotate in degrees
	x += info.width * source_clip->location_x.GetValue(clip_frame_number); // move in percentage of final width
	y += info.height * source_clip->location_y.GetValue(clip_frame_number); // move in percentage of final height
	float sx = source_clip->scale_x.GetValue(clip_frame_number); // percentage X scale
	float sy = source_clip->scale_y.GetValue(clip_frame_number); // percentage Y scale
	bool is_x_animated = source_clip->location_x.Points.size() > 2;
	bool is_y_animated = source_clip->location_y.Points.size() > 2;

	int offset_x = -1;
	int offset_y = -1;
	bool transformed = false;
	if ((!isEqual(x, 0) || !isEqual(y, 0)) && (isEqual(r, 0) && isEqual(sx, 1) && isEqual(sy, 1) && !is_x_animated && !is_y_animated))
	{
		//cout << "SIMPLE" << endl;
		// If only X and Y are different, and no animation is being used (just set the offset for speed)
		offset_x = round(x);
		offset_y = round(y);
		transformed = true;

	} else if (!isEqual(r, 0) || !isEqual(x, 0) || !isEqual(y, 0) || !isEqual(sx, 1) || !isEqual(sy, 1))
	{
		//cout << "COMPLEX" << endl;

		/* RESIZE SOURCE CANVAS - to the same size as timeline canvas */
		if (source_width != info.width || source_height != info.height)
		{
			source_image->borderColor(Magick::Color("none"));
			source_image->border(Magick::Geometry(1, 1, 0, 0, false, false)); // prevent stretching of edge pixels (during the canvas resize)
			source_image->size(Magick::Geometry(info.width, info.height, 0, 0, false, false)); // resize the canvas (to prevent clipping)
		}

		// Use the distort operator, which is very CPU intensive
		// origin X,Y     Scale     Angle  NewX,NewY
		double distort_args[7] = {0,0,  sx,sy,  r,  x,y };
		source_image->distort(Magick::ScaleRotateTranslateDistortion, 7, distort_args, false);
		transformed = true;
	}

	/* Is this the 1st layer?  And the same size as this image? */
	if (new_frame->GetImage()->columns() == 1 && !transformed && source_frame->GetHeight() == new_frame->GetHeight() && source_frame->GetWidth() == new_frame->GetWidth())
	{
		// Just use this image as the background
		new_frame->AddImage(source_image);
	}
	else if (new_frame->GetImage()->columns() == 1)
	{
		/* CREATE BACKGROUND COLOR - needed if this is the 1st layer */
		int red = color.red.GetInt(timeline_frame_number);
		int green = color.green.GetInt(timeline_frame_number);
		int blue = color.blue.GetInt(timeline_frame_number);
		new_frame->AddColor(info.width, info.height, Magick::Color((Magick::Quantum)red, (Magick::Quantum)green, (Magick::Quantum)blue, 0));

		/* COMPOSITE SOURCE IMAGE (LAYER) ONTO FINAL IMAGE */
		tr1::shared_ptr<Magick::Image> new_image = new_frame->GetImage();
		new_image->composite(*source_image.get(), offset_x, offset_y, Magick::OverCompositeOp);
	}
	else
	{
		/* COMPOSITE SOURCE IMAGE (LAYER) ONTO FINAL IMAGE */
		tr1::shared_ptr<Magick::Image> new_image = new_frame->GetImage();
		new_image->composite(*source_image.get(), offset_x, offset_y, Magick::OverCompositeOp);
	}

}

// Update the list of 'opened' clips
void Timeline::update_open_clips(Clip *clip, bool is_open)
{
	// is clip already in list?
	bool clip_found = open_clips.count(clip);

	if (clip_found && !is_open)
	{
		// Mark clip "to be removed"
		closing_clips.push_back(clip);
	}
	else if (!clip_found && is_open)
	{
		// Add clip to 'opened' list, because it's missing
		open_clips[clip] = clip;

		// Open the clip's reader
		clip->Open();
	}

	// Debug output
	#pragma omp critical (debug_output)
	AppendDebugMethod("Timeline::update_open_clips", "clip_found", clip_found, "is_open", is_open, "closing_clips.size()", closing_clips.size(), "open_clips.size()", open_clips.size(), "", -1, "", -1);
}

// Update the list of 'closed' clips
void Timeline::update_closed_clips()
{
	// Close all "to be closed" clips
	list<Clip*>::iterator clip_itr;
	for (clip_itr=closing_clips.begin(); clip_itr != closing_clips.end(); ++clip_itr)
	{
		// Get clip object from the iterator
		Clip *clip = (*clip_itr);

		// Close the clip's reader
		clip->Close();

		// Remove clip from 'opened' list, because it's closed now
		open_clips.erase(clip);
	}

	// Clear list
	closing_clips.clear();

	// Debug output
	#pragma omp critical (debug_output)
	AppendDebugMethod("Timeline::update_closed_clips", "closing_clips.size()", closing_clips.size(), "open_clips.size()", open_clips.size(), "", -1, "", -1, "", -1, "", -1);
}

// Sort clips by position on the timeline
void Timeline::SortClips()
{
	// Debug output
	#pragma omp critical (debug_output)
	AppendDebugMethod("Timeline::SortClips", "clips.size()", clips.size(), "", -1, "", -1, "", -1, "", -1, "", -1);

	// sort clips
	clips.sort(CompareClips());
}

// Sort effects by position on the timeline
void Timeline::SortEffects()
{
	// sort clips
	effects.sort(CompareEffects());
}

// Close the reader (and any resources it was consuming)
void Timeline::Close()
{
	// Close all open clips
	list<Clip*>::iterator clip_itr;
	for (clip_itr=clips.begin(); clip_itr != clips.end(); ++clip_itr)
	{
		// Get clip object from the iterator
		Clip *clip = (*clip_itr);

		// Open or Close this clip, based on if it's intersecting or not
		update_open_clips(clip, false);
	}

	// Actually close the clips
	update_closed_clips();
	is_open = false;

	// Clear cache
	final_cache.Clear();
}

// Open the reader (and start consuming resources)
void Timeline::Open()
{
	is_open = true;
}

// Compare 2 floating point numbers for equality
bool Timeline::isEqual(double a, double b)
{
	return fabs(a - b) < 0.000001;
}

// Get an openshot::Frame object for a specific frame number of this reader.
tr1::shared_ptr<Frame> Timeline::GetFrame(int requested_frame) throw(ReaderClosed)
{
	// Adjust out of bounds frame number
	if (requested_frame < 1)
		requested_frame = 1;

	// Check cache
	if (final_cache.Exists(requested_frame)) {
		// Debug output
		#pragma omp critical (debug_output)
		AppendDebugMethod("Timeline::GetFrame (Cached frame found)", "requested_frame", requested_frame, "", -1, "", -1, "", -1, "", -1, "", -1);

		return final_cache.GetFrame(requested_frame);
	}
	else
	{
		// Debug output
		#pragma omp critical (debug_output)
		AppendDebugMethod("Timeline::GetFrame (Generating frame)", "requested_frame", requested_frame, "", -1, "", -1, "", -1, "", -1, "", -1);

		// Minimum number of frames to process (for performance reasons)
		int minimum_frames = OPEN_MP_NUM_PROCESSORS;

		// Set the number of threads in OpenMP
		omp_set_num_threads(OPEN_MP_NUM_PROCESSORS);
		// Allow nested OpenMP sections
		omp_set_nested(true);

		#pragma omp parallel
		{
			#pragma omp single
			{
				// Debug output
				#pragma omp critical (debug_output)
				AppendDebugMethod("Timeline::GetFrame (Loop through frames)", "requested_frame", requested_frame, "minimum_frames", minimum_frames, "", -1, "", -1, "", -1, "", -1);

				// Get a list of clips that intersect with the requested section of timeline
				// This also opens the readers for intersecting clips, and marks non-intersecting clips as 'needs closing'
				list<Clip*> nearby_clips = find_intersecting_clips(requested_frame, minimum_frames, true);

				// Loop through all requested frames
				for (int frame_number = requested_frame; frame_number < requested_frame + minimum_frames; frame_number++)
				{
					#pragma omp task firstprivate(frame_number)
					{
						// Create blank frame (which will become the requested frame)
						tr1::shared_ptr<Frame> new_frame(tr1::shared_ptr<Frame>(new Frame(frame_number, info.width, info.height, "#000000", 0, info.channels)));

						// Calculate time of frame
						float requested_time = calculate_time(frame_number, info.fps);

						// Debug output
						#pragma omp critical (debug_output)
						AppendDebugMethod("Timeline::GetFrame (Loop through clips)", "frame_number", frame_number, "requested_time", requested_time, "clips.size()", clips.size(), "", -1, "", -1, "", -1);

						// Find Clips near this time
						list<Clip*>::iterator clip_itr;
						for (clip_itr=nearby_clips.begin(); clip_itr != nearby_clips.end(); ++clip_itr)
						{
							// Get clip object from the iterator
							Clip *clip = (*clip_itr);

							// Does clip intersect the current requested time
							float clip_duration = clip->End() - clip->Start();
							bool does_clip_intersect = (clip->Position() <= requested_time && clip->Position() + clip_duration >= requested_time);

							// Debug output
							#pragma omp critical (debug_output)
							AppendDebugMethod("Timeline::GetFrame (Does clip intersect)", "frame_number", frame_number, "requested_time", requested_time, "clip->Position()", clip->Position(), "clip_duration", clip_duration, "does_clip_intersect", does_clip_intersect, "", -1);

							// Clip is visible
							if (does_clip_intersect)
							{
								// Determine the frame needed for this clip (based on the position on the timeline)
								float time_diff = (requested_time - clip->Position()) + clip->Start();
								int clip_frame_number = round(time_diff * info.fps.ToFloat()) + 1;

								// Add clip's frame as layer
								add_layer(new_frame, clip, clip_frame_number, frame_number);

							} else
								// Debug output
								#pragma omp critical (debug_output)
								AppendDebugMethod("Timeline::GetFrame (clip does not intersect)", "frame_number", frame_number, "requested_time", requested_time, "does_clip_intersect", does_clip_intersect, "", -1, "", -1, "", -1);

						} // end clip loop

						// Check for empty frame image (and fill with color)
						if (new_frame->GetImage()->columns() == 1)
						{
							int red = color.red.GetInt(frame_number);
							int green = color.green.GetInt(frame_number);
							int blue = color.blue.GetInt(frame_number);
							new_frame->AddColor(info.width, info.height, Magick::Color((Magick::Quantum)red, (Magick::Quantum)green, (Magick::Quantum)blue));
						}

						// Add final frame to cache
						#pragma omp critical (timeline_cache)
						final_cache.Add(frame_number, new_frame);

					} // end omp task
				} // end frame loop

				// Actually close all clips no longer needed
				#pragma omp critical (reader_lock)
				update_closed_clips();

			} // end omp single
		} // end omp parallel

		// Return frame (or blank frame)
		return final_cache.GetFrame(requested_frame);
	}
}


// Find intersecting clips (or non intersecting clips)
list<Clip*> Timeline::find_intersecting_clips(int requested_frame, int number_of_frames, bool include)
{
	// Find matching clips
	list<Clip*> matching_clips;

	// Calculate time of frame
	float min_requested_time = calculate_time(requested_frame, info.fps);
	float max_requested_time = calculate_time(requested_frame + (number_of_frames - 1), info.fps);

	// Re-Sort Clips (since they likely changed)
	SortClips();

	// Find Clips at this time
	list<Clip*>::iterator clip_itr;
	for (clip_itr=clips.begin(); clip_itr != clips.end(); ++clip_itr)
	{
		// Get clip object from the iterator
		Clip *clip = (*clip_itr);

		// Does clip intersect the current requested time
		float clip_duration = clip->End() - clip->Start();
		bool does_clip_intersect = (clip->Position() <= min_requested_time && clip->Position() + clip_duration >= min_requested_time) ||
								   (clip->Position() > min_requested_time && clip->Position() <= max_requested_time);

		// Debug output
		#pragma omp critical (debug_output)
		AppendDebugMethod("Timeline::find_intersecting_clips (Is clip near or intersecting)", "requested_frame", requested_frame, "min_requested_time", min_requested_time, "max_requested_time", max_requested_time, "clip->Position()", clip->Position(), "clip_duration", clip_duration, "does_clip_intersect", does_clip_intersect);

		// Open (or schedule for closing) this clip, based on if it's intersecting or not
		#pragma omp critical (reader_lock)
		update_open_clips(clip, does_clip_intersect);

		// Clip is visible
		if (does_clip_intersect && include)
			// Add the intersecting clip
			matching_clips.push_back(clip);

		else if (!does_clip_intersect && !include)
			// Add the non-intersecting clip
			matching_clips.push_back(clip);

	} // end clip loop

	// return list
	return matching_clips;
}

// Generate JSON string of this object
string Timeline::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Timeline::JsonValue() {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "Timeline";
	root["viewport_scale"] = viewport_scale.JsonValue();
	root["viewport_x"] = viewport_x.JsonValue();
	root["viewport_y"] = viewport_y.JsonValue();
	root["color"] = color.JsonValue();

	// Add array of clips
	root["clips"] = Json::Value(Json::arrayValue);

	// Find Clips at this time
	list<Clip*>::iterator clip_itr;
	for (clip_itr=clips.begin(); clip_itr != clips.end(); ++clip_itr)
	{
		// Get clip object from the iterator
		Clip *existing_clip = (*clip_itr);
		root["clips"].append(existing_clip->JsonValue());
	}

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

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Timeline::SetJson(string value) throw(InvalidJSON) {

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
void Timeline::SetJsonValue(Json::Value root) throw(InvalidFile, ReaderClosed) {

	// Close timeline before we do anything (this also removes all open and closing clips)
	Close();

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Clear existing clips
	clips.clear();

	if (!root["clips"].isNull())
		// loop through clips
		for (int x = 0; x < root["clips"].size(); x++) {
			// Get each clip
			Json::Value existing_clip = root["clips"][x];

			// Create Clip
			Clip *c = new Clip();

			// Load Json into Clip
			c->SetJsonValue(existing_clip);

			// Add Clip to Timeline
			AddClip(c);
		}

	// Clear existing effects
	effects.clear();

	if (!root["effects"].isNull())
		// loop through effects
		for (int x = 0; x < root["effects"].size(); x++) {
			// Get each effect
			Json::Value existing_effect = root["effects"][x];

			// Create Effect
			EffectBase *e = NULL;

			if (!existing_effect["type"].isNull())
				// Init the matching effect object
				if (existing_effect["type"].asString() == "ChromaKey")
					e = new ChromaKey();

				else if (existing_effect["type"].asString() == "Deinterlace")
					e = new Deinterlace();

				else if (existing_effect["type"].asString() == "Mask")
					e = new Mask();

				else if (existing_effect["type"].asString() == "Negate")
					e = new Negate();

			// Load Json into Effect
			e->SetJsonValue(existing_effect);

			// Add Effect to Timeline
			AddEffect(e);
		}
}

// Apply a special formatted JSON object, which represents a change to the timeline (insert, update, delete)
void Timeline::ApplyJsonDiff(string value) throw(InvalidJSON, InvalidJSONKey) {

	// Clear internal cache (since things are about to change)
	final_cache.Clear();

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::Reader reader;
	bool success = reader.parse( value, root );
	if (!success || !root.isArray())
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid).", "");

	try
	{
		// Process the JSON change array, loop through each item
		for (int x = 0; x < root.size(); x++) {
			// Get each change
			Json::Value change = root[x];
			string root_key = change["key"][(uint)0].asString();

			// Process each type of change
			if (root_key == "clips")
				// Apply to CLIPS
				apply_json_to_clips(change);

			else if (root_key == "effects")
				// Apply to EFFECTS
				apply_json_to_effects(change);

			else
				// Apply to TIMELINE
				apply_json_to_timeline(change);

		}
	}
	catch (exception e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)", "");
	}
}

// Apply JSON diff to clips
void Timeline::apply_json_to_clips(Json::Value change) throw(InvalidJSONKey) {

	// Get key and type of change
	string change_type = change["type"].asString();
	string clip_id = "";
	Clip *existing_clip = NULL;

	// Find id of clip (if any)
	for (int x = 0; x < change["key"].size(); x++) {
		// Get each change
		Json::Value key_part = change["key"][x];

		if (key_part.isObject()) {
			// Check for id
			if (!key_part["id"].isNull()) {
				// Set the id
				clip_id = key_part["id"].asString();

				// Find matching clip in timeline (if any)
				list<Clip*>::iterator clip_itr;
				for (clip_itr=clips.begin(); clip_itr != clips.end(); ++clip_itr)
				{
					// Get clip object from the iterator
					Clip *c = (*clip_itr);
					if (c->Id() == clip_id) {
						existing_clip = c;
						break; // clip found, exit loop
					}
				}
				break; // id found, exit loop
			}
		}
	}

	// Determine type of change operation
	if (change_type == "insert") {

		// Create new clip
		Clip *clip = new Clip();
		clip->SetJsonValue(change["value"]); // Set properties of new clip from JSON
		AddClip(clip); // Add clip to timeline

	} else if (change_type == "update") {

		// Update existing clip
		if (existing_clip)
			existing_clip->SetJsonValue(change["value"]); // Update clip properties from JSON

	} else if (change_type == "delete") {

		// Remove existing clip
		if (existing_clip)
			RemoveClip(existing_clip); // Remove clip from timeline

	}

}

// Apply JSON diff to effects
void Timeline::apply_json_to_effects(Json::Value change) throw(InvalidJSONKey) {

	// Get key and type of change
	string change_type = change["type"].asString();
	string effect_id = "";
	EffectBase *existing_effect = NULL;

	// Find id of an effect (if any)
	for (int x = 0; x < change["key"].size(); x++) {
		// Get each change
		Json::Value key_part = change["key"][x];

		if (key_part.isObject()) {
			// Check for id
			if (!key_part["id"].isNull())
			{
				// Set the id
				effect_id = key_part["id"].asString();

				// Find matching effect in timeline (if any)
				list<EffectBase*>::iterator effect_itr;
				for (effect_itr=effects.begin(); effect_itr != effects.end(); ++effect_itr)
				{
					// Get effect object from the iterator
					EffectBase *e = (*effect_itr);
					if (e->Id() == effect_id) {
						existing_effect = e;
						break; // effect found, exit loop
					}
				}
				break; // id found, exit loop
			}
		}
	}

	// Determine type of change operation
	if (change_type == "insert") {

		// Determine type of effect
		string effect_type = change["value"]["type"].asString();

		// Create Effect
		EffectBase *e = NULL;

		// Init the matching effect object
		if (effect_type == "ChromaKey")
			e = new ChromaKey();

		else if (effect_type == "Deinterlace")
			e = new Deinterlace();

		else if (effect_type == "Mask")
			e = new Mask();

		else if (effect_type == "Negate")
			e = new Negate();

		// Load Json into Effect
		e->SetJsonValue(change["value"]);

		// Add Effect to Timeline
		AddEffect(e);

	} else if (change_type == "update") {

		// Update existing effect
		if (existing_effect)
			existing_effect->SetJsonValue(change["value"]); // Update effect properties from JSON

	} else if (change_type == "delete") {

		// Remove existing effect
		if (existing_effect)
			RemoveEffect(existing_effect); // Remove effect from timeline

	}

}

// Apply JSON diff to timeline properties
void Timeline::apply_json_to_timeline(Json::Value change) throw(InvalidJSONKey) {

	// Get key and type of change
	string change_type = change["type"].asString();
	string root_key = change["key"][(uint)0].asString();

	// Determine type of change operation
	if (change_type == "insert" || change_type == "update") {

		// INSERT / UPDATE
		// Check for valid property
		if (root_key == "color")
			// Set color
			color.SetJsonValue(change["value"]);
		else if (root_key == "viewport_scale")
			// Set viewport scale
			viewport_scale.SetJsonValue(change["value"]);
		else if (root_key == "viewport_x")
			// Set viewport x offset
			viewport_x.SetJsonValue(change["value"]);
		else if (root_key == "viewport_y")
			// Set viewport y offset
			viewport_y.SetJsonValue(change["value"]);
		else
			// Error parsing JSON (or missing keys)
			throw InvalidJSONKey("JSON change key is invalid", change.toStyledString());


	} else if (change["type"].asString() == "delete") {

		// DELETE / RESET
		// Reset the following properties (since we can't delete them)
		if (root_key == "color") {
			color = Color();
			color.red = Keyframe(0.0);
			color.green = Keyframe(0.0);
			color.blue = Keyframe(0.0);
		}
		else if (root_key == "viewport_scale")
			viewport_scale = Keyframe(1.0);
		else if (root_key == "viewport_x")
			viewport_x = Keyframe(0.0);
		else if (root_key == "viewport_y")
			viewport_y = Keyframe(0.0);
		else
			// Error parsing JSON (or missing keys)
			throw InvalidJSONKey("JSON change key is invalid", change.toStyledString());

	}

}




