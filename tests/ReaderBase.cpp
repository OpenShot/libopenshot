/**
 * @file
 * @brief Unit tests for openshot::ReaderBase
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

#include <memory>
#include <string>

#include <catch2/catch.hpp>

#include "ReaderBase.h"
#include "CacheBase.h"
#include "Frame.h"
#include "Json.h"

using namespace openshot;

// Since it is not possible to instantiate an abstract class, this test creates
// a new derived class, in order to test the base class file info struct.
TEST_CASE( "ReaderBase_Derived_Class", "[libopenshot][readerbase]" )
{
	// Create a new derived class from type ReaderBase
	class TestReader : public ReaderBase
	{
	public:
		TestReader() { };
		CacheBase* GetCache() { return nullptr; };
		std::shared_ptr<Frame> GetFrame(int64_t number) { std::shared_ptr<Frame> f(new Frame()); return f; }
		void Close() { };
		void Open() { };
		std::string Json() const { return ""; };
		void SetJson(std::string value) { };
		Json::Value JsonValue() const { return Json::Value("{}"); };
		void SetJsonValue(Json::Value root) { };
		bool IsOpen() { return true; };
		std::string Name() { return "TestReader"; };
	};

	// Create an instance of the derived class
	TestReader t1;

	// Validate the new class
	CHECK(t1.Name() == "TestReader");

	t1.Close();
	t1.Open();
	CHECK(t1.IsOpen() == true);

	CHECK(t1.GetCache() == nullptr);

	t1.SetJson("{ }");
	t1.SetJsonValue(Json::Value("{}"));
	CHECK(t1.Json() == "");
	auto json = t1.JsonValue();
	CHECK(Json::Value("{}") == json);

	auto f = t1.GetFrame(1);

	REQUIRE(f != nullptr);
	CHECK(f->number == 1);

	// Check some of the default values of the FileInfo struct on the base class
	CHECK_FALSE(t1.info.has_audio);
	CHECK_FALSE(t1.info.has_audio);
	CHECK(t1.info.duration == Approx(0.0f).margin(0.00001));
	CHECK(t1.info.height == 0);
	CHECK(t1.info.width == 0);
	CHECK(t1.info.fps.num == 1);
	CHECK(t1.info.fps.den == 1);
}
