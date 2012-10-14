#ifndef OPENSHOT_FRAMERATE_H
#define OPENSHOT_FRAMERATE_H

/**
 * \file
 * \brief Header file for the FrameRate class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include <math.h>
#include "Fraction.h"

namespace openshot
{
	/**
	 * \brief This class represents a frame rate (otherwise known as frames per second).
	 *
	 * Frame rates are stored as a fraction, such as 24/1, 25/1 and 30000/1001 (29.97).
	 *
	 * Please see the following <b>Example Code</b>:
	 * \code
	 * Framerate rate(25, 1);
	 * assert(rate.GetRoundedFPS() == 25);
	 *
	 * Framerate rate(30000, 1001);
	 * assert(rate.GetRoundedFPS() == 30);
	 * \endcode
	 */
	class Framerate {
	private:
		int m_numerator;
		int m_denominator;

	public:

		/// Default constructor (24/1 FPS)
		Framerate();

		/// Constructor which lets you set the frame rate (as a fraction)
		Framerate(int numerator, int denominator);

		/// Return a rounded integer of the frame rate (for example 30000/1001 returns 30 fps)
		int GetRoundedFPS();

		/// Return a float of the frame rate (for example 30000/1001 returns 29.97...)
		float GetFPS();

		/// Return a Fraction of the framerate
		Fraction GetFraction();
	};
}

#endif
