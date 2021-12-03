/**
 * @file
 * @brief Helper functions for Json parsing
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Json.h"
#include "Exceptions.h"

const Json::Value openshot::stringToJson(const std::string value) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::CharReaderBuilder rbuilder;
	Json::CharReader* reader(rbuilder.newCharReader());

	std::string errors;
	bool success = reader->parse( value.c_str(), value.c_str() + value.size(),
	                              &root, &errors );
	delete reader;

	if (!success)
		// Raise exception
		throw openshot::InvalidJSON("JSON could not be parsed (or is invalid)");

	return root;
}
