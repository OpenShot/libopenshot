/**
 * @file
 * @brief Source file for Tracker effect class
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

#include "effects/Tracker.h"
#include <google/protobuf/util/time_util.h>
#include "Timeline.h"

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
std::shared_ptr<Frame> Tracker::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
    // Get the frame's image
	cv::Mat frame_image = frame->GetImageCV();

    // Check if frame isn't NULL
    if(!frame_image.empty())
	{
        // Check if track data exists for the requested frame
        if (trackedData->Contains(frame_number)) 
		{
			// Get the width and height of the image
			float fw = frame_image.size().width;
        	float fh = frame_image.size().height;

			// Get the bounding-box of given frame
			BBox fd = trackedData->GetBox(frame_number);

			// Create a rotated rectangle object that holds the bounding box
			cv::RotatedRect box ( cv::Point2f( (int)(fd.cx*fw), (int)(fd.cy*fh) ), 
								  cv::Size2f( (int)(fd.width*fw), (int)(fd.height*fh) ), 
								  (int) (fd.angle) );
			// Get the bouding box vertices
			cv::Point2f vertices[4];
			box.points(vertices);
			// Draw the bounding-box on the image
			for (int i = 0; i < 4; i++)
			{
				cv::line(frame_image, vertices[i], vertices[(i+1)%4], cv::Scalar(255,0,0), 2);
			}
        }
    }

	// Set image with drawn box to frame
    // If the input image is NULL or doesn't have tracking data, it's returned as it came
	frame->SetImageCV(frame_image);

	return frame;
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

	// Get trackedData JSON 
	Json::Value trackedDataJSON;
	trackedDataJSON = trackedData->JsonValue();

	// Save the trackedData properties on root
	for (auto const& trackedObject : trackedObjects){
	    Json::Value trackedObjectJSON = trackedObject.second->JsonValue();
		// Save the trackedObject JSON on root
        root["box_id-"+to_string(trackedObject.first)] = trackedObjectJSON["box_id"];
        root["delta_x-"+to_string(trackedObject.first)] = trackedObjectJSON["delta_x"];
        root["delta_y-"+to_string(trackedObject.first)] = trackedObjectJSON["delta_y"];
        root["scale_x-"+to_string(trackedObject.first)] = trackedObjectJSON["scale_x"];
        root["scale_y-"+to_string(trackedObject.first)] = trackedObjectJSON["scale_y"];
        root["rotation-"+to_string(trackedObject.first)] = trackedObjectJSON["rotation"];
	}

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

	if(!root["type"].isNull())
		info.class_name = root["type"].asString();

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
	if (!root["protobuf_data_path"].isNull())
	{
		protobuf_data_path = (root["protobuf_data_path"].asString());
		if(!trackedData->LoadBoxData(protobuf_data_path))
		{
			std::cout<<"Invalid protobuf data path";
			protobuf_data_path = "";
		}
	}

	for (auto const& trackedObject : trackedObjects){
		Json::Value trackedObjectJSON;
        trackedObjectJSON["box_id"] = root["box_id-"+to_string(trackedObject.first)];
        trackedObjectJSON["delta_x"] = root["delta_x-"+to_string(trackedObject.first)];
        trackedObjectJSON["delta_y"] = root["delta_y-"+to_string(trackedObject.first)];
        trackedObjectJSON["scale_x"] = root["scale_x-"+to_string(trackedObject.first)];
        trackedObjectJSON["scale_y"] = root["scale_y-"+to_string(trackedObject.first)];
        trackedObjectJSON["rotation"] = root["rotation-"+to_string(trackedObject.first)];
		if (!trackedObjectJSON.isNull())
			trackedObject.second->SetJsonValue(trackedObjectJSON);
	}

	return;
}


// Get all properties for a specific frame
std::string Tracker::PropertiesJSON(int64_t requested_frame) const {
	
	// Generate JSON properties list
	Json::Value root; 
	
	// Add trackedObjects properties to JSON
	for (auto const& trackedObject : trackedObjects){
		Json::Value trackedObjectJSON = trackedObject.second->PropertiesJSON(requested_frame);
        root["box_id-"+to_string(trackedObject.first)] = trackedObjectJSON["box_id"];
        root["x1-"+to_string(trackedObject.first)] = trackedObjectJSON["x1"];
        root["y1-"+to_string(trackedObject.first)] = trackedObjectJSON["y1"];
        root["x2-"+to_string(trackedObject.first)] = trackedObjectJSON["x2"];
        root["y2-"+to_string(trackedObject.first)] = trackedObjectJSON["y2"];
        root["delta_x-"+to_string(trackedObject.first)] = trackedObjectJSON["delta_x"];
        root["delta_y-"+to_string(trackedObject.first)] = trackedObjectJSON["delta_y"];
        root["scale_x-"+to_string(trackedObject.first)] = trackedObjectJSON["scale_x"];
        root["scale_y-"+to_string(trackedObject.first)] = trackedObjectJSON["scale_y"];
        root["rotation-"+to_string(trackedObject.first)] = trackedObjectJSON["rotation"];
	}

	// Append effect's properties
	root["name"] = add_property_json("Tracker", 0.0, "string", "", NULL, -1, -1, true, requested_frame);
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
