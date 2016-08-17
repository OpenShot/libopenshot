/**
 * @file
 * @brief Source file for EffectInfo class
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

#include "../include/EffectInfo.h"


using namespace openshot;


// Generate JSON string of this object
string EffectInfo::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Create a new effect instance
EffectBase* EffectInfo::CreateEffect(string effect_type) {
	// Init the matching effect object
	if (effect_type == "Blur")
		return new Blur();

	else if (effect_type == "Brightness")
		return new Brightness();

	else if (effect_type == "ChromaKey")
		return new ChromaKey();

	else if (effect_type == "Deinterlace")
		return new Deinterlace();

	else if (effect_type == "Mask")
		return new Mask();

	else if (effect_type == "Negate")
		return new Negate();

	else if (effect_type == "Saturation")
		return new Saturation();
}

// Generate Json::JsonValue for this object
Json::Value EffectInfo::JsonValue() {

	// Create root json object
	Json::Value root;

	// Append info JSON from each supported effect
	root.append(Blur().JsonInfo());
	root.append(Brightness().JsonInfo());
	root.append(ChromaKey().JsonInfo());
	root.append(Deinterlace().JsonInfo());
	root.append(Mask().JsonInfo());
	root.append(Negate().JsonInfo());
	root.append(Saturation().JsonInfo());

	// return JsonValue
	return root;

}
