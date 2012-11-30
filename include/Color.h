#ifndef OPENSHOT_COLOR_H
#define OPENSHOT_COLOR_H

/**
 * \file
 * \brief Header file for Color class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "KeyFrame.h"

namespace openshot {

	/**
	 * \brief This struct represents a color (used on the timeline and clips)
	 *
	 * Colors are represented by 4 curves, representing red, green, and blue.  The curves
	 * can be used to animate colors over time.
	 */
	struct Color{
		Keyframe red; ///<Curve representing the red value (0 - 65536)
		Keyframe green; ///<Curve representing the red value (0 - 65536)
		Keyframe blue; ///<Curve representing the red value (0 - 65536)
	};


}

#endif
