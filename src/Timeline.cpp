#include "../include/Timeline.h"

using namespace openshot;

// Default Constructor for the timeline (which sets the canvas width and height)
Timeline::Timeline(int width, int height, Framerate fps) :
		width(width), height(height), fps(fps)
{
	// Init viewport size (curve based, because it can be animated)
	viewport_scale = Keyframe(100.0);
	viewport_x = Keyframe(0.0);
	viewport_y = Keyframe(0.0);
}

// Add an openshot::Clip to the timeline
void Timeline::AddClip(Clip* clip)
{
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

		cout << "-- Remove clip " << clip << " from opened clips map" << endl;
	}
	else if (!clip_found && is_open)
	{
		// Add clip to 'opened' list, because it's missing
		open_clips[clip] = clip;

		// Open the clip's reader
		clip->Open();

		cout << "-- Add clip " << clip << " to opened clips map" << endl;
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

// Get an openshot::Frame object for a specific frame number of this reader.
Frame* Timeline::GetFrame(int requested_frame) throw(ReaderClosed)
{
	// Adjust out of bounds frame number
	if (requested_frame < 1)
		requested_frame = 1;

	// Calculate time of frame
	float requested_time = calculate_time(requested_frame, fps);

	cout << "requested_frame: " << requested_frame << ", requested_time: " << requested_time << endl;

	// Find Clips at this time
	list<Clip*>::iterator clip_itr;
	for (clip_itr=clips.begin(); clip_itr != clips.end(); ++clip_itr)
	{
		// Get clip object from the iterator
		Clip *clip = (*clip_itr);

		// Does clip intersect the current requested time
		bool does_clip_intersect = (clip->Position() <= requested_time && clip->Position() + clip->Reader()->info.duration >= requested_time);

		// Open or Close this clip, based on if it's intersecting or not
		update_open_clips(clip, does_clip_intersect);

		// Clip is visible
		if (does_clip_intersect)
		{
			// Display the clip (DEBUG)
			//clip->GetFrame(requested_frame)->Display();
		}
	}


}
