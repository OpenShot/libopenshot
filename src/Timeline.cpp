#include "../include/Timeline.h"

using namespace openshot;

// Default Constructor for the timeline (which sets the canvas width and height)
Timeline::Timeline(int width, int height) :
		width(width), height(height)
{
	// Init viewport size (curve based, because it can be animated)
	viewport_scale = Keyframe(100.0);
	viewport_x = Keyframe(0.0);
	viewport_y = Keyframe(0.0);
}

// Get an openshot::Frame object for a specific frame number of this reader.
Frame* Timeline::GetFrame(int requested_frame)
{

}
