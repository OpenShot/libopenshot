/**
 * @file
 * @brief Unit tests for openshot::Coordinate
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

TEST(Coordinate_Default_Constructor)
{
	// Create an empty coordinate
	Coordinate c1;

	CHECK_CLOSE(0.0f, c1.X, 0.00001);
	CHECK_CLOSE(0.0f, c1.Y, 0.00001);
}

TEST(Coordinate_X_Y_Constructor)
{
	// Create an empty coordinate
	Coordinate c1(2,8);

	CHECK_CLOSE(2.0f, c1.X, 0.00001);
	CHECK_CLOSE(8.0f, c1.Y, 0.00001);
}
