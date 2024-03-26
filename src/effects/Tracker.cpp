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

/// Blank constructor, useful when using Json to load the effect properties
Tracker::Tracker(std::string clipTrackerDataPath)
{
	// Init effect properties
	init_effect_details();
	// Instantiate a TrackedObjectBBox object and point to it
	TrackedObjectBBox trackedDataObject;
	trackedData = std::make_shared<TrackedObjectBBox>(trackedDataObject);
	// Tries to load the tracked object's data from protobuf file
	trackedData->LoadBoxData(clipTrackerDataPath);
	ClipBase* parentClip = this->ParentClip();
	trackedData->ParentClip(parentClip);
	trackedData->Id(std::to_string(0));
	// Insert TrackedObject with index 0 to the trackedObjects map
	trackedObjects.insert({0, trackedData});
}

// Default constructor
Tracker::Tracker()
{
	// Init effect properties
	init_effect_details();
	// Instantiate a TrackedObjectBBox object and point to it
	TrackedObjectBBox trackedDataObject;
	trackedData = std::make_shared<TrackedObjectBBox>(trackedDataObject);
	ClipBase* parentClip = this->ParentClip();
	trackedData->ParentClip(parentClip);
	trackedData->Id(std::to_string(0));
	// Insert TrackedObject with index 0 to the trackedObjects map
	trackedObjects.insert({0, trackedData});
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
std::shared_ptr<Frame> Tracker::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number) {
    // Get the frame's QImage
    std::shared_ptr<QImage> frame_image = frame->GetImage();

    // Check if frame isn't NULL
    if(frame_image && !frame_image->isNull() &&
       trackedData->Contains(frame_number) &&
       trackedData->visible.GetValue(frame_number) == 1) {
        QPainter painter(frame_image.get());

        // Get the bounding-box of the given frame
        BBox fd = trackedData->GetBox(frame_number);

        // Create a QRectF for the bounding box
        QRectF boxRect((fd.cx - fd.width / 2) * frame_image->width(),
                       (fd.cy - fd.height / 2) * frame_image->height(),
                       fd.width * frame_image->width(),
                       fd.height * frame_image->height());

        // Check if track data exists for the requested frame
        if (trackedData->draw_box.GetValue(frame_number) == 1) {
            painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

            // Get trackedObjectBox keyframes
            std::vector<int> stroke_rgba = trackedData->stroke.GetColorRGBA(frame_number);
            int stroke_width = trackedData->stroke_width.GetValue(frame_number);
            float stroke_alpha = trackedData->stroke_alpha.GetValue(frame_number);
            std::vector<int> bg_rgba = trackedData->background.GetColorRGBA(frame_number);
            float bg_alpha = trackedData->background_alpha.GetValue(frame_number);
            float bg_corner = trackedData->background_corner.GetValue(frame_number);

            // Set the pen for the border
            QPen pen(QColor(stroke_rgba[0], stroke_rgba[1], stroke_rgba[2], 255 * stroke_alpha));
            pen.setWidth(stroke_width);
            painter.setPen(pen);

            // Set the brush for the background
            QBrush brush(QColor(bg_rgba[0], bg_rgba[1], bg_rgba[2], 255 * bg_alpha));
            painter.setBrush(brush);

            // Draw the rounded rectangle
            painter.drawRoundedRect(boxRect, bg_corner, bg_corner);
        }

        painter.end();
    }

    // No need to set the image back to the frame, as we directly modified the frame's QImage
    return frame;
}

// Get the indexes and IDs of all visible objects in the given frame
std::string Tracker::GetVisibleObjects(int64_t frame_number) const{

	// Initialize the JSON objects
	Json::Value root;
	root["visible_objects_index"] = Json::Value(Json::arrayValue);
	root["visible_objects_id"] = Json::Value(Json::arrayValue);

	// Iterate through the tracked objects
	for (const auto& trackedObject : trackedObjects){
		// Get the tracked object JSON properties for this frame
		Json::Value trackedObjectJSON = trackedObject.second->PropertiesJSON(frame_number);
		if (trackedObjectJSON["visible"]["value"].asBool()){
			// Save the object's index and ID if it's visible in this frame
			root["visible_objects_index"].append(trackedObject.first);
			root["visible_objects_id"].append(trackedObject.second->Id());
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

	if (!root["BaseFPS"].isNull() && root["BaseFPS"].isObject())
	{
		if (!root["BaseFPS"]["num"].isNull())
		{
			BaseFPS.num = (int) root["BaseFPS"]["num"].asInt();
		}
		if (!root["BaseFPS"]["den"].isNull())
		{
			BaseFPS.den = (int) root["BaseFPS"]["den"].asInt();
		}
	}

	if (!root["TimeScale"].isNull())
		TimeScale = (double) root["TimeScale"].asDouble();

	// Set data from Json (if key is found)
	if (!root["protobuf_data_path"].isNull() && protobuf_data_path.size() <= 1)
	{
		protobuf_data_path = root["protobuf_data_path"].asString();
		if(!trackedData->LoadBoxData(protobuf_data_path))
		{
			std::clog << "Invalid protobuf data path " << protobuf_data_path << '\n';
			protobuf_data_path = "";
		}
	}

	if (!root["objects"].isNull()){
		for (auto const& trackedObject : trackedObjects){
			std::string obj_id = std::to_string(trackedObject.first);
			if(!root["objects"][obj_id].isNull()){
				trackedObject.second->SetJsonValue(root["objects"][obj_id]);
			}
		}
	}

	// Set the tracked object's ids
	if (!root["objects_id"].isNull()){
		for (auto const& trackedObject : trackedObjects){
			Json::Value trackedObjectJSON;
			trackedObjectJSON["box_id"] = root["objects_id"][trackedObject.first].asString();
			trackedObject.second->SetJsonValue(trackedObjectJSON);
		}
	}

	return;
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
