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
Timeline::Timeline(int width, int height, Fraction fps, int sample_rate, int channels, ChannelLayout channel_layout) :
		is_open(false), auto_map_clips(true)
{
	// Init viewport size (curve based, because it can be animated)
	viewport_scale = Keyframe(100.0);
	viewport_x = Keyframe(0.0);
	viewport_y = Keyframe(0.0);

	// Init background color
	color.red = Keyframe(0.0);
	color.green = Keyframe(0.0);
	color.blue = Keyframe(0.0);

	// Init FileInfo struct (clear all values)
	info.width = width;
	info.height = height;
	info.fps = fps;
	info.sample_rate = sample_rate;
	info.channels = channels;
	info.channel_layout = channel_layout;
	info.video_timebase = fps.Reciprocal();
	info.duration = 60 * 30; // 30 minute default duration
	info.has_audio = true;
	info.has_video = true;
	info.video_length = info.fps.ToFloat() * info.duration;

	// Init cache
	final_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);
}

// Add an openshot::Clip to the timeline
void Timeline::AddClip(Clip* clip) throw(ReaderClosed)
{
	// All clips should be converted to the frame rate of this timeline
	if (auto_map_clips)
		// Apply framemapper (or update existing framemapper)
		apply_mapper_to_clip(clip);

	// Add clip to list
	clips.push_back(clip);

	// Sort clips
	sort_clips();
}

