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
#include <QBrush>
#include <QColor>
#include <QRectF>
#include <QString>
#include <QStringList>
using namespace std;
using namespace openshot;

namespace {
bool is_all_objects_key(const std::string& name)
{
    const QString normalized = QString::fromStdString(name).trimmed().toLower();
    return normalized == "all" || normalized == "*" || normalized == "-1";
}

std::shared_ptr<TrackedObjectBBox> make_all_objects_properties(
	const std::shared_ptr<TrackedObjectBase>& source,
	bool has_mask_data = false)
{
	auto properties = std::make_shared<TrackedObjectBBox>();
	if (source) {
		properties->SetJsonValue(source->JsonValue());
		auto source_bbox = std::dynamic_pointer_cast<TrackedObjectBBox>(source);
		has_mask_data = has_mask_data || (source_bbox && source_bbox->HasMaskData());
	}
	if (has_mask_data) {
		properties->AddMask(0, ObjectMaskData{1, 1, {0, 1}});
	}
	properties->Id("All Objects");
	return properties;
}

bool has_tracked_object_mask_data(
	const std::map<int, std::shared_ptr<TrackedObjectBase>>& tracked_objects)
{
	for (const auto& tracked_object : tracked_objects) {
		auto bbox = std::dynamic_pointer_cast<TrackedObjectBBox>(tracked_object.second);
		if (bbox && bbox->HasMaskData())
			return true;
	}
	return false;
}

cv::Scalar default_class_color(const std::string& class_name, int index)
{
    const QString normalized = QString::fromStdString(class_name).trimmed().toLower();

    // RGB values. Keep common object-detection classes on clear, saturated colors
    // instead of the previous deterministic random palette.
    if (normalized == "person") return cv::Scalar(83, 160, 237);
    if (normalized == "car") return cv::Scalar(42, 200, 185);
    if (normalized == "truck") return cv::Scalar(239, 126, 92);
    if (normalized == "bus") return cv::Scalar(250, 196, 72);
    if (normalized == "bicycle") return cv::Scalar(122, 201, 67);
    if (normalized == "motorbike" || normalized == "motorcycle") return cv::Scalar(180, 126, 235);
    if (normalized == "dog") return cv::Scalar(237, 92, 140);
    if (normalized == "cat") return cv::Scalar(101, 214, 128);

    static const cv::Scalar palette[] = {
        cv::Scalar(83, 160, 237),
        cv::Scalar(42, 200, 185),
        cv::Scalar(239, 126, 92),
        cv::Scalar(250, 196, 72),
        cv::Scalar(122, 201, 67),
        cv::Scalar(180, 126, 235),
        cv::Scalar(237, 92, 140),
        cv::Scalar(72, 190, 230),
    };
    return palette[index % (sizeof(palette) / sizeof(palette[0]))];
}

QImage alpha_mask_image_from_rle(const ObjectMaskData& mask)
{
    QImage image(mask.width, mask.height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!mask.HasData())
        return image;

    QRgb* data = reinterpret_cast<QRgb*>(image.bits());
    const int total = mask.width * mask.height;
    int offset = 0;
    bool value = false;
    for (uint32_t count : mask.rle) {
        const int end = std::min(total, offset + static_cast<int>(count));
        if (value) {
            std::fill(data + offset, data + end, qRgba(255, 255, 255, 255));
        }
        offset = end;
        value = !value;
        if (offset >= total)
            break;
    }
    return image;
}
}


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
                    float stroke_alpha = trackedObject->stroke_alpha.GetValue(frame_number);
                    float bg_alpha = trackedObject->background_alpha.GetValue(frame_number);
                    float bg_corner = trackedObject->background_corner.GetValue(frame_number);

                    // Set the pen for the border
                    QPen pen(QColor(stroke_rgba[0], stroke_rgba[1], stroke_rgba[2], 255 * stroke_alpha));
                    pen.setWidthF(trackedObject->ScaledStrokeWidth(
                        frame_number, frame_image->width(), frame_image->height()));
                    painter.setPen(pen);

                    // Set the brush for the background
                    QBrush brush(QColor(bg_rgba[0], bg_rgba[1], bg_rgba[2], 255 * bg_alpha));
                    painter.setBrush(brush);

                    if (display_boxes.GetValue(frame_number) == 1 && trackedObject->draw_box.GetValue(frame_number) == 1) {
                        // Only draw boxes if both properties are set to YES (draw all boxes, and draw box of the selected box)
                        painter.drawRoundedRect(boxRect, bg_corner, bg_corner);
                    }

                    ObjectMaskData object_mask = trackedObject->GetMask(frame_number, 5);
                    if (object_mask.HasData() && trackedObject->draw_mask.GetValue(frame_number) == 1) {
                        QImage mask = alpha_mask_image_from_rle(object_mask)
                            .scaled(frame_image->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                        std::vector<int> mask_rgba = trackedObject->mask_color.GetColorRGBA(frame_number);
                        float mask_alpha = trackedObject->mask_alpha.GetValue(frame_number);
                        QColor mask_color(mask_rgba[0], mask_rgba[1], mask_rgba[2], 255 * mask_alpha);
                        QImage overlay(frame_image->size(), QImage::Format_ARGB32_Premultiplied);
                        overlay.fill(Qt::transparent);
                        QPainter overlay_painter(&overlay);
                        overlay_painter.setCompositionMode(QPainter::CompositionMode_Source);
                        overlay_painter.fillRect(overlay.rect(), mask_color);
                        overlay_painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                        overlay_painter.drawImage(0, 0, mask);
                        overlay_painter.end();
                        painter.drawImage(0, 0, overlay);
                    }

                    if(display_box_text.GetValue(frame_number) == 1 && trackedObject->draw_text.GetValue(frame_number) == 1) {
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
    classesColor.clear();
    detectionsData.clear();
    trackedObjects.clear();

    // Seed colors for each class
    for (int i = 0; i < objMessage.classnames_size(); ++i) {
        const std::string class_name = objMessage.classnames(i);
        classNames.push_back(class_name);
        classesColor.push_back(default_class_color(class_name, i));
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
        std::vector<ObjectMaskData> masks;

        // For each bounding box in this frame
        for (int di = 0; di < pbFrame.bounding_box_size(); ++di) {
            const auto &b = pbFrame.bounding_box(di);
            float x = b.x(), y = b.y(), w = b.w(), h = b.h();
            int   classId   = b.classid();
            float confidence= b.confidence();
            int   objectId  = b.objectid();
            ObjectMaskData mask;
            if (b.has_mask()) {
                mask.width = b.mask().width();
                mask.height = b.mask().height();
                for (int rleIndex = 0; rleIndex < b.mask().rle_size(); ++rleIndex) {
                    mask.rle.push_back(b.mask().rle(rleIndex));
                }
            }

            // Record for DetectionData
            classIds.push_back(classId);
            confidences.push_back(confidence);
            boxes.emplace_back(x, y, w, h);
            objectIds.push_back(objectId);
            masks.push_back(mask);

            // Either append to an existing TrackedObjectBBox…
            auto it = trackedObjects.find(objectId);
            if (it != trackedObjects.end()) {
                it->second->AddBox(frameId, x + w/2, y + h/2, w, h, 0.0);
                auto bbox = std::dynamic_pointer_cast<TrackedObjectBBox>(it->second);
                if (bbox && mask.HasData())
                    bbox->AddMask(frameId, mask);
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
                tmpObj.background_alpha = Keyframe(0.15);
                tmpObj.AddBox(frameId, x + w/2, y + h/2, w, h, 0.0);
                if (mask.HasData())
                    tmpObj.AddMask(frameId, mask);

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
            classIds, confidences, boxes, frameId, objectIds, masks
        );
    }

    google::protobuf::ShutdownProtobufLibrary();

    // Default to the pseudo-selection that edits every tracked object.
    if (!trackedObjects.empty()) {
        selectedObjectIndex = -1;
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

	if (!trackedObjects.empty()) {
		root["visible_objects_index"].append(-1);
		root["visible_objects_id"].append("All Objects");
		root["visible_class_names"].append("All Objects");
	}

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

std::shared_ptr<QImage> ObjectDetection::TrackedObjectMask(std::shared_ptr<QImage> target_image, int64_t frame_number) const {
	if (!target_image || target_image->isNull())
		return {};

	auto detections_it = detectionsData.find(frame_number);
	if (detections_it == detectionsData.end())
		return {};

	auto mask_image = std::make_shared<QImage>(
		target_image->width(), target_image->height(), QImage::Format_RGBA8888_Premultiplied);
	mask_image->fill(QColor(0, 0, 0, 255));

	QPainter painter(mask_image.get());
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QBrush(QColor(255, 255, 255, 255)));

	bool drew_any_box = false;
	const DetectionData& detections = detections_it->second;
	for (int i = 0; i < detections.boxes.size(); i++) {
		if (detections.confidences.at(i) < confidence_threshold)
			continue;

		const int class_id = detections.classIds.at(i);
		if (class_id < 0 || class_id >= classNames.size())
			continue;

		const std::string class_name = classNames[class_id];
		if (!display_classes.empty() &&
			std::find(display_classes.begin(), display_classes.end(), class_name) == display_classes.end()) {
			continue;
		}

		int object_id = detections.objectIds.at(i);
		auto tracked_object_it = trackedObjects.find(object_id);
		if (tracked_object_it == trackedObjects.end() || !tracked_object_it->second)
			continue;

		auto tracked_object = std::static_pointer_cast<TrackedObjectBBox>(tracked_object_it->second);
		if (!tracked_object->ExactlyContains(frame_number) ||
			tracked_object->visible.GetValue(frame_number) != 1) {
			continue;
		}

		BBox box = tracked_object->GetBox(frame_number);
		if (box.width <= 0.0f || box.height <= 0.0f || box.cx < 0.0f || box.cy < 0.0f)
			continue;

		ObjectMaskData object_mask = tracked_object->GetMask(frame_number, 5);
		if (object_mask.HasData() && tracked_object->draw_mask.GetValue(frame_number) == 1) {
			QImage mask = alpha_mask_image_from_rle(object_mask)
				.scaled(target_image->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			painter.drawImage(0, 0, mask);
			drew_any_box = true;
			continue;
		}

		const double x = (box.cx - box.width / 2.0) * target_image->width();
		const double y = (box.cy - box.height / 2.0) * target_image->height();
		const double w = box.width * target_image->width();
		const double h = box.height * target_image->height();
		const double corner = tracked_object->background_corner.GetValue(frame_number);
		painter.drawRoundedRect(QRectF(x, y, w, h), corner, corner);
		drew_any_box = true;
	}

	painter.end();
	if (!drew_any_box)
		return {};
	return mask_image;
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
			allObjectsProperties.reset();
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
			if (is_all_objects_key(name)) {
				if (!allObjectsProperties) {
					std::shared_ptr<TrackedObjectBase> firstObject;
					if (!trackedObjects.empty())
						firstObject = trackedObjects.begin()->second;
					allObjectsProperties = make_all_objects_properties(
						firstObject, has_tracked_object_mask_data(trackedObjects));
				}
				allObjectsProperties->SetJsonValue(root["objects"][name]);
				for (auto& trackedObject : trackedObjects) {
					if (trackedObject.second)
						trackedObject.second->SetJsonValue(root["objects"][name]);
				}
			}
		}

		for (const auto& name : memberNames)
		{
			if (is_all_objects_key(name))
				continue;
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
	if(selectedObjectIndex == -1 && !trackedObjects.empty()){
		auto selectedObject = allObjectsProperties
			? allObjectsProperties
			: make_all_objects_properties(trackedObjects.begin()->second, has_tracked_object_mask_data(trackedObjects));
		if (selectedObject){
			Json::Value trackedObjectJSON = selectedObject->PropertiesJSON(requested_frame);
			trackedObjectJSON["box_id"]["memo"] = "All Objects";
			trackedObjectJSON.removeMember("x1");
			trackedObjectJSON.removeMember("y1");
			trackedObjectJSON.removeMember("x2");
			trackedObjectJSON.removeMember("y2");
			objects["all"] = trackedObjectJSON;
		}
	}
	else if(trackedObjects.count(selectedObjectIndex) != 0){
		auto selectedObject = trackedObjects.at(selectedObjectIndex);
		if (selectedObject){
			Json::Value trackedObjectJSON = selectedObject->PropertiesJSON(requested_frame);
			// add object json
			objects[selectedObject->Id()] = trackedObjectJSON;
		}
	}
	root["objects"] = objects;

	root["selected_object_index"] = add_property_json("Selected Object", selectedObjectIndex, "int", "", NULL, -1, 200, false, requested_frame);
	root["confidence_threshold"] = add_property_json("Confidence Threshold", confidence_threshold, "float", "", NULL, 0, 1, false, requested_frame);
	root["class_filter"] = add_property_json("Class Filter", 0.0, "string", class_filter, NULL, -1, -1, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
