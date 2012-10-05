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

// Sort clips by position on the timeline
void Timeline::SortClips()
{
	// sort clips
	clips.sort(compare_clip_pointers());
}

// Get an openshot::Frame object for a specific frame number of this reader.
Frame* Timeline::GetFrame(int requested_frame)
{
	// Adjust out of bounds frame number
	if (requested_frame < 1)
		requested_frame = 1;

	// Calculate time of frame
	float requested_time = calculate_time(requested_frame, fps);

	// Find Clips at this time
	list<Clip*>::iterator clip_itr;
	for (clip_itr=clips.begin(); clip_itr != clips.end(); ++clip_itr)
	{
		// Get clip object from the iterator
		Clip *clip = (*clip_itr);

		cout << clip->Position() << endl;
	}

}
