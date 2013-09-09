#ifndef OPENSHOT_POINT_H
#define OPENSHOT_POINT_H

/**
 * \file
 * \brief Header file for the Point class
 * \author Copyright (c) 2008-2013 OpenShot Studios, LLC
 */

#include "Coordinate.h"

namespace openshot
{
	/**
	 * This controls how a Keyframe uses this point to interpolate between the previous
	 * point and this one.  Bezier is a smooth curve.  Linear is a straight line.  Constant is
	 * is a jump from the previous point to this one.
	 */
	enum Interpolation_Type {
		BEZIER,
		LINEAR,
		CONSTANT
	};

	/**
	 * When BEZIER interpolation is used, the point's left and right handles are used
	 * to influence the direction of the curve.  AUTO will try and adjust the handles
	 * automatically, to achieve the smoothest curves.  MANUAL will leave the handles
	 * alone, making it the responsibility of the user to set them.
	 */
	enum Handle_Type {
		AUTO,
		MANUAL
	};

	/**
	 * \brief A Point is the basic building block of a key-frame curve.
	 *
	 * Points have a primary coordinate and a left and right handle coordinate.
	 * The handles are used to influence the direction of the curve as it
	 * moves between the primary coordinate and the next primary coordinate when the
	 * interpolation mode is BEZIER.  When using LINEAR or CONSTANT, the handles are
	 * ignored.
	 *
	 * Please see the following <b>Example Code</b>:
	 * \code
	 * Coordinate c1(3,9);
	 * Point p1(c1, BEZIER);
	 * assert(c1.X == 3);
	 * assert(c1.Y == 9);
	 *
	 * \endcode
	 */
	class Point {
	public:
		Coordinate co; 						///< This is the primary coordinate
		Coordinate handle_left; 			///< This is the left handle coordinate
		Coordinate handle_right; 			///< This is the right handle coordinate
		Interpolation_Type interpolation;	///< This is the interpolation mode
		Handle_Type handle_type; 			///< This is the handle mode

		/// Constructor which creates a single coordinate at X=0
		Point(float y);

		/// Constructor which also creates a Point and sets the X and Y of the Point.
		Point(float x, float y);

		/// Constructor which also creates a Point and sets the X,Y, and interpolation of the Point.
		Point(float x, float y, Interpolation_Type interpolation);

		// Constructor which takes a coordinate
		Point(Coordinate co);

		// Constructor which takes a coordinate and interpolation mode
		Point(Coordinate co, Interpolation_Type interpolation);

		// Constructor which takes a coordinate, interpolation mode, and handle type
		Point(Coordinate co, Interpolation_Type interpolation, Handle_Type handle_type);

		/**
		 * Set the left and right handles to the same Y coordinate as the primary
		 * coordinate, but offset the X value by a given amount.  This is typically used
		 * to smooth the curve (if BEZIER interpolation mode is used)
		 */
		void Initialize_Handles(float Offset = 0.0f);

	};

}

#endif
