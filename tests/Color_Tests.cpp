/**
 * @file
 * @brief Unit tests for openshot::Color
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(Color_Default_Constructor)
{
	// Create an empty color
	Color c1;

	CHECK_CLOSE(0.0f, c1.red.GetValue(0), 0.00001);
	CHECK_CLOSE(0.0f, c1.green.GetValue(0), 0.00001);
	CHECK_CLOSE(0.0f, c1.blue.GetValue(0), 0.00001);
}

TEST(Color_Animate_Colors)
{
	// Create an empty color
	Color c1;

	// Set starting color (on frame 0)
	c1.red.AddPoint(0, 12000);
	c1.green.AddPoint(0, 5000);
	c1.blue.AddPoint(0, 1000);

	// Set ending color (on frame 1000)
	c1.red.AddPoint(1000, 32000);
	c1.green.AddPoint(1000, 12000);
	c1.blue.AddPoint(1000, 5000);

	// Check the color at frame 500
	CHECK_CLOSE(22011, c1.red.GetInt(500), 0.01);
	CHECK_CLOSE(8504, c1.green.GetInt(500), 0.01);
	CHECK_CLOSE(3002, c1.blue.GetInt(500), 0.01);
}
