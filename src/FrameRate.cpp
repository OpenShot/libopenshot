/**
 * @file
 * @brief Source file for the FrameRate class
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

#include "../include/FrameRate.h"

using namespace openshot;

Framerate::Framerate(int numerator, int denominator) {
	m_numerator = numerator;
	m_denominator = denominator;
}

Framerate::Framerate() {
	m_numerator = 24;
	m_denominator = 1;
}

// Return a rounded integer of the frame rate (for example 30000/1001 returns 30 fps)
int Framerate::GetRoundedFPS() {
	return round((float) m_numerator / m_denominator);
}

// Return a float of the frame rate (for example 30000/1001 returns 29.97...)
float Framerate::GetFPS() {
	return (float) m_numerator / m_denominator;
}

// Return a Fraction of the framerate
Fraction Framerate::GetFraction()
{
	return Fraction(m_numerator, m_denominator);
}
