/**
 * @file
 * @brief Source file for Object Detection effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <fstream>
#include <iostream>

#include "effects/ObjectDetection.h"
#include "effects/Tracker.h"
#include "Exceptions.h"
#include "Timeline.h"
#include "objdetectdata.pb.h"

#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QString>
#include <QStringList>
using namespace std;
using namespace openshot;


/// Blank constructor, useful when using Json to load the effect properties
ObjectDetection::ObjectDetection(std::string clipObDetectDataPath) :
display_box_text(1.0), display_boxes(1.0)
{
	// Init effect properties
	init_effect_details();

	// Tries to load the tracker data from protobuf
	LoadObjDetectdData(clipObDetectDataPath);

	// Initialize the selected object index as the first object index
	selectedObjectIndex = trackedObjects.begin()->first;
}

// Default constructor
ObjectDetection::ObjectDetection() :
        display_box_text(1.0), display_boxes(1.0)
{
	// Init effect properties
	init_effect_details();

	// Initialize the selected object index as the first object index
	selectedObjectIndex = trackedObjects.begin()->first;
}

// Init effect settings
void ObjectDetection::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "ObjectDetection";
	info.name = "Object Detector";
	info.description = "Detect objects through the video.";
	info.has_audio = false;
	info.has_video = true;
	info.has_tracked_object = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> ObjectDetection::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number) {
    // Get the frame's QImage
    std::shared_ptr<QImage> frame_image = frame->GetImage();

    // Check if frame isn't NULL
    if(!frame_image || frame_image->isNull()) {
        return frame;
    }

    QPainter painter(frame_image.get());
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    if (detectionsData.find(frame_number) != detectionsData.end()) {
        DetectionData detections = detectionsData[frame_number];
        for (int i = 0; i < detections.boxes.size(); i++) {
            if (detections.confidences.at(i) < confidence_threshold ||
                (!display_classes.empty() &&
                 std::find(display_classes.begin(), display_classes.end(), classNames[detections.classIds.at(i)]) == display_classes.end())) {
                continue;
            }

            int objectId = detections.objectIds.at(i);
            auto trackedObject_it = trackedObjects.find(objectId);

            if (trackedObject_it != trackedObjects.end()) {
                std::shared_ptr<TrackedObjectBBox> trackedObject = std::static_pointer_cast<TrackedObjectBBox>(trackedObject_it->second);

                Clip* parentClip = (Clip*) trackedObject->ParentClip();
                if (parentClip && trackedObject->Contains(frame_number) && trackedObject->visible.GetValue(frame_number) == 1) {
                    BBox trackedBox = trackedObject->GetBox(frame_number);
                    QRectF boxRect((trackedBox.cx - trackedBox.width / 2) * frame_image->width(),
                                   (trackedBox.cy - trackedBox.height / 2) * frame_image->height(),
                                   trackedBox.width * frame_image->width(),
                                   trackedBox.height * frame_image->height());

                    // Get properties of tracked object (i.e. colors, stroke width, etc...)
                    std::vector<int> stroke_rgba = trackedObject->stroke.GetColorRGBA(frame_number);
                    std::vector<int> bg_rgba = trackedObject->background.GetColorRGBA(frame_number);
                    int stroke_width = trackedObject->stroke_width.GetValue(frame_number);
                    float stroke_alpha = trackedObject->stroke_alpha.GetValue(frame_number);
                    float bg_alpha = trackedObject->background_alpha.GetValue(frame_number);
                    float bg_corner = trackedObject->background_corner.GetValue(frame_number);

                    // Set the pen for the border
                    QPen pen(QColor(stroke_rgba[0], stroke_rgba[1], stroke_rgba[2], 255 * stroke_alpha));
                    pen.setWidth(stroke_width);
                    painter.setPen(pen);

                    // Set the brush for the background
                    QBrush brush(QColor(bg_rgba[0], bg_rgba[1], bg_rgba[2], 255 * bg_alpha));
                    painter.setBrush(brush);

                    if (display_boxes.GetValue(frame_number) == 1 && trackedObject->draw_box.GetValue(frame_number) == 1) {
                        // Only draw boxes if both properties are set to YES (draw all boxes, and draw box of the selected box)
                        painter.drawRoundedRect(boxRect, bg_corner, bg_corner);
                    }

                    if(display_box_text.GetValue(frame_number) == 1) {
                        // Draw text label above bounding box
                        // Get the confidence and classId for the current detection
                        int classId = detections.classIds.at(i);

                        // Get the label for the class name and its confidence
                        QString label = QString::number(objectId);
                        if (!classNames.empty()) {
                            label = QString::fromStdString(classNames[classId]) + ":" + label;
                        }

                        // Set up the painter, font, and pen
                        QFont font;
                        font.setPixelSize(14);
                        painter.setFont(font);

                        // Calculate the size of the text
                        QFontMetrics fontMetrics(font);
                        QSize labelSize = fontMetrics.size(Qt::TextSingleLine, label);

                        // Define the top left point of the rectangle
                        double left = boxRect.center().x() - (labelSize.width() / 2.0);
                        double top = std::max(static_cast<int>(boxRect.top()), labelSize.height()) - 4.0;

                        // Draw the text
                        painter.drawText(QPointF(left, top), label);
                    }
                }
            }
        }
    }

    painter.end();

    // The frame's QImage has been modified in place, so we just return the original frame
    return frame;
}

// Load protobuf data file
bool ObjectDetection::LoadObjDetectdData(std::string inputFilePath){
	// Create tracker message
	pb_objdetect::ObjDetect objMessage;

	// Read the existing tracker message.
	std::fstream input(inputFilePath, std::ios::in | std::ios::binary);
	if (!objMessage.ParseFromIstream(&input)) {
		std::cerr << "Failed to parse protobuf message." << std::endl;
		return false;
	}

	// Make sure classNames, detectionsData and trackedObjects are empty
	classNames.clear();
	detectionsData.clear();
	trackedObjects.clear();

	// Seed to generate same random numbers
	std::srand(1);
	// Get all classes names and assign a color to them
	for(int i = 0; i < objMessage.classnames_size(); i++)
	{
		classNames.push_back(objMessage.classnames(i));
		classesColor.push_back(cv::Scalar(std::rand()%205 + 50, std::rand()%205 + 50, std::rand()%205 + 50));
	}

	// Iterate over all frames of the saved message
	for (size_t i = 0; i < objMessage.frame_size(); i++)
	{
		// Create protobuf message reader
		const pb_objdetect::Frame& pbFrameData = objMessage.frame(i);

		// Get frame Id
		size_t id = pbFrameData.id();

		// Load bounding box data
		const google::protobuf::RepeatedPtrField<pb_objdetect::Frame_Box > &pBox = pbFrameData.bounding_box();

		// Construct data vectors related to detections in the current frame
		std::vector<int> classIds;
		std::vector<float> confidences;
		std::vector<cv::Rect_<float>> boxes;
		std::vector<int> objectIds;

		// Iterate through the detected objects
		for(int i = 0; i < pbFrameData.bounding_box_size(); i++)
		{
			// Get bounding box coordinates
			float x = pBox.Get(i).x();
			float y = pBox.Get(i).y();
			float w = pBox.Get(i).w();
			float h = pBox.Get(i).h();
			// Get class Id (which will be assign to a class name)
			int classId = pBox.Get(i).classid();
			// Get prediction confidence
			float confidence = pBox.Get(i).confidence();

			// Get the object Id
			int objectId = pBox.Get(i).objectid();

			// Search for the object id on trackedObjects map
			auto trackedObject = trackedObjects.find(objectId);
			// Check if object already exists on the map
			if (trackedObject != trackedObjects.end())
			{
				// Add a new BBox to it
				trackedObject->second->AddBox(id, x+(w/2), y+(h/2), w, h, 0.0);
			}
			else
			{
				// There is no tracked object with that id, so insert a new one
				TrackedObjectBBox trackedObj((int)classesColor[classId](0), (int)classesColor[classId](1), (int)classesColor[classId](2), (int)0);
                trackedObj.stroke_alpha = Keyframe(1.0);
				trackedObj.AddBox(id, x+(w/2), y+(h/2), w, h, 0.0);

				std::shared_ptr<TrackedObjectBBox> trackedObjPtr = std::make_shared<TrackedObjectBBox>(trackedObj);
				ClipBase* parentClip = this->ParentClip();
				trackedObjPtr->ParentClip(parentClip);

				// Create a temp ID. This ID is necessary to initialize the object_id Json list
				// this Id will be replaced by the one created in the UI
				trackedObjPtr->Id(std::to_string(objectId));
				trackedObjects.insert({objectId, trackedObjPtr});
			}

			// Create OpenCV rectangle with the bouding box info
			cv::Rect_<float> box(x, y, w, h);

			// Push back data into vectors
			boxes.push_back(box);
			classIds.push_back(classId);
			confidences.push_back(confidence);
			objectIds.push_back(objectId);
		}

		// Assign data to object detector map
		detectionsData[id] = DetectionData(classIds, confidences, boxes, id, objectIds);
	}

	// Delete all global objects allocated by libprotobuf.
	google::protobuf::ShutdownProtobufLibrary();

	return true;
}

// Get the indexes and IDs of all visible objects in the given frame
std::string ObjectDetection::GetVisibleObjects(int64_t frame_number) const{

	// Initialize the JSON objects
	Json::Value root;
	root["visible_objects_index"] = Json::Value(Json::arrayValue);
	root["visible_objects_id"] = Json::Value(Json::arrayValue);
    root["visible_class_names"] = Json::Value(Json::arrayValue);

	// Check if track data exists for the requested frame
	if (detectionsData.find(frame_number) == detectionsData.end()){
		return root.toStyledString();
	}
	DetectionData detections = detectionsData.at(frame_number);

	// Iterate through the tracked objects
	for(int i = 0; i<detections.boxes.size(); i++){
		// Does not show boxes with confidence below the threshold
		if(detections.confidences.at(i) < confidence_threshold){
			continue;
		}

		// Get class name of tracked object
        auto className = classNames[detections.classIds.at(i)];

        // If display_classes is not empty, check if className is in it
        if (!display_classes.empty()) {
            auto it = std::find(display_classes.begin(), display_classes.end(), className);
            if (it == display_classes.end()) {
                // If not in display_classes, skip this detection
                continue;
            }
            root["visible_class_names"].append(className);
        } else {
            // include all class names
            root["visible_class_names"].append(className);
        }

		int objectId = detections.objectIds.at(i);
		// Search for the object in the trackedObjects map
		auto trackedObject = trackedObjects.find(objectId);

		// Get the tracked object JSON properties for this frame
		Json::Value trackedObjectJSON = trackedObject->second->PropertiesJSON(frame_number);

		if (trackedObjectJSON["visible"]["value"].asBool() &&
			trackedObject->second->ExactlyContains(frame_number)){
			// Save the object's index and ID if it's visible in this frame
			root["visible_objects_index"].append(trackedObject->first);
			root["visible_objects_id"].append(trackedObject->second->Id());
		}
	}

	return root.toStyledString();
}

// Generate JSON string of this object
std::string ObjectDetection::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value ObjectDetection::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["protobuf_data_path"] = protobuf_data_path;
	root["selected_object_index"] = selectedObjectIndex;
	root["confidence_threshold"] = confidence_threshold;
	root["display_box_text"] = display_box_text.JsonValue();
	root["display_boxes"] = display_boxes.JsonValue();

	// Add tracked object's IDs to root
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
void ObjectDetection::SetJson(const std::string value) {

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
void ObjectDetection::SetJsonValue(const Json::Value root) {
	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["protobuf_data_path"].isNull() && protobuf_data_path.size() <= 1){
		protobuf_data_path = root["protobuf_data_path"].asString();

		if(!LoadObjDetectdData(protobuf_data_path)){
			throw InvalidFile("Invalid protobuf data path", "");
			protobuf_data_path = "";
		}
	}

	// Set the selected object index
	if (!root["selected_object_index"].isNull())
		selectedObjectIndex = root["selected_object_index"].asInt();

	if (!root["confidence_threshold"].isNull())
		confidence_threshold = root["confidence_threshold"].asFloat();

	if (!root["display_box_text"].isNull())
		display_box_text.SetJsonValue(root["display_box_text"]);

    if (!root["display_boxes"].isNull())
        display_boxes.SetJsonValue(root["display_boxes"]);

    if (!root["class_filter"].isNull()) {
        class_filter = root["class_filter"].asString();

        // Convert the class_filter to a QString
        QString qClassFilter = QString::fromStdString(root["class_filter"].asString());

        // Split the QString by commas and automatically trim each resulting string
        QStringList classList = qClassFilter.split(',', QString::SkipEmptyParts);
        display_classes.clear();

        // Iterate over the QStringList and add each trimmed, non-empty string
        for (const QString &classItem : classList) {
            QString trimmedItem = classItem.trimmed().toLower();
            if (!trimmedItem.isEmpty()) {
                display_classes.push_back(trimmedItem.toStdString());
            }
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
}

// Get all properties for a specific frame
std::string ObjectDetection::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	Json::Value objects;
	if(trackedObjects.count(selectedObjectIndex) != 0){
		auto selectedObject = trackedObjects.at(selectedObjectIndex);
		if (selectedObject){
			Json::Value trackedObjectJSON = selectedObject->PropertiesJSON(requested_frame);
			// add object json
			objects[selectedObject->Id()] = trackedObjectJSON;
		}
	}
	root["objects"] = objects;

	root["selected_object_index"] = add_property_json("Selected Object", selectedObjectIndex, "int", "", NULL, 0, 200, false, requested_frame);
	root["confidence_threshold"] = add_property_json("Confidence Theshold", confidence_threshold, "float", "", NULL, 0, 1, false, requested_frame);
	root["class_filter"] = add_property_json("Class Filter", 0.0, "string", class_filter, NULL, -1, -1, false, requested_frame);

	root["display_box_text"] = add_property_json("Draw All Text", display_box_text.GetValue(requested_frame), "int", "", &display_box_text, 0, 1, false, requested_frame);
	root["display_box_text"]["choices"].append(add_property_choice_json("Yes", true, display_box_text.GetValue(requested_frame)));
	root["display_box_text"]["choices"].append(add_property_choice_json("No", false, display_box_text.GetValue(requested_frame)));

    root["display_boxes"] = add_property_json("Draw All Boxes", display_boxes.GetValue(requested_frame), "int", "", &display_boxes, 0, 1, false, requested_frame);
    root["display_boxes"]["choices"].append(add_property_choice_json("Yes", true, display_boxes.GetValue(requested_frame)));
    root["display_boxes"]["choices"].append(add_property_choice_json("No", false, display_boxes.GetValue(requested_frame)));

	// Return formatted string
	return root.toStyledString();
}
