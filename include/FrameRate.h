/**
 * @file
 * @brief Header file for the FrameRate class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_FRAMERATE_H
#define OPENSHOT_FRAMERATE_H

#include <math.h>
#include "Fraction.h"

namespace openshot
{
	/**
	 * @brief This class represents a frame rate (otherwise known as frames per second).
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
