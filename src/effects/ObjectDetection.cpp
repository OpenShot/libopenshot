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
#include <algorithm>

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


// Default constructor
ObjectDetection::ObjectDetection()
  : display_box_text(1.0)
  , display_boxes(1.0)
{
	// Init effect metadata
	init_effect_details();

	// We haven’t loaded any protobuf yet, so there's nothing to pick.
	selectedObjectIndex = -1;
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
bool ObjectDetection::LoadObjDetectdData(std::string inputFilePath)
{
    // Parse the file
    pb_objdetect::ObjDetect objMessage;
    std::fstream input(inputFilePath, std::ios::in | std::ios::binary);
    if (!objMessage.ParseFromIstream(&input)) {
        std::cerr << "Failed to parse protobuf message." << std::endl;
        return false;
    }

    // Clear out any old state
    classNames.clear();
    detectionsData.clear();
    trackedObjects.clear();

    // Seed colors for each class
    std::srand(1);
    for (int i = 0; i < objMessage.classnames_size(); ++i) {
        classNames.push_back(objMessage.classnames(i));
        classesColor.push_back(cv::Scalar(
            std::rand() % 205 + 50,
            std::rand() % 205 + 50,
            std::rand() % 205 + 50
        ));
    }

    // Walk every frame in the protobuf
    for (size_t fi = 0; fi < objMessage.frame_size(); ++fi) {
        const auto &pbFrame = objMessage.frame(fi);
        size_t frameId = pbFrame.id();

        // Buffers for DetectionData
        std::vector<int>   classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect_<float>> boxes;
        std::vector<int>   objectIds;

        // For each bounding box in this frame
        for (int di = 0; di < pbFrame.bounding_box_size(); ++di) {
            const auto &b = pbFrame.bounding_box(di);
            float x = b.x(), y = b.y(), w = b.w(), h = b.h();
            int   classId   = b.classid();
            float confidence= b.confidence();
            int   objectId  = b.objectid();

            // Record for DetectionData
            classIds.push_back(classId);
            confidences.push_back(confidence);
            boxes.emplace_back(x, y, w, h);
            objectIds.push_back(objectId);

            // Either append to an existing TrackedObjectBBox…
            auto it = trackedObjects.find(objectId);
            if (it != trackedObjects.end()) {
                it->second->AddBox(frameId, x + w/2, y + h/2, w, h, 0.0);
            }
            else {
                // …or create a brand-new one
                TrackedObjectBBox tmpObj(
                    (int)classesColor[classId][0],
                    (int)classesColor[classId][1],
                    (int)classesColor[classId][2],
                    /*alpha=*/0
                );
                tmpObj.stroke_alpha = Keyframe(1.0);
                tmpObj.AddBox(frameId, x + w/2, y + h/2, w, h, 0.0);

				auto ptr = std::make_shared<TrackedObjectBBox>(tmpObj);
				ptr->ParentClip(this->ParentClip());

				// Prefix with effect UUID for a unique string ID
				std::string prefix = this->Id();
				if (!prefix.empty())
					prefix += "-";
				ptr->Id(prefix + std::to_string(objectId));
				trackedObjects.emplace(objectId, ptr);
			}
		}

		// Save the DetectionData for this frame
        detectionsData[frameId] = DetectionData(
            classIds, confidences, boxes, frameId, objectIds
        );
    }

    google::protobuf::ShutdownProtobufLibrary();

    // Finally, pick a default selectedObjectIndex if we have any
    if (!trackedObjects.empty()) {
        selectedObjectIndex = trackedObjects.begin()->first;
    }

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
void ObjectDetection::SetJsonValue(const Json::Value root)
{
	// Parent properties
	EffectBase::SetJsonValue(root);

	// If a protobuf path is provided, load & prefix IDs
	if (!root["protobuf_data_path"].isNull()) {
		std::string new_path = root["protobuf_data_path"].asString();
		if (protobuf_data_path != new_path || trackedObjects.empty()) {
			protobuf_data_path = new_path;
			if (!LoadObjDetectdData(protobuf_data_path)) {
				throw InvalidFile("Invalid protobuf data path", "");
			}
		}
	}

	// Selected index, thresholds, UI flags, filters, etc.
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
		QStringList parts = QString::fromStdString(class_filter).split(',');
		display_classes.clear();
		for (auto &p : parts) {
			auto s = p.trimmed().toLower();
			if (!s.isEmpty()) {
				display_classes.push_back(s.toStdString());
			}
		}
	}

	// Apply any per-object overrides
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
