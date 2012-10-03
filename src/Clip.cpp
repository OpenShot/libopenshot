#include "../include/Clip.h"

using namespace openshot;

// Default Constructor for a clip
Clip::Clip()
{
	// Init clip settings
	position = 0.0;
	layer = 0;
	start = 0.0;
	end = 0.0;
	gravity = CENTER;
	scale = FIT;
	anchor = CANVAS;

	// Init scale curves
	scale_x = Keyframe(100.0);
	scale_y = Keyframe(100.0);

	// Init location curves
	location_x = Keyframe(0.0);
	location_y = Keyframe(0.0);

	// Init alpha & rotation
	alpha = Keyframe(100.0);
	rotation = Keyframe(0.0);

	// Init time & volume
	time = Keyframe(0.0);
	volume = Keyframe(100.0);

}
