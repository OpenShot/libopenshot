/**
 * @file
 * @brief Source file for the TrackedObjectBase class
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

#include "TrackedObjectBase.h"
#include <algorithm>
#include <functional>
#include <utility>


namespace openshot{

	// Blank constructor
    TrackedObjectBase::TrackedObjectBase() : visible(1.0), draw_box(1)
	{
		// Initializes the id as "None"
		id = "None";
		childClipId = "None";
    }

	// Default constructor
	TrackedObjectBase::TrackedObjectBase(std::string _id) : visible(1.0)
	{
		Id(_id);
		childClipId = "None";
	}

	Json::Value TrackedObjectBase::add_property_choice_json(std::string name, int value, int selected_value) const {
		// Create choice
		Json::Value new_choice = Json::Value(Json::objectValue);
		new_choice["name"] = name;
		new_choice["value"] = value;
		new_choice["selected"] = (value == selected_value);

		// return JsonValue
		return new_choice;
	}
}