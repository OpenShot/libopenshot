/**
 * @file
 * @brief Source file for Stabilizer effect class
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

#include "../../include/effects/Stabilizer.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Stabilizer::Stabilizer(std::string clipStabilizedDataPath)
{   
    // Init effect properties
	init_effect_details();

    // Tries to load the stabilization data from protobuf
    LoadStabilizedData(clipStabilizedDataPath);
}

// Default constructor
Stabilizer::Stabilizer(Color color, Keyframe left, Keyframe top, Keyframe right, Keyframe bottom) :
		color(color), left(left), top(top), right(right), bottom(bottom)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Stabilizer::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Stabilizer";
	info.name = "Stabilizer";
	info.description = "Stabilize video clip to remove undesired shaking and jitter.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Stabilizer::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
	cv::Mat T(2,3,CV_64F);
	// Grab Mat image
	cv::Mat cur = frame->GetImageCV();
	T.at<double>(0,0) = cos(transformationData[frame_number].da);
	T.at<double>(0,1) = -sin(transformationData[frame_number].da);
	T.at<double>(1,0) = sin(transformationData[frame_number].da);
	T.at<double>(1,1) = cos(transformationData[frame_number].da);

	T.at<double>(0,2) = transformationData[frame_number].dx;
	T.at<double>(1,2) = transformationData[frame_number].dy;
	cv::Mat frame_stabilized;

	cv::warpAffine(cur, frame_stabilized, T, cur.size());
	// Scale up the image to remove black borders
	cv::Mat T_scale = cv::getRotationMatrix2D(cv::Point2f(frame_stabilized.cols/2, frame_stabilized.rows/2), 0, 1.04); 
  	cv::warpAffine(frame_stabilized, frame_stabilized, T_scale, frame_stabilized.size()); 
	frame->SetImageCV(frame_stabilized);
	
	return frame;
}

// Load protobuf file
bool Stabilizer::LoadStabilizedData(std::string inputFilePath){
    libopenshotstabilize::Stabilization stabilizationMessage;

    // Read the existing tracker message.
    fstream input(inputFilePath, ios::in | ios::binary);
    if (!stabilizationMessage.ParseFromIstream(&input)) {
        cerr << "Failed to parse protobuf message." << endl;
        return false;
    }

    // Make sure the data vectors are empty
    transformationData.clear();
    trajectoryData.clear();

    // Iterate over all frames of the saved message
    for (int i = 0; i < stabilizationMessage.frame_size(); i++) {
        const libopenshotstabilize::Frame& pbFrameData = stabilizationMessage.frame(i);

        int id = pbFrameData.id();

        float x = pbFrameData.x();
        float y = pbFrameData.y();
        float a = pbFrameData.a();

        trajectoryData.push_back(CamTrajectory(x,y,a));

        float dx = pbFrameData.dx();
        float dy = pbFrameData.dy();
        float da = pbFrameData.da();

        transformationData.push_back(TransformParam(dx,dy,da));
    }

    if (stabilizationMessage.has_last_updated()) {
        cout << "  Loaded Data. Saved Time Stamp: " << TimeUtil::ToString(stabilizationMessage.last_updated()) << endl;
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}



// Generate JSON string of this object
std::string Stabilizer::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Stabilizer::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["color"] = color.JsonValue();
	root["left"] = left.JsonValue();
	root["top"] = top.JsonValue();
	root["right"] = right.JsonValue();
	root["bottom"] = bottom.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Stabilizer::SetJson(const std::string value) {

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
void Stabilizer::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["color"].isNull())
		color.SetJsonValue(root["color"]);
	if (!root["left"].isNull())
		left.SetJsonValue(root["left"]);
	if (!root["top"].isNull())
		top.SetJsonValue(root["top"]);
	if (!root["right"].isNull())
		right.SetJsonValue(root["right"]);
	if (!root["bottom"].isNull())
		bottom.SetJsonValue(root["bottom"]);
}

// Get all properties for a specific frame
std::string Stabilizer::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["color"] = add_property_json("Bar Color", 0.0, "color", "", NULL, 0, 255, false, requested_frame);
	root["color"]["red"] = add_property_json("Red", color.red.GetValue(requested_frame), "float", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["blue"] = add_property_json("Blue", color.blue.GetValue(requested_frame), "float", "", &color.blue, 0, 255, false, requested_frame);
	root["color"]["green"] = add_property_json("Green", color.green.GetValue(requested_frame), "float", "", &color.green, 0, 255, false, requested_frame);
	root["left"] = add_property_json("Left Size", left.GetValue(requested_frame), "float", "", &left, 0.0, 0.5, false, requested_frame);
	root["top"] = add_property_json("Top Size", top.GetValue(requested_frame), "float", "", &top, 0.0, 0.5, false, requested_frame);
	root["right"] = add_property_json("Right Size", right.GetValue(requested_frame), "float", "", &right, 0.0, 0.5, false, requested_frame);
	root["bottom"] = add_property_json("Bottom Size", bottom.GetValue(requested_frame), "float", "", &bottom, 0.0, 0.5, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
