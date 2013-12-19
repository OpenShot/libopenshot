/**
 * @file
 * @brief Unit tests for openshot::ReaderBase
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

// Since it is not possible to instantiate an abstract class, this test creates
// a new derived class, in order to test the base class file info struct.
TEST(ReaderBase_Derived_Class)
{
	// Create a new derived class from type ReaderBase
	class TestReader : public ReaderBase
	{
	public:
		TestReader() { InitFileInfo(); };
		tr1::shared_ptr<Frame> GetFrame(int number) { tr1::shared_ptr<Frame> f(new Frame()); return f; }
		void Close() { };
		void Open() { };
		string Json() { };
		void SetJson(string value) throw(InvalidJSON) { };
		Json::Value JsonValue() { };
		void SetJsonValue(Json::Value root) { };
		bool IsOpen() { return true; };
	};

	// Create an instance of the derived class
	TestReader t1;

	// Check some of the default values of the FileInfo struct on the base class
	// If InitFileInfo() is not called in the derived class, these checks would fail.
	CHECK_EQUAL(false, t1.info.has_audio);
	CHECK_EQUAL(false, t1.info.has_audio);
	CHECK_CLOSE(0.0f, t1.info.duration, 0.00001);
	CHECK_EQUAL(0, t1.info.height);
	CHECK_EQUAL(0, t1.info.width);
	CHECK_EQUAL(1, t1.info.fps.num);
	CHECK_EQUAL(1, t1.info.fps.den);
}
