/**
 * @file
 * @brief Unit tests for openshot::Color
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
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

#include <catch2/catch.hpp>

#include "Settings.h"

using namespace openshot;

TEST_CASE( "Default_Constructor", "[libopenshot][settings]" )
{
	// Create an empty color
	Settings *s = Settings::Instance();

	CHECK(s->OMP_THREADS == 12);
	CHECK_FALSE(s->HIGH_QUALITY_SCALING);
}

TEST_CASE( "Change_Settings", "[libopenshot][settings]" )
{
	// Create an empty color
	Settings *s = Settings::Instance();
	s->OMP_THREADS = 8;
	s->HIGH_QUALITY_SCALING = true;

	CHECK(s->OMP_THREADS == 8);
	CHECK(s->HIGH_QUALITY_SCALING == true);

	CHECK(Settings::Instance()->OMP_THREADS == 8);
	CHECK(Settings::Instance()->HIGH_QUALITY_SCALING == true);
}