// Add an effect to the timeline
void Timeline::AddEffect(EffectBase* effect)
{
	// Add effect to list
	effects.push_back(effect);

	// Sort effects
	sort_effects();
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

// Apply a FrameMapper to a clip which matches the settings of this timeline
void Timeline::apply_mapper_to_clip(Clip* clip)
{
	// Determine type of reader
	ReaderBase* clip_reader = NULL;
	if (clip->Reader()->Name() == "FrameMapper")
	{
		// Get the existing reader
		clip_reader = (ReaderBase*) clip->Reader();

	} else {

		// Create a new FrameMapper to wrap the current reader
		clip_reader = (ReaderBase*) new FrameMapper(clip->Reader(), info.fps, PULLDOWN_NONE, info.sample_rate, info.channels, info.channel_layout);
	}

	// Update the mapping
	FrameMapper* clip_mapped_reader = (FrameMapper*) clip_reader;
	clip_mapped_reader->ChangeMapping(info.fps, PULLDOWN_NONE, info.sample_rate, info.channels, info.channel_layout);

	// Update clip reader
	clip->Reader(clip_reader);
}

// Apply the timeline's framerate and samplerate to all clips
void Timeline::ApplyMapperToClips()
{
	// Clear all cached frames
	final_cache.Clear();

	// Loop through all clips
	list<Clip*>::iterator clip_itr;
	for (clip_itr=clips.begin(); clip_itr != clips.end(); ++clip_itr)
	{
		// Get clip object from the iterator
		Clip *clip = (*clip_itr);

		// Apply framemapper (or update existing framemapper)
		apply_mapper_to_clip(clip);
	}
}

// Calculate time of a frame number, based on a framerate
float Timeline::calculate_time(long int number, Fraction rate)
{
	// Get float version of fps fraction
	float raw_fps = rate.ToFloat();

	// Return the time (in seconds) of this frame
	return float(number - 1) / raw_fps;
}

// Apply effects to the source frame (if any)
tr1::shared_ptr<Frame> Timeline::apply_effects(tr1::shared_ptr<Frame> frame, long int timeline_frame_number, int layer)
{
	// Calculate time of frame
	float requested_time = calculate_time(timeline_frame_number, info.fps);

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::apply_effects", "requested_time", requested_time, "frame->number", frame->number, "timeline_frame_number", timeline_frame_number, "layer", layer, "", -1, "", -1);

	// Find Effects at this position and layer
	list<EffectBase*>::iterator effect_itr;
	for (effect_itr=effects.begin(); effect_itr != effects.end(); ++effect_itr)
	{
		// Get effect object from the iterator
		EffectBase *effect = (*effect_itr);

		// Does clip intersect the current requested time
		float effect_duration = effect->End() - effect->Start();
		bool does_effect_intersect = (effect->Position() <= requested_time && effect->Position() + effect_duration >= requested_time && effect->Layer() == layer);

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::apply_effects (Does effect intersect)", "effect->Position()", effect->Position(), "requested_time", requested_time, "does_effect_intersect", does_effect_intersect, "timeline_frame_number", timeline_frame_number, "layer", layer, "effect_duration", effect_duration);

		// Clip is visible
		if (does_effect_intersect)
		{
			// Determine the frame needed for this clip (based on the position on the timeline)
			float time_diff = (requested_time - effect->Position()) + effect->Start();
			int effect_frame_number = round(time_diff * info.fps.ToFloat()) + 1;

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("Timeline::apply_effects (Process Effect)", "time_diff", time_diff, "effect_frame_number", effect_frame_number, "effect_duration", effect_duration, "does_effect_intersect", does_effect_intersect, "", -1, "", -1);

			// Apply the effect to this frame
			frame = effect->GetFrame(frame, effect_frame_number);
		}

	} // end effect loop

	// Return modified frame
	return frame;
}

// Get or generate a blank frame
tr1::shared_ptr<Frame> Timeline::GetOrCreateFrame(Clip* clip, long int number)
{
	tr1::shared_ptr<Frame> new_frame;

	// Init some basic properties about this frame
	int samples_in_frame = Frame::GetSamplesPerFrame(number, info.fps, info.sample_rate, info.channels);

	try {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetOrCreateFrame (from reader)", "number", number, "samples_in_frame", samples_in_frame, "", -1, "", -1, "", -1, "", -1);

		// Attempt to get a frame (but this could fail if a reader has just been closed)
		new_frame = tr1::shared_ptr<Frame>(clip->GetFrame(number));

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
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetOrCreateFrame (create blank)", "number", number, "samples_in_frame", samples_in_frame, "", -1, "", -1, "", -1, "", -1);

	// Create blank frame
	new_frame = tr1::shared_ptr<Frame>(new Frame(number, info.width, info.height, "#000000", samples_in_frame, info.channels));
	new_frame->SampleRate(info.sample_rate);
	new_frame->ChannelsLayout(info.channel_layout);
	return new_frame;
}

// Process a new layer of video or audio
void Timeline::add_layer(tr1::shared_ptr<Frame> new_frame, Clip* source_clip, long int clip_frame_number, long int timeline_frame_number, bool is_top_clip)
{
	// Get the clip's frame & image
	tr1::shared_ptr<Frame> source_frame = GetOrCreateFrame(source_clip, clip_frame_number);

	// No frame found... so bail
	if (!source_frame)
		return;

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer", "new_frame->number", new_frame->number, "clip_frame_number", clip_frame_number, "timeline_frame_number", timeline_frame_number, "", -1, "", -1, "", -1);

	/* REPLACE IMAGE WITH WAVEFORM IMAGE (IF NEEDED) */
	if (source_clip->Waveform())
	{
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Generate Waveform Image)", "source_frame->number", source_frame->number, "source_clip->Waveform()", source_clip->Waveform(), "clip_frame_number", clip_frame_number, "", -1, "", -1, "", -1);

		// Get the color of the waveform
		int red = source_clip->wave_color.red.GetInt(clip_frame_number);
		int green = source_clip->wave_color.green.GetInt(clip_frame_number);
		int blue = source_clip->wave_color.blue.GetInt(clip_frame_number);
		int alpha = source_clip->wave_color.alpha.GetInt(clip_frame_number);

		// Generate Waveform Dynamically (the size of the timeline)
		tr1::shared_ptr<QImage> source_image = source_frame->GetWaveform(info.width, info.height, red, green, blue, alpha);
		source_frame->AddImage(tr1::shared_ptr<QImage>(source_image));
	}

	/* Apply effects to the source frame (if any). If multiple clips are overlapping, only process the
	 * effects on the top clip. */
	if (is_top_clip)
		source_frame = apply_effects(source_frame, timeline_frame_number, source_clip->Layer());

	// Declare an image to hold the source frame's image
	tr1::shared_ptr<QImage> source_image;

	/* COPY AUDIO - with correct volume */
	if (source_clip->Reader()->info.has_audio) {

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Copy Audio)", "source_clip->Reader()->info.has_audio", source_clip->Reader()->info.has_audio, "source_frame->GetAudioChannelsCount()", source_frame->GetAudioChannelsCount(), "info.channels", info.channels, "clip_frame_number", clip_frame_number, "timeline_frame_number", timeline_frame_number, "", -1);

		if (source_frame->GetAudioChannelsCount() == info.channels)
			for (int channel = 0; channel < source_frame->GetAudioChannelsCount(); channel++)
			{
				float initial_volume = 1.0f;
				float previous_volume = source_clip->volume.GetValue(clip_frame_number - 1); // previous frame's percentage of volume (0 to 1)
				float volume = source_clip->volume.GetValue(clip_frame_number); // percentage of volume (0 to 1)
				int channel_filter = source_clip->channel_filter.GetInt(clip_frame_number); // optional channel to filter (if not -1)
				int channel_mapping = source_clip->channel_mapping.GetInt(clip_frame_number); // optional channel to map this channel to (if not -1)

				// If channel filter enabled, check for correct channel (and skip non-matching channels)
				if (channel_filter != -1 && channel_filter != channel)
					continue; // skip to next channel

				// If channel mapping disabled, just use the current channel
				if (channel_mapping == -1)
					channel_mapping = channel;

				// If no ramp needed, set initial volume = clip's volume
				if (isEqual(previous_volume, volume))
					initial_volume = volume;

				// Apply ramp to source frame (if needed)
				if (!isEqual(previous_volume, volume))
					source_frame->ApplyGainRamp(channel_mapping, 0, source_frame->GetAudioSamplesCount(), previous_volume, volume);

				// TODO: Improve FrameMapper (or Timeline) to always get the correct number of samples per frame.
				// Currently, the ResampleContext sometimes leaves behind a few samples for the next call, and the
				// number of samples returned is variable... and does not match the number expected.
				// This is a crude solution at best. =)
				if (new_frame->GetAudioSamplesCount() != source_frame->GetAudioSamplesCount())
					// Force timeline frame to match the source frame
					new_frame->ResizeAudio(info.channels, source_frame->GetAudioSamplesCount(), info.sample_rate, info.channel_layout);

				// Copy audio samples (and set initial volume).  Mix samples with existing audio samples.  The gains are added together, to
				// be sure to set the gain's correctly, so the sum does not exceed 1.0 (of audio distortion will happen).
				new_frame->AddAudio(false, channel_mapping, 0, source_frame->GetAudioSamples(channel), source_frame->GetAudioSamplesCount(), initial_volume);

			}
		else
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (No Audio Copied - Wrong # of Channels)", "source_clip->Reader()->info.has_audio", source_clip->Reader()->info.has_audio, "source_frame->GetAudioChannelsCount()", source_frame->GetAudioChannelsCount(), "info.channels", info.channels, "clip_frame_number", clip_frame_number, "timeline_frame_number", timeline_frame_number, "", -1);

	}

	// Skip out if only an audio frame
	if (!source_clip->Waveform() && !source_clip->Reader()->info.has_video)
		// Skip the rest of the image processing for performance reasons
		return;

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Get Source Image)", "source_frame->number", source_frame->number, "source_clip->Waveform()", source_clip->Waveform(), "clip_frame_number", clip_frame_number, "", -1, "", -1, "", -1);

	// Get actual frame image data
	source_image = source_frame->GetImage();

	// Get some basic image properties
	int source_width = source_image->width();
	int source_height = source_image->height();

	/* ALPHA & OPACITY */
	if (source_clip->alpha.GetValue(clip_frame_number) != 1.0)
	{
		float alpha = source_clip->alpha.GetValue(clip_frame_number);

		// Get source image's pixels
		unsigned char *pixels = (unsigned char *) source_image->bits();

		// Loop through pixels
		for (int pixel = 0, byte_index=0; pixel < source_image->width() * source_image->height(); pixel++, byte_index+=4)
		{
			// Get the alpha values from the pixel
			int A = pixels[byte_index + 3];

			// Apply alpha to pixel
			pixels[byte_index + 3] *= alpha;
		}

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Set Alpha & Opacity)", "alpha", alpha, "source_frame->number", source_frame->number, "clip_frame_number", clip_frame_number, "", -1, "", -1, "", -1);
	}

	/* RESIZE SOURCE IMAGE - based on scale type */
	switch (source_clip->scale)
	{
	case (SCALE_FIT):
		// keep aspect ratio
		source_image = tr1::shared_ptr<QImage>(new QImage(source_image->scaled(info.width, info.height, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
		source_width = source_image->width();
		source_height = source_image->height();

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Scale: SCALE_FIT)", "source_frame->number", source_frame->number, "source_width", source_width, "source_height", source_height, "", -1, "", -1, "", -1);
		break;

	case (SCALE_STRETCH):
		// ignore aspect ratio
		source_image = tr1::shared_ptr<QImage>(new QImage(source_image->scaled(info.width, info.height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
		source_width = source_image->width();
		source_height = source_image->height();

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Scale: SCALE_STRETCH)", "source_frame->number", source_frame->number, "source_width", source_width, "source_height", source_height, "", -1, "", -1, "", -1);
		break;

	case (SCALE_CROP):
		QSize width_size(info.width, round(info.width / (float(source_width) / float(source_height))));
		QSize height_size(round(info.height / (float(source_height) / float(source_width))), info.height);

		// respect aspect ratio
		if (width_size.width() >= info.width && width_size.height() >= info.height)
			source_image = tr1::shared_ptr<QImage>(new QImage(source_image->scaled(width_size.width(), width_size.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
		else
			source_image = tr1::shared_ptr<QImage>(new QImage(source_image->scaled(height_size.width(), height_size.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation))); // height is larger, so resize to it
		source_width = source_image->width();
		source_height = source_image->height();

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Scale: SCALE_CROP)", "source_frame->number", source_frame->number, "source_width", source_width, "source_height", source_height, "", -1, "", -1, "", -1);
		break;
	}

	/* GRAVITY LOCATION - Initialize X & Y to the correct values (before applying location curves) */
	float x = 0.0; // left
	float y = 0.0; // top

	// Adjust size for scale x and scale y
	float sx = source_clip->scale_x.GetValue(clip_frame_number); // percentage X scale
	float sy = source_clip->scale_y.GetValue(clip_frame_number); // percentage Y scale
	float scaled_source_width = source_width * sx;
	float scaled_source_height = source_height * sy;

	switch (source_clip->gravity)
	{
	case (GRAVITY_TOP):
		x = (info.width - scaled_source_width) / 2.0; // center
		break;
	case (GRAVITY_TOP_RIGHT):
		x = info.width - scaled_source_width; // right
		break;
	case (GRAVITY_LEFT):
		y = (info.height - scaled_source_height) / 2.0; // center
		break;
	case (GRAVITY_CENTER):
		x = (info.width - scaled_source_width) / 2.0; // center
		y = (info.height - scaled_source_height) / 2.0; // center
		break;
	case (GRAVITY_RIGHT):
		x = info.width - scaled_source_width; // right
		y = (info.height - scaled_source_height) / 2.0; // center
		break;
	case (GRAVITY_BOTTOM_LEFT):
		y = (info.height - scaled_source_height); // bottom
		break;
	case (GRAVITY_BOTTOM):
		x = (info.width - scaled_source_width) / 2.0; // center
		y = (info.height - scaled_source_height); // bottom
		break;
	case (GRAVITY_BOTTOM_RIGHT):
		x = info.width - scaled_source_width; // right
		y = (info.height - scaled_source_height); // bottom
		break;
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Gravity)", "source_frame->number", source_frame->number, "source_clip->gravity", source_clip->gravity, "info.width", info.width, "source_width", source_width, "info.height", info.height, "source_height", source_height);

	/* LOCATION, ROTATION, AND SCALE */
	float r = source_clip->rotation.GetValue(clip_frame_number); // rotate in degrees
	x += (info.width * source_clip->location_x.GetValue(clip_frame_number)); // move in percentage of final width
	y += (info.height * source_clip->location_y.GetValue(clip_frame_number)); // move in percentage of final height
	bool is_x_animated = source_clip->location_x.Points.size() > 1;
	bool is_y_animated = source_clip->location_y.Points.size() > 1;

	int offset_x = -1;
	int offset_y = -1;
	bool transformed = false;
	QTransform transform;
	if ((!isEqual(x, 0) || !isEqual(y, 0)) && (isEqual(r, 0) && isEqual(sx, 1) && isEqual(sy, 1) && !is_x_animated && !is_y_animated))
	{
		// SIMPLE OFFSET
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Transform: SIMPLE)", "source_frame->number", source_frame->number, "x", x, "y", y, "r", r, "sx", sx, "sy", sy);

		// If only X and Y are different, and no animation is being used (just set the offset for speed)
		transformed = true;

		// Set QTransform
		transform.translate(x, y);

	} else if (!isEqual(r, 0) || !isEqual(x, 0) || !isEqual(y, 0) || !isEqual(sx, 1) || !isEqual(sy, 1))
	{
		// COMPLEX DISTORTION
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Transform: COMPLEX)", "source_frame->number", source_frame->number, "x", x, "y", y, "r", r, "sx", sx, "sy", sy);

		// Use the QTransform object, which can be very CPU intensive
		transformed = true;

		// Set QTransform
		if (!isEqual(r, 0)) {
			// ROTATE CLIP
			float origin_x = x + (source_width / 2.0);
			float origin_y = y + (source_height / 2.0);
			transform.translate(origin_x, origin_y);
			transform.rotate(r);
			transform.translate(-origin_x,-origin_y);
		}

		// Set QTransform
		if (!isEqual(x, 0) || !isEqual(y, 0)) {
			// TRANSLATE/MOVE CLIP
			transform.translate(x, y);
		}

		if (!isEqual(sx, 0) || !isEqual(sy, 0)) {
			// TRANSLATE/MOVE CLIP
			transform.scale(sx, sy);
		}

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Transform: COMPLEX: Completed ScaleRotateTranslateDistortion)", "source_frame->number", source_frame->number, "x", x, "y", y, "r", r, "sx", sx, "sy", sy);
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Transform: Composite Image Layer: Prepare)", "source_frame->number", source_frame->number, "offset_x", offset_x, "offset_y", offset_y, "new_frame->GetImage()->width()", new_frame->GetImage()->width(), "transformed", transformed, "", -1);

	/* COMPOSITE SOURCE IMAGE (LAYER) ONTO FINAL IMAGE */
	tr1::shared_ptr<QImage> new_image = new_frame->GetImage();

	// Load timeline's new frame image into a QPainter
	QPainter painter(new_image.get());
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing, true);

	// Apply transform (translate, rotate, scale)... if any
	if (transformed)
		painter.setTransform(transform);

	// Composite a new layer onto the image
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, *source_image);
    painter.end();

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Transform: Composite Image Layer: Completed)", "source_frame->number", source_frame->number, "offset_x", offset_x, "offset_y", offset_y, "new_frame->GetImage()->width()", new_frame->GetImage()->width(), "transformed", transformed, "", -1);
}

