/**
 * @file
 * @brief Unit tests for openshot::Framerate
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

#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(FrameRate_Check_Rounded_24_FPS)
{
	// Create framerate for 24 fps
	Framerate rate(24, 1);

	CHECK_EQUAL(24, rate.GetRoundedFPS());
}

TEST(FrameRate_Check_Rounded_25_FPS)
{
	// Create framerate for 25 fps
	Framerate rate(25, 1);

	CHECK_EQUAL(25, rate.GetRoundedFPS());
}

TEST(FrameRate_Check_Rounded_29_97_FPS)
{
	// Create framerate for 29.97 fps
	Framerate rate(30000, 1001);

	CHECK_EQUAL(30, rate.GetRoundedFPS());
}

TEST(FrameRate_Check_Decimal_24_FPS)
{
	// Create framerate for 24 fps
	Framerate rate(24, 1);

	CHECK_CLOSE(24.0f, rate.GetFPS(), 0.0001);
}

TEST(FrameRate_Check_Decimal_25_FPS)
{
	// Create framerate for 24 fps
	Framerate rate(25, 1);

	CHECK_CLOSE(25.0f, rate.GetFPS(), 0.0001);
}

TEST(FrameRate_Check_Decimal_29_97_FPS)
{
	// Create framerate for 29.97 fps
	Framerate rate(30000, 1001);

	CHECK_CLOSE(29.97f, rate.GetFPS(), 0.0001);
}

