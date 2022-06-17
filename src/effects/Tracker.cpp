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
#include <fstream>
#include <iostream>

#include "effects/Tracker.h"
#include "Exceptions.h"
#include "Timeline.h"
#include "trackerdata.pb.h"

#include <google/protobuf/util/time_util.h>

#include <QImage>
#include <QPainter>
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
std::shared_ptr<Frame> Tracker::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
    // Get the frame's image
	cv::Mat frame_image = frame->GetImageCV();

	// Initialize the Qt rectangle that will hold the positions of the bounding-box
	QRectF boxRect;
	// Initialize the image of the TrackedObject child clip
	std::shared_ptr<QImage> childClipImage = nullptr;

    // Check if frame isn't NULL
    if(!frame_image.empty() &&
		trackedData->Contains(frame_number) &&
		trackedData->visible.GetValue(frame_number) == 1)
	{
		// Get the width and height of the image
		float fw = frame_image.size().width;
		float fh = frame_image.size().height;

		// Get the bounding-box of given frame
		BBox fd = trackedData->GetBox(frame_number);

        // Check if track data exists for the requested frame
        if (trackedData->draw_box.GetValue(frame_number) == 1)
		{
			std::vector<int> stroke_rgba = trackedData->stroke.GetColorRGBA(frame_number);
			int stroke_width = trackedData->stroke_width.GetValue(frame_number);
			float stroke_alpha = trackedData->stroke_alpha.GetValue(frame_number);
			std::vector<int> bg_rgba = trackedData->background.GetColorRGBA(frame_number);
			float bg_alpha = trackedData->background_alpha.GetValue(frame_number);

			// Create a rotated rectangle object that holds the bounding box
			cv::RotatedRect box ( cv::Point2f( (int)(fd.cx*fw), (int)(fd.cy*fh) ),
								  cv::Size2f( (int)(fd.width*fw), (int)(fd.height*fh) ),
								  (int) (fd.angle) );

			DrawRectangleRGBA(frame_image, box, bg_rgba, bg_alpha, 1, true);
			DrawRectangleRGBA(frame_image, box, stroke_rgba, stroke_alpha, stroke_width, false);
		}

		// Get the image of the Tracked Object' child clip
		if (trackedData->ChildClipId() != ""){
			// Cast the parent timeline of this effect
			Timeline* parentTimeline = (Timeline *) ParentTimeline();
			if (parentTimeline){
				// Get the Tracked Object's child clip
				Clip* childClip = parentTimeline->GetClip(trackedData->ChildClipId());
				if (childClip){
					// Get the image of the child clip for this frame
					std::shared_ptr<Frame> f(new Frame(1, frame->GetWidth(), frame->GetHeight(), "#00000000"));
					std::shared_ptr<Frame> childClipFrame = childClip->GetFrame(f, frame_number);
					childClipImage = childClipFrame->GetImage();

					// Set the Qt rectangle with the bounding-box properties
					boxRect.setRect((int)((fd.cx-fd.width/2)*fw),
									(int)((fd.cy - fd.height/2)*fh),
									(int)(fd.width*fw),
									(int)(fd.height*fh) );
				}
			}
		}

    }

	// Set image with drawn box to frame
    // If the input image is NULL or doesn't have tracking data, it's returned as it came
	frame->SetImageCV(frame_image);

	// Set the bounding-box image with the Tracked Object's child clip image
	if (childClipImage){
		// Get the frame image
		QImage frameImage = *(frame->GetImage());

		// Set a Qt painter to the frame image
		QPainter painter(&frameImage);

		// Draw the child clip image inside the bounding-box
		painter.drawImage(boxRect, *childClipImage, QRectF(0, 0, frameImage.size().width(),  frameImage.size().height()));

		// Set the frame image as the composed image
		frame->AddImage(std::make_shared<QImage>(frameImage));
	}

	return frame;
}

void Tracker::DrawRectangleRGBA(cv::Mat &frame_image, cv::RotatedRect box, std::vector<int> color, float alpha, int thickness, bool is_background){
	// Get the bouding box vertices
	cv::Point2f vertices2f[4];
	box.points(vertices2f);

	// TODO: take a rectangle of frame_image by refencence and draw on top of that to improve speed
	// select min enclosing rectangle to draw on a small portion of the image
	// cv::Rect rect  = box.boundingRect();
	// cv::Mat image = frame_image(rect)

	if(is_background){
		cv::Mat overlayFrame;
		frame_image.copyTo(overlayFrame);

		// draw bounding box background
		cv::Point vertices[4];
		for(int i = 0; i < 4; ++i){
			vertices[i] = vertices2f[i];}

		cv::Rect rect  = box.boundingRect();
		cv::fillConvexPoly(overlayFrame, vertices, 4, cv::Scalar(color[2],color[1],color[0]), cv::LINE_AA);
		// add opacity
		cv::addWeighted(overlayFrame, 1-alpha, frame_image, alpha, 0, frame_image);
	}
	else{
		cv::Mat overlayFrame;
		frame_image.copyTo(overlayFrame);

		// Draw bounding box
		for (int i = 0; i < 4; i++)
		{
			cv::line(overlayFrame, vertices2f[i], vertices2f[(i+1)%4], cv::Scalar(color[2],color[1],color[0]),
						thickness, cv::LINE_AA);
		}

		// add opacity
		cv::addWeighted(overlayFrame, 1-alpha, frame_image, alpha, 0, frame_image);
	}
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
	Json::Value root;

	// Add trackedObject properties to JSON
	Json::Value objects;
	for (auto const& trackedObject : trackedObjects){
		Json::Value trackedObjectJSON = trackedObject.second->PropertiesJSON(requested_frame);
		// add object json
        objects[trackedObject.second->Id()] = trackedObjectJSON;
    }
	root["objects"] = objects;

	// Append effect's properties
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