// Update the list of 'opened' clips
void Timeline::update_open_clips(Clip *clip, bool does_clip_intersect)
{
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::update_open_clips (before)", "does_clip_intersect", does_clip_intersect, "closing_clips.size()", closing_clips.size(), "open_clips.size()", open_clips.size(), "", -1, "", -1, "", -1);

	// is clip already in list?
	bool clip_found = open_clips.count(clip);

	if (clip_found && !does_clip_intersect)
	{
		// Remove clip from 'opened' list, because it's closed now
		open_clips.erase(clip);

		// Close clip
		clip->Close();
	}
	else if (!clip_found && does_clip_intersect)
	{
		// Add clip to 'opened' list, because it's missing
		open_clips[clip] = clip;

		// Open the clip
		clip->Open();
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::update_open_clips (after)", "does_clip_intersect", does_clip_intersect, "clip_found", clip_found, "closing_clips.size()", closing_clips.size(), "open_clips.size()", open_clips.size(), "", -1, "", -1);
}

// Sort clips by position on the timeline
void Timeline::sort_clips()
{
	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::SortClips", "clips.size()", clips.size(), "", -1, "", -1, "", -1, "", -1, "", -1);

	// sort clips
	clips.sort(CompareClips());
}

// Sort effects by position on the timeline
void Timeline::sort_effects()
{
	// sort clips
	effects.sort(CompareEffects());
}

// Close the reader (and any resources it was consuming)
void Timeline::Close()
{
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::Close", "", -1, "", -1, "", -1, "", -1, "", -1, "", -1);

	// Close all open clips
	list<Clip*>::iterator clip_itr;
	for (clip_itr=clips.begin(); clip_itr != clips.end(); ++clip_itr)
	{
		// Get clip object from the iterator
		Clip *clip = (*clip_itr);

		// Open or Close this clip, based on if it's intersecting or not
		update_open_clips(clip, false);
	}

	// Mark timeline as closed
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
tr1::shared_ptr<Frame> Timeline::GetFrame(long int requested_frame) throw(ReaderClosed, OutOfBoundsFrame)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw ReaderClosed("The Timeline is closed.  Call Open() before calling this method.", "");

	// Adjust out of bounds frame number
	if (requested_frame < 1)
		requested_frame = 1;

	// Check cache
	tr1::shared_ptr<Frame> frame = final_cache.GetFrame(requested_frame);
	if (frame) {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Cached frame found)", "requested_frame", requested_frame, "", -1, "", -1, "", -1, "", -1, "", -1);

		// Return cached frame
		return frame;
	}
	else
	{
		// Create a scoped lock, allowing only a single thread to run the following code at one time
		const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

		// Check cache again (due to locking)
		frame = final_cache.GetFrame(requested_frame);
		if (frame) {
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Cached frame found on 2nd look)", "requested_frame", requested_frame, "", -1, "", -1, "", -1, "", -1, "", -1);

			// Return cached frame
			return frame;
		}

		// Minimum number of frames to process (for performance reasons)
		int minimum_frames = OPEN_MP_NUM_PROCESSORS;

		// Get a list of clips that intersect with the requested section of timeline
		// This also opens the readers for intersecting clips, and marks non-intersecting clips as 'needs closing'
		vector<Clip*> nearby_clips = find_intersecting_clips(requested_frame, minimum_frames, true);

		omp_set_num_threads(OPEN_MP_NUM_PROCESSORS);
		// Allow nested OpenMP sections
		omp_set_nested(true);

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame", "requested_frame", requested_frame, "minimum_frames", minimum_frames, "OPEN_MP_NUM_PROCESSORS", OPEN_MP_NUM_PROCESSORS, "", -1, "", -1, "", -1);

		// GENERATE CACHE FOR CLIPS (IN FRAME # SEQUENCE)
		// Determine all clip frames, and request them in order (to keep resampled audio in sequence)
		for (long int frame_number = requested_frame; frame_number < requested_frame + minimum_frames; frame_number++)
		{
			// Calculate time of timeline frame
			float requested_time = calculate_time(frame_number, info.fps);
			// Loop through clips
			for (int clip_index = 0; clip_index < nearby_clips.size(); clip_index++)
			{
				// Get clip object from the iterator
				Clip *clip = nearby_clips[clip_index];
				bool does_clip_intersect = (clip->Position() <= requested_time && clip->Position() + clip->Duration() >= requested_time);
				if (does_clip_intersect)
				{
					// Get clip frame #
					float time_diff = (requested_time - clip->Position()) + clip->Start();
					int clip_frame_number = round(time_diff * info.fps.ToFloat()) + 1;
					// Cache clip object
					clip->GetFrame(clip_frame_number);
				}
			}
		}

		#pragma omp parallel
		{
			// Loop through all requested frames
			#pragma omp for ordered firstprivate(nearby_clips, requested_frame, minimum_frames)
			for (long int frame_number = requested_frame; frame_number < requested_frame + minimum_frames; frame_number++)
			{
				// Debug output
				ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (processing frame)", "frame_number", frame_number, "omp_get_thread_num()", omp_get_thread_num(), "", -1, "", -1, "", -1, "", -1);

				// Init some basic properties about this frame
				int samples_in_frame = Frame::GetSamplesPerFrame(frame_number, info.fps, info.sample_rate, info.channels);

				// Create blank frame (which will become the requested frame)
				tr1::shared_ptr<Frame> new_frame(tr1::shared_ptr<Frame>(new Frame(frame_number, info.width, info.height, "#000000", samples_in_frame, info.channels)));
				new_frame->SampleRate(info.sample_rate);
				new_frame->ChannelsLayout(info.channel_layout);

				// Debug output
				ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Adding solid color)", "frame_number", frame_number, "info.width", info.width, "info.height", info.height, "", -1, "", -1, "", -1);

				// Add Background Color to 1st layer (if animated or not black)
				if ((color.red.Points.size() > 1 || color.green.Points.size() > 1 || color.blue.Points.size() > 1) ||
					(color.red.GetValue(frame_number) != 0.0 || color.green.GetValue(frame_number) != 0.0 || color.blue.GetValue(frame_number) != 0.0))
				new_frame->AddColor(info.width, info.height, color.GetColorHex(frame_number));

				// Calculate time of frame
				float requested_time = calculate_time(frame_number, info.fps);

				// Debug output
				ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Loop through clips)", "frame_number", frame_number, "requested_time", requested_time, "clips.size()", clips.size(), "nearby_clips.size()", nearby_clips.size(), "", -1, "", -1);

				// Find Clips near this time
				for (int clip_index = 0; clip_index < nearby_clips.size(); clip_index++)
				{
					// Get clip object from the iterator
					Clip *clip = nearby_clips[clip_index];

					// Does clip intersect the current requested time
					bool does_clip_intersect = (clip->Position() <= requested_time && clip->Position() + clip->Duration() >= requested_time);

					// Debug output
					ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Does clip intersect)", "frame_number", frame_number, "requested_time", requested_time, "clip->Position()", clip->Position(), "clip->Duration()", clip->Duration(), "does_clip_intersect", does_clip_intersect, "", -1);

					// Clip is visible
					if (does_clip_intersect)
					{
						// Determine if clip is "top" clip on this layer (only happens when multiple clips are overlapping)
						bool is_top_clip = true;
						for (int top_clip_index = 0; top_clip_index < nearby_clips.size(); top_clip_index++)
						{
							Clip *nearby_clip = nearby_clips[top_clip_index];
							if (clip->Id() != nearby_clip->Id() && clip->Layer() == nearby_clip->Layer() &&
									nearby_clip->Position() <= requested_time && nearby_clip->Position() + nearby_clip->Duration() >= requested_time &&
									nearby_clip->Position() > clip->Position()) {
								is_top_clip = false;
								break;
							}
						}

						// Determine the frame needed for this clip (based on the position on the timeline)
						float time_diff = (requested_time - clip->Position()) + clip->Start();
						int clip_frame_number = round(time_diff * info.fps.ToFloat()) + 1;

						// Debug output
						ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Calculate clip's frame #)", "time_diff", time_diff, "requested_time", requested_time, "clip->Position()", clip->Position(), "clip->Start()", clip->Start(), "info.fps.ToFloat()", info.fps.ToFloat(), "clip_frame_number", clip_frame_number);

						// Add clip's frame as layer
						add_layer(new_frame, clip, clip_frame_number, frame_number, is_top_clip);

					} else
						// Debug output
						ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (clip does not intersect)", "frame_number", frame_number, "requested_time", requested_time, "does_clip_intersect", does_clip_intersect, "", -1, "", -1, "", -1);

				} // end clip loop

				// Debug output
				ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Add frame to cache)", "frame_number", frame_number, "info.width", info.width, "info.height", info.height, "", -1, "", -1, "", -1);

				// Add final frame to cache
				final_cache.Add(frame_number, new_frame);

			} // end frame loop
		} // end parallel

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (end parallel region)", "requested_frame", requested_frame, "omp_get_thread_num()", omp_get_thread_num(), "", -1, "", -1, "", -1, "", -1);

		// Return frame (or blank frame)
		return final_cache.GetFrame(requested_frame);
	}
}


