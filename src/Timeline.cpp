#include "../include/Timeline.h"

using namespace openshot;

// Default Constructor for the timeline (which sets the canvas width and height)
Timeline::Timeline(int width, int height, Framerate fps, int sample_rate, int channels) :
		width(width), height(height), fps(fps), sample_rate(sample_rate), channels(channels)
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
	InitFileInfo();
	info.width = width;
	info.height = height;
	info.fps = fps.GetFraction();
	info.sample_rate = sample_rate;
	info.channels = channels;
	info.video_timebase = fps.GetFraction().Reciprocal();
}

// Add an openshot::Clip to the timeline
void Timeline::AddClip(Clip* clip)
{
	// All clips must be converted to the frame rate of this timeline,
	// so assign the same frame rate to each clip.
	clip->Reader()->info.fps = fps.GetFraction();

	// Add clip to list
	clips.push_back(clip);

	// Sort clips
	SortClips();
}

// Remove an openshot::Clip to the timeline
void Timeline::RemoveClip(Clip* clip)
{
	clips.remove(clip);
}

// Calculate time of a frame number, based on a framerate
float Timeline::calculate_time(int number, Framerate rate)
{
	// Get float version of fps fraction
	float raw_fps = rate.GetFPS();

	// Return the time (in seconds) of this frame
	return float(number - 1) / raw_fps;
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
		source_image = source_frame->GetWaveform(width, height, red, green, blue);
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
	Magick::Geometry new_size(width, height);
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
		Magick::Geometry width_size(width, round(width / (float(source_width) / float(source_height))));
		Magick::Geometry height_size(round(height / (float(source_height) / float(source_width))), height);
		new_size.aspect(false); // respect aspect ratio
		if (width_size.width() >= width && width_size.height() >= height)
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
		x = (width - source_width) / 2.0; // center
		break;
	case (GRAVITY_TOP_RIGHT):
		x = width - source_width; // right
		break;
	case (GRAVITY_LEFT):
		y = (height - source_height) / 2.0; // center
		break;
	case (GRAVITY_CENTER):
		x = (width - source_width) / 2.0; // center
		y = (height - source_height) / 2.0; // center
		break;
	case (GRAVITY_RIGHT):
		x = width - source_width; // right
		y = (height - source_height) / 2.0; // center
		break;
	case (GRAVITY_BOTTOM_LEFT):
		y = (height - source_height); // bottom
		break;
	case (GRAVITY_BOTTOM):
		x = (width - source_width) / 2.0; // center
		y = (height - source_height); // bottom
		break;
	case (GRAVITY_BOTTOM_RIGHT):
		x = width - source_width; // right
		y = (height - source_height); // bottom
		break;
	}

	/* LOCATION, ROTATION, AND SCALE */
	float r = source_clip->rotation.GetValue(clip_frame_number); // rotate in degrees
	x += width * source_clip->location_x.GetValue(clip_frame_number); // move in percentage of final width
	y += height * source_clip->location_y.GetValue(clip_frame_number); // move in percentage of final height
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
		if (source_width != width || source_height != height)
		{
			source_image->borderColor(Magick::Color("none"));
			source_image->border(Magick::Geometry(1, 1, 0, 0, false, false)); // prevent stretching of edge pixels (during the canvas resize)
			source_image->size(Magick::Geometry(width, height, 0, 0, false, false)); // resize the canvas (to prevent clipping)
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
		new_frame->AddColor(width, height, Magick::Color(red, green, blue, 0));

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
}

// Sort clips by position on the timeline
void Timeline::SortClips()
{
	// sort clips
	clips.sort(CompareClips());
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
}

// Open the reader (and start consuming resources)
void Timeline::Open()
{

}

// Calculate the # of samples per video frame (for a specific frame number)
int Timeline::GetSamplesPerFrame(int frame_number)
{
	// Get the total # of samples for the previous frame, and the current frame (rounded)
	double fps_value = fps.GetFraction().Reciprocal().ToDouble();
	double previous_samples = round((sample_rate * fps_value) * (frame_number - 1));
	double total_samples = round((sample_rate * fps_value) * frame_number);

	// Subtract the previous frame's total samples with this frame's total samples.  Not all sample rates can
	// be evenly divided into frames, so each frame can have have different # of samples.
	double samples_per_frame = total_samples - previous_samples;
	return samples_per_frame;
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
	if (final_cache.Exists(requested_frame))
		return final_cache.GetFrame(requested_frame);
	else
	{
		// Minimum number of packets to process (for performance reasons)
		int minimum_frames = omp_get_num_procs();

		//omp_set_num_threads(1);
		omp_set_nested(true);
		#pragma omp parallel
		{
			#pragma omp single
			{
				// Loop through all requested frames
				for (int frame_number = requested_frame; frame_number < requested_frame + minimum_frames; frame_number++)
				{
					#pragma omp task firstprivate(frame_number)
					{
						// Create blank frame (which will become the requested frame)
						tr1::shared_ptr<Frame> new_frame(tr1::shared_ptr<Frame>(new Frame(frame_number, width, height, "#000000", GetSamplesPerFrame(frame_number), channels)));
						new_frame->SetSampleRate(info.sample_rate);

						// Calculate time of frame
						float requested_time = calculate_time(frame_number, fps);

						// Find Clips at this time
						list<Clip*>::iterator clip_itr;
						for (clip_itr=clips.begin(); clip_itr != clips.end(); ++clip_itr)
						{
							// Get clip object from the iterator
							Clip *clip = (*clip_itr);

							// Does clip intersect the current requested time
							float clip_duration = clip->End() - clip->Start();
							bool does_clip_intersect = (clip->Position() <= requested_time && clip->Position() + clip_duration >= requested_time);

							// Open (or schedule for closing) this clip, based on if it's intersecting or not
							#pragma omp critical (reader_lock)
							update_open_clips(clip, does_clip_intersect);

							// Clip is visible
							if (does_clip_intersect)
							{
								// Determine the frame needed for this clip (based on the position on the timeline)
								float time_diff = (requested_time - clip->Position()) + clip->Start();
								int clip_frame_number = round(time_diff * fps.GetFPS()) + 1;

								// Add clip's frame as layer
								add_layer(new_frame, clip, clip_frame_number, frame_number);

							} else
								#pragma omp critical (timeline_output)
								cout << "FRAME NOT IN CLIP DURATION: frame: " << frame_number << ", clip->Position(): " << clip->Position() << ", requested_time: " << requested_time << ", clip_duration: " << clip_duration << endl;

							// Check for empty frame image (and fill with color)
							if (new_frame->GetImage()->columns() == 1)
							{
								int red = color.red.GetInt(frame_number);
								int green = color.green.GetInt(frame_number);
								int blue = color.blue.GetInt(frame_number);
								new_frame->AddColor(width, height, Magick::Color(red, green, blue));
							}

							// Add final frame to cache
							#pragma omp critical (timeline_cache)
							final_cache.Add(frame_number, new_frame);

						} // end clip loop

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
