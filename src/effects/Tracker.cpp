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

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Tracker::Tracker(std::string clipTrackerDataPath)
{   
    // Init effect properties
	init_effect_details();
    // Tries to load the tracked object's data from protobuf file
	trackedData.LoadBoxData(clipTrackerDataPath);
}

// Default constructor
Tracker::Tracker()
{
	// Init effect properties
	init_effect_details();
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
        if (trackedData.Contains(frame_number)) 
		{
			// Get the width and height of the image
			float fw = frame_image.size().width;
        	float fh = frame_image.size().height;

			// Get the bounding-box of given frame
			BBox fd = this->trackedData.GetValue(frame_number);

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
	root["type"] = info.class_name;
	root["protobuf_data_path"] = protobuf_data_path;
    root["BaseFPS"]["num"] = BaseFPS.num;
	root["BaseFPS"]["den"] = BaseFPS.den;
	root["TimeScale"] = this->TimeScale;
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

	trackedData.SetBaseFPS(this->BaseFPS);
	trackedData.ScalePoints(TimeScale);

	// Set data from Json (if key is found)
	if (!root["protobuf_data_path"].isNull())
	{
		protobuf_data_path = (root["protobuf_data_path"].asString());
		if(!trackedData.LoadBoxData(protobuf_data_path))
		{
			std::cout<<"Invalid protobuf data path";
			protobuf_data_path = "";
		}
	}
}


// Get all properties for a specific frame
std::string Tracker::PropertiesJSON(int64_t requested_frame) const {
	
	// Generate JSON properties list
	Json::Value root;

	// Effect's properties
	root["name"] = add_property_json("Tracker", 0.0, "string", "", NULL, -1, -1, true, requested_frame);
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Get the bounding-box for the given-frame
	BBox fd = trackedData.GetValue(requested_frame);
	// Add the data of given frame bounding-box to the JSON object
	root["x1"] = add_property_json("X1", fd.cx-(fd.width/2), "float", "", NULL, 0.0, 1.0, false, requested_frame);
	root["y1"] = add_property_json("Y1", fd.cy-(fd.height/2), "float", "", NULL, 0.0, 1.0, false, requested_frame);
	root["x2"] = add_property_json("X2", fd.cx+(fd.width/2), "float", "", NULL, 0.0, 1.0, false, requested_frame);
	root["y2"] = add_property_json("Y2", fd.cy+(fd.height/2), "float", "", NULL, 0.0, 1.0, false, requested_frame);

	// Add the bounding-box Keyframes to the JSON object
	root["delta_x"] = add_property_json("Displacement X-axis", trackedData.delta_x.GetValue(requested_frame), "float", "", &trackedData.delta_x, -1.0, 1.0, false, requested_frame);
	root["delta_y"] = add_property_json("Displacement Y-axis", trackedData.delta_y.GetValue(requested_frame), "float", "", &trackedData.delta_y, -1.0, 1.0, false, requested_frame);
	root["scale_x"] = add_property_json("Scale (Width)", trackedData.scale_x.GetValue(requested_frame), "float", "", &trackedData.scale_x, -1.0, 1.0, false, requested_frame);
	root["scale_y"] = add_property_json("Scale (Height)", trackedData.scale_y.GetValue(requested_frame), "float", "", &trackedData.scale_y, -1.0, 1.0, false, requested_frame);
	root["rotation"] = add_property_json("Rotation", trackedData.rotation.GetValue(requested_frame), "float", "", &trackedData.rotation, 0, 360, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}

// Generate JSON string of the trackedData object passing the frame number
std::string Tracker::Json(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;

	// Add the KeyframeBBox class properties to the JSON object
	root["type"] = info.class_name;
	root["protobuf_data_path"] = protobuf_data_path;
    root["BaseFPS"]["num"] = BaseFPS.num;
	root["BaseFPS"]["den"] = BaseFPS.den;
	root["TimeScale"] = this->TimeScale;

	// Add the bounding-box Keyframes to the JSON object
	root["delta_x"] = trackedData.delta_x.JsonValue();
	root["delta_y"] = trackedData.delta_y.JsonValue();
	root["scale_x"] = trackedData.scale_x.JsonValue();
	root["scale_y"] = trackedData.scale_y.JsonValue();
	root["rotation"] = trackedData.rotation.JsonValue();

	return root.toStyledString();
}

// Set the tracketData object properties by a JSON string
void Tracker::SetJson(int64_t requested_frame, const std::string value) 
{
	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		
		// Set all values that match
		if (!root["delta_x"].isNull())
			trackedData.delta_x.SetJsonValue(root["delta_x"]);
		if (!root["delta_y"].isNull())
			trackedData.delta_y.SetJsonValue(root["delta_y"]);
		if (!root["scale_x"].isNull())
			trackedData.scale_x.SetJsonValue(root["scale_x"]);
		if (!root["scale_y"].isNull())
			trackedData.scale_y.SetJsonValue(root["scale_y"]);
		if (!root["rotation"].isNull())
			trackedData.rotation.SetJsonValue(root["rotation"]);
	}

	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
	return;
}
