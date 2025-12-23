/**
 * @file
 * @brief Source file for Tracker effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <string>
#include <memory>
#include <iostream>
#include <algorithm>

#include "effects/Tracker.h"
#include "Exceptions.h"
#include "Timeline.h"
#include "trackerdata.pb.h"

#include <google/protobuf/util/time_util.h>

#include <QImage>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QRectF>

using namespace std;
using namespace openshot;
using google::protobuf::util::TimeUtil;


// Default constructor
Tracker::Tracker()
{
	// Initialize effect metadata
	init_effect_details();

	// Create a placeholder object so we always have index 0 available
	trackedData = std::make_shared<TrackedObjectBBox>();
	trackedData->ParentClip(this->ParentClip());

	// Seed our map with a single entry at index 0
	trackedObjects.clear();
	trackedObjects.emplace(0, trackedData);

	// Assign ID to the placeholder object
	if (trackedData)
	trackedData->Id(Id() + "-0");
}

// Init effect settings
void Tracker::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Tracker";
	info.name = "Tracker";
	info.description = "Track the selected bounding box through the video.";
	info.has_audio = false;
	info.has_video = true;
	info.has_tracked_object = true;

	this->TimeScale = 1.0;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Tracker::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
	// Sanity‐check
	if (!frame) return frame;
	auto frame_image = frame->GetImage();
	if (!frame_image || frame_image->isNull()) return frame;
	if (!trackedData) return frame;

	// 2) Only proceed if we actually have a box and it's visible
	if (!trackedData->Contains(frame_number) ||
		trackedData->visible.GetValue(frame_number) != 1)
		return frame;

	QPainter painter(frame_image.get());
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

	// Draw the box
	BBox fd = trackedData->GetBox(frame_number);
	QRectF boxRect(
		(fd.cx - fd.width/2) * frame_image->width(),
		(fd.cy - fd.height/2) * frame_image->height(),
		fd.width * frame_image->width(),
		fd.height * frame_image->height()
	);

	if (trackedData->draw_box.GetValue(frame_number) == 1)
	{
		auto stroke_rgba   = trackedData->stroke.GetColorRGBA(frame_number);
		int  stroke_width  = trackedData->stroke_width.GetValue(frame_number);
		float stroke_alpha = trackedData->stroke_alpha.GetValue(frame_number);
		auto bg_rgba       = trackedData->background.GetColorRGBA(frame_number);
		float bg_alpha     = trackedData->background_alpha.GetValue(frame_number);
		float bg_corner    = trackedData->background_corner.GetValue(frame_number);

		QPen pen(QColor(
			stroke_rgba[0], stroke_rgba[1], stroke_rgba[2],
			int(255 * stroke_alpha)
		));
		pen.setWidth(stroke_width);
		painter.setPen(pen);

		QBrush brush(QColor(
			bg_rgba[0], bg_rgba[1], bg_rgba[2],
			int(255 * bg_alpha)
		));
		painter.setBrush(brush);

		painter.drawRoundedRect(boxRect, bg_corner, bg_corner);
	}

	painter.end();
	return frame;
}

// Get the indexes and IDs of all visible objects in the given frame
std::string Tracker::GetVisibleObjects(int64_t frame_number) const
{
	Json::Value root;
	root["visible_objects_index"] = Json::Value(Json::arrayValue);
	root["visible_objects_id"]    = Json::Value(Json::arrayValue);

	if (trackedObjects.empty())
		return root.toStyledString();

	for (auto const& kv : trackedObjects) {
		auto ptr = kv.second;
		if (!ptr) continue;

		// Directly get the Json::Value for this object's properties
		Json::Value propsJson = ptr->PropertiesJSON(frame_number);

		if (propsJson["visible"]["value"].asBool()) {
			root["visible_objects_index"].append(kv.first);
			root["visible_objects_id"].append(ptr->Id());
		}
	}

	return root.toStyledString();
}

// Generate JSON string of this object
std::string Tracker::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Tracker::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties

	// Save the effect's properties on root
	root["type"] = info.class_name;
	root["protobuf_data_path"] = protobuf_data_path;
	root["BaseFPS"]["num"] = BaseFPS.num;
	root["BaseFPS"]["den"] = BaseFPS.den;
	root["TimeScale"] = this->TimeScale;

	// Add trackedObjects IDs to JSON
	Json::Value objects;
	for (auto const& trackedObject : trackedObjects){
		Json::Value trackedObjectJSON = trackedObject.second->JsonValue();
		// add object json
		objects[trackedObject.second->Id()] = trackedObjectJSON;
	}
	root["objects"] = objects;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Tracker::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
	return;
}

// Load Json::Value into this object
void Tracker::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	if (!root["BaseFPS"].isNull()) {
		if (!root["BaseFPS"]["num"].isNull())
			BaseFPS.num = root["BaseFPS"]["num"].asInt();
		if (!root["BaseFPS"]["den"].isNull())
			BaseFPS.den = root["BaseFPS"]["den"].asInt();
	}

	if (!root["TimeScale"].isNull()) {
		TimeScale = root["TimeScale"].asDouble();
	}

	if (!root["protobuf_data_path"].isNull()) {
		std::string new_path = root["protobuf_data_path"].asString();
		if (protobuf_data_path != new_path || trackedData->GetLength() == 0) {
			protobuf_data_path = new_path;
			if (!trackedData->LoadBoxData(protobuf_data_path)) {
				std::clog << "Invalid protobuf data path " << protobuf_data_path << '\n';
				protobuf_data_path.clear();
			}
			else {
				// prefix "<effectUUID>-<index>" for each entry
				for (auto& kv : trackedObjects) {
					auto idx = kv.first;
					auto ptr = kv.second;
					if (ptr) {
						std::string prefix = this->Id();
						if (!prefix.empty())
							prefix += "-";
						ptr->Id(prefix + std::to_string(idx));
					}
				}
			}
		}
	}

	// then any per-object JSON overrides...
	if (!root["objects"].isNull()) {
		// Iterate over the supplied objects (indexed by id or position)
		const auto memberNames = root["objects"].getMemberNames();
		for (const auto& name : memberNames)
		{
			// Determine the numeric index of this object
			int index = -1;
			bool numeric_key = std::all_of(name.begin(), name.end(), ::isdigit);
			if (numeric_key) {
				index = std::stoi(name);
			}
			else
			{
				size_t pos = name.find_last_of('-');
				if (pos != std::string::npos) {
					try {
						index = std::stoi(name.substr(pos + 1));
					} catch (...) {
						index = -1;
					}
				}
			}

			auto obj_it = trackedObjects.find(index);
			if (obj_it != trackedObjects.end() && obj_it->second) {
				// Update object id if provided as a non-numeric key
				if (!numeric_key)
					obj_it->second->Id(name);
				obj_it->second->SetJsonValue(root["objects"][name]);
			}
		}
	}

	// Set the tracked object's ids (legacy format)
	if (!root["objects_id"].isNull()) {
		for (auto& kv : trackedObjects) {
			if (!root["objects_id"][kv.first].isNull())
				kv.second->Id(root["objects_id"][kv.first].asString());
		}
	}
}

// Get all properties for a specific frame
std::string Tracker::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Add trackedObject properties to JSON
	Json::Value objects;
	for (auto const& trackedObject : trackedObjects){
		Json::Value trackedObjectJSON = trackedObject.second->PropertiesJSON(requested_frame);
		// add object json
		objects[trackedObject.second->Id()] = trackedObjectJSON;
	}
	root["objects"] = objects;

	// Return formatted string
	return root.toStyledString();
}
