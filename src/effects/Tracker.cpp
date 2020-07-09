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

#include "../../include/effects/Tracker.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Tracker::Tracker(std::string clipTrackerDataPath)
{   
    // Init effect properties
	init_effect_details();

    // Tries to load the tracker data from protobuf
    LoadTrackedData(clipTrackerDataPath);
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
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Tracker::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
    // Get the frame's image
	cv::Mat frame_image = frame->GetImageCV();

    // Draw box on image
    FrameData fd = trackedDataById[frame_number];
    cv::Rect2d box(fd.x1, fd.y1, fd.x2-fd.x1, fd.y2-fd.y1);
    cv::rectangle(frame_image, box, cv::Scalar( 255, 0, 0 ), 2, 1 );

	// Set image with drawn box to frame
	frame->SetImageCV(frame_image);

	return frame;
}

// Load protobuf data file
bool Tracker::LoadTrackedData(std::string inputFilePath){
    // Create tracker message
    libopenshottracker::Tracker trackerMessage;

    {
        // Read the existing tracker message.
        fstream input(inputFilePath, ios::in | ios::binary);
        if (!trackerMessage.ParseFromIstream(&input)) {
            cerr << "Failed to parse protobuf message." << endl;
            return false;
        }
    }

    // Make sure the trackedData is empty
    trackedDataById.clear();

    // Iterate over all frames of the saved message
    for (int i = 0; i < trackerMessage.frame_size(); i++) {
        const libopenshottracker::Frame& pbFrameData = trackerMessage.frame(i);

        // Load frame and rotation data
        int id = pbFrameData.id();
        float rotation = pbFrameData.rotation();

        // Load bounding box data
        const libopenshottracker::Frame::Box& box = pbFrameData.bounding_box();
        int x1 = box.x1();
        int y1 = box.y1();
        int x2 = box.x2();
        int y2 = box.y2();

        // Assign data to tracker map
        trackedDataById[id] = FrameData(id, rotation, x1, y1, x2, y2);
    }

    // Show the time stamp from the last update in tracker data file 
    if (trackerMessage.has_last_updated()) {
        cout << "  Loaded Data. Saved Time Stamp: " << TimeUtil::ToString(trackerMessage.last_updated()) << endl;
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}

// Get tracker info for the desired frame 
FrameData Tracker::GetTrackedData(int frameId){

	// Check if the tracker info for the requested frame exists
    if ( trackedDataById.find(frameId) == trackedDataById.end() ) {
        return FrameData();
    } else {
        return trackedDataById[frameId];
    }

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
}

// Get all properties for a specific frame
std::string Tracker::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
