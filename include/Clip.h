#ifndef OPENSHOT_CLIP_H
#define OPENSHOT_CLIP_H

/**
 * \file
 * \brief Header file for Clip class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "KeyFrame.h"

using namespace std;
using namespace openshot;

namespace openshot {

	/**
	 * This enumeration determines how clips are aligned to their parent container.
	 */
	enum GravityType
	{
		TOP_LEFT,
		TOP,
		TOP_RIGHT,
		LEFT,
		CENTER,
		RIGHT,
		BOTTOM_LEFT,
		BOTTOM,
		BOTTOM_RIGHT
	};

	/**
	 * This enumeration determines how clips are scaled to fit their parent container.
	 */
	enum ScaleType
	{
		CROP,
		FIT,
		STRETCH,
		NONE
	};

	/**
	 * This enumeration determines what parent a clip should be aligned to.
	 */
	enum AnchorType
	{
		CANVAS,
		VIEWPORT
	};

	/**
	 * \brief This class represents a clip
	 *
	 * Each image, video, or audio file is represented on a layer as a clip.  A clip has many
	 * properties that affect how it behaves on the timeline, such as its size, position,
	 * transparency, rotation, speed, volume, etc...
	 */
	class Clip {
	private:
		float position; ///<The position of the timeline where this clip should start playing
		int layer; ///<The layer this clip is on. Lower clips are covered up by higher clips.
		float start; ///<The position in seconds to start playing (used to trim the beginning of a clip)
		float end; ///<The position in seconds to end playing (used to trim the ending of a clip)

		GravityType gravity; ///<The gravity of a clip determines where it snaps to it's parent
		ScaleType scale; ///<The scale determines how a clip should be resized to fit it's parent
		AnchorType anchor; ///<The anchor determines what parent a clip should snap to

		// Scale and Location curves
		Keyframe scale_x; ///<Curve representing the horizontal scaling (0 to 100)
		Keyframe scale_y; ///<Curve representing the vertical scaling (0 to 100)
		Keyframe location_x; ///<Curve representing the relative X position based on the gravity (-100 to 100)
		Keyframe location_y; ///<Curve representing the relative Y position based on the gravity (-100 to 100)

		// Alpha and Rotation curves
		Keyframe alpha; ///<Curve representing the alpha or transparency (0 to 100)
		Keyframe rotation; ///<Curve representing the rotation (0 to 360)

		// Time and Volume curves
		Keyframe time; ///<Curve representing the frames over time to play (used for speed and direction of video)
		Keyframe volume; ///<Curve representing the volume (0 to 100)

	public:

		/// Default Constructor
		Clip();

	};


}

#endif
