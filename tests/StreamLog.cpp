/**
 * @file
 * @brief Unit tests for openshot::Fraction
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

#include <string>
#include <sstream>
#include <ostream>

#include <catch2/catch.hpp>

#include "ZmqLogger.h"

// A destination for our logging messages
std::stringstream output;
// Define a log function that writes to output
void myLogOut(const std::string& message) {
	output << message << std::endl;
}
#define myLog() openshot::StreamLog(myLogOut).GetStream()

TEST_CASE( "Log to stream", "[libopenshot][streamlog]" )
{
	// Reset output buffer
	output.str(std::string());
	output.clear();

	myLog() << "StreamLogger test log";

	CHECK(output.str() == "StreamLogger test log\n");
}

TEST_CASE( "LOGVAR macro", "[libopenshot][streamlog]" )
{
	// Reset output buffer
	output.str(std::string());
	output.clear();

	int x = 10;
	myLog() << "Value of x: " << LOGVAR(x);

	CHECK(output.str() == "Value of x: x = 10\n");
}
