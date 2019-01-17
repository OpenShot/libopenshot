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

TEST(Settings_Default_Constructor)
{
	// Create an empty color
	Settings *s = Settings::Instance();

	CHECK_EQUAL(false, s->HARDWARE_DECODE);
	CHECK_EQUAL(false, s->HARDWARE_ENCODE);
	CHECK_EQUAL(false, s->HIGH_QUALITY_SCALING);
	CHECK_EQUAL(false, s->WAIT_FOR_VIDEO_PROCESSING_TASK);
}

TEST(Settings_Change_Settings)
{
	// Create an empty color
	Settings *s = Settings::Instance();
	s->HARDWARE_DECODE = true;
	s->HARDWARE_ENCODE = true;
	s->HIGH_QUALITY_SCALING = true;
	s->WAIT_FOR_VIDEO_PROCESSING_TASK = true;

	CHECK_EQUAL(true, s->HARDWARE_DECODE);
	CHECK_EQUAL(true, s->HARDWARE_ENCODE);
	CHECK_EQUAL(true, s->HIGH_QUALITY_SCALING);
	CHECK_EQUAL(true, s->WAIT_FOR_VIDEO_PROCESSING_TASK);

	CHECK_EQUAL(true, s->HARDWARE_DECODE);
	CHECK_EQUAL(true, s->HARDWARE_ENCODE);
	CHECK_EQUAL(true, Settings::Instance()->HIGH_QUALITY_SCALING);
	CHECK_EQUAL(true, Settings::Instance()->WAIT_FOR_VIDEO_PROCESSING_TASK);
}