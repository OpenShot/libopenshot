#ifndef OPENSHOT_COORDINATE_H
#define OPENSHOT_COORDINATE_H

/**
 * \file
 * \brief Header file for Coordinate class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

/// This namespace is the default namespace for all code in the openshot library.
namespace openshot {

	/**
	 * \brief This class represents a Cartesian coordinate (X, Y) used in the Keyframe animation system.
	 *
	 * Animation involves the changing (i.e. interpolation) of numbers over time.  A series of Coordinate
	 * objects allows us to plot a specific curve or line used during interpolation.  In other words, it helps us
	 * control how a number changes over time (quickly or slowly).
	 *
	 * Please see the following <b>Example Code</b>:
	 * \code
	 * Coordinate c1(2,4);
	 * assert(c1.X == 2.0f);
	 * assert(c1.Y == 4.0f);
	 * \endcode
	 */
	class Coordinate {
	public:
		float X; ///< The X value of the coordinate
		float Y; ///< The Y value of the coordinate

		/// The default constructor, which defaults to (0,0)
		Coordinate();

		/// Constructor which also sets the X and Y
		Coordinate(float x, float y);
	};

}

#endif
