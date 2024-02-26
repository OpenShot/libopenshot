/**
 * @file
 * @brief Source file for the TrackedObjectBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "TrackedObjectBase.h"

#include "Json.h"

namespace openshot
{

	// Default constructor, delegating
	TrackedObjectBase::TrackedObjectBase() : TrackedObjectBase("") {}

	// Constructor
	TrackedObjectBase::TrackedObjectBase(std::string _id)
		: visible(1.0), draw_box(1), id(_id) {}

	Json::Value TrackedObjectBase::add_property_choice_json(
		std::string name, int value, int selected_value) const
	{
		// Create choice
		Json::Value new_choice = Json::Value(Json::objectValue);
		new_choice["name"] = name;
		new_choice["value"] = value;
		new_choice["selected"] = (value == selected_value);

		// return JsonValue
		return new_choice;
	}
} // namespace openshot
