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

// Calculate time of a frame number, based on a framerate
float Timeline::calculate_time(int number, Framerate rate)
{
	// Get float version of fps fraction
	float raw_fps = rate.GetFPS();

	// Return the time (in seconds) of this frame
	return float(number - 1) / raw_fps;
}

// Process a new layer of video or audio
void Timeline::add_layer(tr1::shared_ptr<Frame> new_frame, Clip* source_clip, int clip_frame_number)
{
	// Get the clip's frame & image
	tr1::shared_ptr<Frame> source_frame = source_clip->GetFrame(clip_frame_number);
	tr1::shared_ptr<Magick::Image> source_image = source_frame->GetImage();

	// Replace image (needed if this is the 1st layer)
	if (new_frame->GetImage()->columns() == 1)
		new_frame->AddColor(width, height, "#000000");

	// Apply image effects
	//if (source_clip->rotation.GetValue(clip_frame_number) != 0)
	//	source_image->rotate(source_clip->rotation.GetValue(clip_frame_number));
	if (source_clip->alpha.GetValue(clip_frame_number) != 0)
	{
		// Calculate opacity of new image
		int new_opacity = 65535.0f * source_clip->alpha.GetValue(clip_frame_number);
		if (new_opacity < 0) new_opacity = 0; // completely invisible
		source_image->opacity(new_opacity);
	}

	// Copy audio from source frame
	for (int channel = 0; channel < source_frame->GetAudioChannelsCount(); channel++)
		new_frame->AddAudio(channel, 0, source_frame->GetAudioSamples(channel), source_frame->GetAudioSamplesCount(), 1.0f);

	// Location, Rotation, and Scale
	float r = source_clip->rotation.GetValue(clip_frame_number);
	float x = source_clip->location_x.GetValue(clip_frame_number);
	float y = source_clip->location_y.GetValue(clip_frame_number);
	float sx = source_clip->scale_x.GetValue(clip_frame_number);
	float sy = source_clip->scale_y.GetValue(clip_frame_number);

	cout << "r: " << r << endl;
	cout << "x: " << x << endl;
	cout << "y: " << y << endl;

	// X,Y     Scale     Angle  NewX,NewY
	double distort_args[7] = { source_image->columns()/2,source_image->rows()/2,  sx,sy,  r,  x,y };
	//source_image->size(Magick::Geometry(round(x + source_image->columns()), round(y + source_image->rows()), 0,0,false,false));
	source_image->distort(Magick::ScaleRotateTranslateDistortion, 7, distort_args, true);

	if (clip_frame_number == 100)
		source_image->display();

	// Composite images together
	tr1::shared_ptr<Magick::Image> new_image = new_frame->GetImage();
	new_image->composite(*source_image.get(), 0, 0, Magick::BlendCompositeOp);
}

// Update the list of 'opened' clips
void Timeline::update_open_clips(Clip *clip, bool is_open)
{
	// is clip already in list?
	bool clip_found = open_clips.count(clip);

	if (clip_found && !is_open)
	{
		// Remove clip from 'opened' list, because it's closed now
		open_clips.erase(clip);

		// Close the clip's reader
		clip->Close();
	}
	else if (!clip_found && is_open)
	{
		// Add clip to 'opened' list, because it's missing
		open_clips[clip] = clip;

		// Open the clip's reader
		clip->Open();
	}
}

// Sort clips by position on the timeline
void Timeline::SortClips()
{
	// sort clips
	clips.sort(compare_clip_pointers());
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


// Get an openshot::Frame object for a specific frame number of this reader.
tr1::shared_ptr<Frame> Timeline::GetFrame(int requested_frame) throw(ReaderClosed)
{
	// Adjust out of bounds frame number
	if (requested_frame < 1)
		requested_frame = 1;

	// Create blank frame (which will become the requested frame)
	tr1::shared_ptr<Frame> new_frame(tr1::shared_ptr<Frame>(new Frame(requested_frame, width, height, "#000000", GetSamplesPerFrame(requested_frame), channels)));

	// Calculate time of frame
	float requested_time = calculate_time(requested_frame, fps);

	// Find Clips at this time
	list<Clip*>::iterator clip_itr;
	for (clip_itr=clips.begin(); clip_itr != clips.end(); ++clip_itr)
	{
		// Get clip object from the iterator
		Clip *clip = (*clip_itr);

		// Does clip intersect the current requested time
		float clip_duration = clip->End() - clip->Start();
		bool does_clip_intersect = (clip->Position() <= requested_time && clip->Position() + clip_duration >= requested_time);

		// Open or Close this clip, based on if it's intersecting or not
		update_open_clips(clip, does_clip_intersect);

		// Clip is visible
		if (does_clip_intersect)
		{
			// Determine the frame needed for this clip (based on the position on the timeline)
			float time_diff = (requested_time - clip->Position()) + clip->Start();
			int clip_frame_number = round(time_diff * fps.GetFPS()) + 1;

			// Add clip's frame as layer
			add_layer(new_frame, clip, clip_frame_number);

		} else
			cout << "FRAME NOT IN CLIP DURATION: frame: " << requested_frame << ", pos: " << clip->Position() << ", end: " << clip->End() << endl;
	}

	// Check for empty frame image (and fill with color)
	if (new_frame->GetImage()->columns() == 1)
		new_frame->AddColor(width, height, "#000000");

	// No clips found
	return new_frame;
}