// Find intersecting clips (or non intersecting clips)
vector<Clip*> Timeline::find_intersecting_clips(long int requested_frame, int number_of_frames, bool include)
{
	// Find matching clips
	vector<Clip*> matching_clips;

	// Calculate time of frame
	float min_requested_time = calculate_time(requested_frame, info.fps);
	float max_requested_time = calculate_time(requested_frame + (number_of_frames - 1), info.fps);

	// Re-Sort Clips (since they likely changed)
	sort_clips();

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
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::find_intersecting_clips (Is clip near or intersecting)", "requested_frame", requested_frame, "min_requested_time", min_requested_time, "max_requested_time", max_requested_time, "clip->Position()", clip->Position(), "clip_duration", clip_duration, "does_clip_intersect", does_clip_intersect);

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

	if (!root["clips"].isNull()) {
		// Clear existing clips
		clips.clear();

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
	}

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

	if (!root["duration"].isNull()) {
		// Update duration of timeline
		info.duration = root["duration"].asDouble();
		info.video_length = info.fps.ToFloat() * info.duration;
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

	// Adjust cache (in case something changed)
	final_cache.SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);
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

	// Check for a more specific key (targetting this clip's effects)
	// For example: ["clips", {"id:123}, "effects", {"id":432}]
	if (existing_clip && change["key"].size() == 4 && change["key"][2] == "effects")
	{
		// This change is actually targetting a specific effect under a clip (and not the clip)
		Json::Value key_part = change["key"][3];

		if (key_part.isObject()) {
			// Check for id
			if (!key_part["id"].isNull())
			{
				// Set the id
				string effect_id = key_part["id"].asString();

				// Find matching effect in timeline (if any)
				list<EffectBase*> effect_list = existing_clip->Effects();
				list<EffectBase*>::iterator effect_itr;
				for (effect_itr=effect_list.begin(); effect_itr != effect_list.end(); ++effect_itr)
				{
					// Get effect object from the iterator
					EffectBase *e = (*effect_itr);
					if (e->Id() == effect_id) {
						// Apply the change to the effect directly
						apply_json_to_effects(change, e);
						return; // effect found, don't update clip
					}
				}
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
				string effect_id = key_part["id"].asString();

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

	// Now that we found the effect, apply the change to it
	if (existing_effect || change_type == "insert")
		// Apply change to effect
		apply_json_to_effects(change, existing_effect);
}

// Apply JSON diff to effects (if you already know which effect needs to be updated)
void Timeline::apply_json_to_effects(Json::Value change, EffectBase* existing_effect) throw(InvalidJSONKey) {

	// Get key and type of change
	string change_type = change["type"].asString();

	// Determine type of change operation
	if (change_type == "insert") {

		// Determine type of effect
		string effect_type = change["value"]["type"].asString();

		// Create Effect
		EffectBase *e = NULL;

		// Init the matching effect object
		e = EffectInfo().CreateEffect(effect_type);

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
	string sub_key = "";
	if (change["key"].size() >= 2)
		sub_key = change["key"][(uint)1].asString();

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
		else if (root_key == "duration") {
			// Update duration of timeline
			info.duration = change["value"].asDouble();
			info.video_length = info.fps.ToFloat() * info.duration;
		}
		else if (root_key == "width")
			// Set width
			info.width = change["value"].asInt();
		else if (root_key == "height")
			// Set height
			info.height = change["value"].asInt();
		else if (root_key == "fps" && sub_key == "" && change["value"].isObject()) {
			// Set fps fraction
			if (!change["value"]["num"].isNull())
				info.fps.num = change["value"]["num"].asInt();
			if (!change["value"]["den"].isNull())
				info.fps.den = change["value"]["den"].asInt();
		}
		else if (root_key == "fps" && sub_key == "num")
			// Set fps.num
			info.fps.num = change["value"].asInt();
		else if (root_key == "fps" && sub_key == "den")
			// Set fps.den
			info.fps.den = change["value"].asInt();
		else if (root_key == "sample_rate")
			// Set sample rate
			info.sample_rate = change["value"].asInt();
		else if (root_key == "channels")
			// Set channels
			info.channels = change["value"].asInt();
		else if (root_key == "channel_layout")
			// Set channel layout
			info.channel_layout = (ChannelLayout) change["value"].asInt();

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




