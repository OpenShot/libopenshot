/**
 * @file
 * @brief Source file for EffectBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <iostream>
#include <iomanip>

#include "EffectBase.h"

#include "Exceptions.h"
#include "Timeline.h"

using namespace openshot;

// Initialize the values of the EffectInfo struct
void EffectBase::InitEffectInfo()
{
	// Init clip settings
	Position(0.0);
	Layer(0);
	Start(0.0);
	End(0.0);
	Order(0);
	ParentClip(NULL);
	parentEffect = NULL;

	info.has_video = false;
	info.has_audio = false;
	info.has_tracked_object = false;
	info.name = "";
	info.description = "";
	info.parent_effect_id = "";
	info.apply_before_clip = true;
}

// Display file information
void EffectBase::DisplayInfo(std::ostream* out) {
	*out << std::fixed << std::setprecision(2) << std::boolalpha;
	*out << "----------------------------" << std::endl;
	*out << "----- Effect Information -----" << std::endl;
	*out << "----------------------------" << std::endl;
	*out << "--> Name: " << info.name << std::endl;
	*out << "--> Description: " << info.description << std::endl;
	*out << "--> Has Video: " << info.has_video << std::endl;
	*out << "--> Has Audio: " << info.has_audio << std::endl;
	*out << "--> Apply Before Clip Keyframes: " << info.apply_before_clip << std::endl;
	*out << "--> Order: " << order << std::endl;
	*out << "----------------------------" << std::endl;
}

// Constrain a color value from 0 to 255
int EffectBase::constrain(int color_value)
{
	// Constrain new color from 0 to 255
	if (color_value < 0)
		color_value = 0;
	else if (color_value > 255)
		color_value = 255;

	return color_value;
}

// Generate JSON string of this object
std::string EffectBase::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value EffectBase::JsonValue() const {

	// Create root json object
	Json::Value root = ClipBase::JsonValue(); // get parent properties
	root["name"] = info.name;
	root["class_name"] = info.class_name;
	root["description"] = info.description;
	root["parent_effect_id"] = info.parent_effect_id;
	root["has_video"] = info.has_video;
	root["has_audio"] = info.has_audio;
	root["has_tracked_object"] = info.has_tracked_object;
	root["apply_before_clip"] = info.apply_before_clip;
	root["order"] = Order();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void EffectBase::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void EffectBase::SetJsonValue(const Json::Value root) {

	if (ParentTimeline()){
		// Get parent timeline
		Timeline* parentTimeline = static_cast<Timeline *>(ParentTimeline());

		// Get the list of effects on the timeline
		std::list<EffectBase*> effects = parentTimeline->ClipEffects();

		// TODO: Fix recursive call for Object Detection

		// // Loop through the effects and check if we have a child effect linked to this effect
		for (auto const& effect : effects){
			// Set the properties of all effects which parentEffect points to this
			if ((effect->info.parent_effect_id == this->Id()) && (effect->Id() != this->Id()))
				effect->SetJsonValue(root);
		}
	}

	// Set this effect properties with the parent effect properties (except the id and parent_effect_id)
	Json::Value my_root;
	if (parentEffect){
		my_root = parentEffect->JsonValue();
		my_root["id"] = this->Id();
		my_root["parent_effect_id"] = this->info.parent_effect_id;
	} else {
		my_root = root;
	}

	// Set parent data
	ClipBase::SetJsonValue(my_root);

	// Set data from Json (if key is found)
	if (!my_root["order"].isNull())
		Order(my_root["order"].asInt());

	if (!my_root["apply_before_clip"].isNull())
		info.apply_before_clip = my_root["apply_before_clip"].asBool();

	if (!my_root["parent_effect_id"].isNull()){
		info.parent_effect_id = my_root["parent_effect_id"].asString();
		if (info.parent_effect_id.size() > 0 && info.parent_effect_id != "" && parentEffect == NULL)
			SetParentEffect(info.parent_effect_id);
		else
			parentEffect = NULL;
	}
}

// Generate Json::Value for this object
Json::Value EffectBase::JsonInfo() const {

	// Create root json object
	Json::Value root;
	root["name"] = info.name;
	root["class_name"] = info.class_name;
	root["description"] = info.description;
	root["has_video"] = info.has_video;
	root["has_audio"] = info.has_audio;

	// return JsonValue
	return root;
}

// Get all properties for a specific frame
Json::Value EffectBase::BasePropertiesJSON(int64_t requested_frame) const {
	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 30 * 60 * 60 * 48, true, requested_frame);

	// Add replace_image choices (dropdown style)
	root["apply_before_clip"] = add_property_json("Apply Before Clip Keyframes", info.apply_before_clip, "int", "", NULL, 0, 1, false, requested_frame);
	root["apply_before_clip"]["choices"].append(add_property_choice_json("Yes", true, info.apply_before_clip));
	root["apply_before_clip"]["choices"].append(add_property_choice_json("No", false, info.apply_before_clip));

	// Set the parent effect which properties this effect will inherit
	root["parent_effect_id"] = add_property_json("Parent", 0.0, "string", info.parent_effect_id, NULL, -1, -1, false, requested_frame);

	return root;
}

/// Parent clip object of this reader (which can be unparented and NULL)
openshot::ClipBase* EffectBase::ParentClip() {
	return clip;
}

/// Set parent clip object of this reader
void EffectBase::ParentClip(openshot::ClipBase* new_clip) {
	clip = new_clip;
}

// Set the parent effect from which this properties will be set to
void EffectBase::SetParentEffect(std::string parentEffect_id) {

	// Get parent Timeline
	Timeline* parentTimeline = static_cast<Timeline *>(ParentTimeline());

	if (parentTimeline){

		// Get a pointer to the parentEffect
		EffectBase* parentEffectPtr = parentTimeline->GetClipEffect(parentEffect_id);

		if (parentEffectPtr){
			// Set the parent Effect
			parentEffect = parentEffectPtr;

			// Set the properties of this effect with the parent effect's properties
			Json::Value EffectJSON = parentEffect->JsonValue();
			EffectJSON["id"] = this->Id();
			EffectJSON["parent_effect_id"] = this->info.parent_effect_id;
			this->SetJsonValue(EffectJSON);
		}
	}
	return;
}

// Return the ID of this effect's parent clip
std::string EffectBase::ParentClipId() const{
	if(clip)
		return clip->Id();
	else
		return "";
}
