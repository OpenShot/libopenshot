/**
 * @file
 * @brief Unit tests for openshot::Timeline
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
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
 */

#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(Timeline_Constructor)
{
	// Create a default fraction (should be 1/1)
	Fraction fps(30000,1000);
	Timeline t1(640, 480, fps, 44100, 2);

	// Check values
	CHECK_EQUAL(640, t1.info.width);
	CHECK_EQUAL(480, t1.info.height);

	// Create a default fraction (should be 1/1)
	Timeline t2(300, 240, fps, 44100, 2);

	// Check values
	CHECK_EQUAL(300, t2.info.width);
	CHECK_EQUAL(240, t2.info.height);
}

TEST(Timeline_Width_and_Height_Functions)
{
	// Create a default fraction (should be 1/1)
	Fraction fps(30000,1000);
	Timeline t1(640, 480, fps, 44100, 2);

	// Check values
	CHECK_EQUAL(640, t1.info.width);
	CHECK_EQUAL(480, t1.info.height);

	// Set width
	t1.info.width = 600;

	// Check values
	CHECK_EQUAL(600, t1.info.width);
	CHECK_EQUAL(480, t1.info.height);

	// Set height
	t1.info.height = 400;

	// Check values
	CHECK_EQUAL(600, t1.info.width);
	CHECK_EQUAL(400, t1.info.height);
}

TEST(Timeline_Framerate)
{
	// Create a default fraction (should be 1/1)
	Fraction fps(24,1);
	Timeline t1(640, 480, fps, 44100, 2);

	// Check values
	CHECK_CLOSE(24.0f, t1.info.fps.ToFloat(), 0.00001);
}
