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
using namespace std;
using namespace openshot;


/// Blank constructor, useful when using Json to load the effect properties
ObjectDetection::ObjectDetection(std::string clipObDetectDataPath)
{
    // Init effect properties
    init_effect_details();

    // Tries to load the tracker data from protobuf
    LoadObjDetectdData(clipObDetectDataPath);

    // Initialize the selected object index as the first object index
    selectedObjectIndex = trackedObjects.begin()->first;
}

// Default constructor
ObjectDetection::ObjectDetection()
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
std::shared_ptr<Frame> ObjectDetection::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
    // Get the frame's image
    cv::Mat cv_image = frame->GetImageCV();

    // Check if frame isn't NULL
    if(cv_image.empty()){
        return frame;
    }

    // Initialize the Qt rectangle that will hold the positions of the bounding-box
	std::vector<QRectF> boxRects;
	// Initialize the image of the TrackedObject child clip
	std::vector<std::shared_ptr<QImage>> childClipImages;

    // Check if track data exists for the requested frame
    if (detectionsData.find(frame_number) != detectionsData.end()) {
        float fw = cv_image.size().width;
        float fh = cv_image.size().height;

        DetectionData detections = detectionsData[frame_number];
        for(int i = 0; i<detections.boxes.size(); i++){

            // Does not show boxes with confidence below the threshold
            if(detections.confidences.at(i) < confidence_threshold){
                continue;
            }
            // Just display selected classes
            if( display_classes.size() > 0 &&
                std::find(display_classes.begin(), display_classes.end(), classNames[detections.classIds.at(i)]) == display_classes.end()){
                continue;
            }

            // Get the object id
            int objectId = detections.objectIds.at(i);

            // Search for the object in the trackedObjects map
            auto trackedObject_it = trackedObjects.find(objectId);

            // Cast the object as TrackedObjectBBox
            std::shared_ptr<TrackedObjectBBox> trackedObject = std::static_pointer_cast<TrackedObjectBBox>(trackedObject_it->second);

            // Check if the tracked object has data for this frame
            if (trackedObject->Contains(frame_number) &&
                trackedObject->visible.GetValue(frame_number) == 1)
            {
                // Get the bounding-box of given frame
                BBox trackedBox = trackedObject->GetBox(frame_number);
                bool draw_text = !display_box_text.GetValue(frame_number);
                std::vector<int> stroke_rgba = trackedObject->stroke.GetColorRGBA(frame_number);
                int stroke_width = trackedObject->stroke_width.GetValue(frame_number);
                float stroke_alpha = trackedObject->stroke_alpha.GetValue(frame_number);
                std::vector<int> bg_rgba = trackedObject->background.GetColorRGBA(frame_number);
                float bg_alpha = trackedObject->background_alpha.GetValue(frame_number);

                // Create a rotated rectangle object that holds the bounding box
                // cv::RotatedRect box ( cv::Point2f( (int)(trackedBox.cx*fw), (int)(trackedBox.cy*fh) ),
                //					 cv::Size2f( (int)(trackedBox.width*fw), (int)(trackedBox.height*fh) ),
                //					 (int) (trackedBox.angle) );

                // DrawRectangleRGBA(cv_image, box, bg_rgba, bg_alpha, 1, true);
                // DrawRectangleRGBA(cv_image, box, stroke_rgba, stroke_alpha, stroke_width, false);


                cv::Rect2d box(
                    (int)( (trackedBox.cx-trackedBox.width/2)*fw),
                    (int)( (trackedBox.cy-trackedBox.height/2)*fh),
                    (int)(  trackedBox.width*fw),
                    (int)(  trackedBox.height*fh)
                    );

                // If the Draw Box property is off, then make the box invisible
                if (trackedObject->draw_box.GetValue(frame_number) == 0)
                {
                    bg_alpha = 1.0;
                    stroke_alpha = 1.0;
                }

                drawPred(detections.classIds.at(i), detections.confidences.at(i),
                    box, cv_image, detections.objectIds.at(i), bg_rgba, bg_alpha, 1, true, draw_text);
                drawPred(detections.classIds.at(i), detections.confidences.at(i),
                    box, cv_image, detections.objectIds.at(i), stroke_rgba, stroke_alpha, stroke_width, false, draw_text);


                // Get the Detected Object's child clip
                if (trackedObject->ChildClipId() != ""){
                    // Cast the parent timeline of this effect
                    Timeline* parentTimeline = (Timeline *) ParentTimeline();
                    if (parentTimeline){
                        // Get the Tracked Object's child clip
                        Clip* childClip = parentTimeline->GetClip(trackedObject->ChildClipId());

                        if (childClip){
                            std::shared_ptr<Frame> f(new Frame(1, frame->GetWidth(), frame->GetHeight(), "#00000000"));
                            // Get the image of the child clip for this frame
					        std::shared_ptr<Frame> childClipFrame = childClip->GetFrame(f, frame_number);
                            childClipImages.push_back(childClipFrame->GetImage());

                            // Set the Qt rectangle with the bounding-box properties
                            QRectF boxRect;
                            boxRect.setRect((int)((trackedBox.cx-trackedBox.width/2)*fw),
                                            (int)((trackedBox.cy - trackedBox.height/2)*fh),
                                            (int)(trackedBox.width*fw),
                                            (int)(trackedBox.height*fh));
                            boxRects.push_back(boxRect);
                        }
                    }
                }
            }
        }
    }

    // Update Qt image with new Opencv frame
    frame->SetImageCV(cv_image);

	// Set the bounding-box image with the Tracked Object's child clip image
	if(boxRects.size() > 0){
        // Get the frame image
        QImage frameImage = *(frame->GetImage());
        for(int i; i < boxRects.size();i++){
            // Set a Qt painter to the frame image
            QPainter painter(&frameImage);
            // Draw the child clip image inside the bounding-box
            painter.drawImage(boxRects[i], *childClipImages[i], QRectF(0, 0, frameImage.size().width(),  frameImage.size().height()));
        }
        // Set the frame image as the composed image
        frame->AddImage(std::make_shared<QImage>(frameImage));
    }

    return frame;
}

void ObjectDetection::DrawRectangleRGBA(cv::Mat &frame_image, cv::RotatedRect box, std::vector<int> color, float alpha,
                                        int thickness, bool is_background){
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

void ObjectDetection::drawPred(int classId, float conf, cv::Rect2d box, cv::Mat& frame, int objectNumber, std::vector<int> color,
                                float alpha, int thickness, bool is_background, bool display_text)
{

    if(is_background){
        cv::Mat overlayFrame;
        frame.copyTo(overlayFrame);

        //Draw a rectangle displaying the bounding box
        cv::rectangle(overlayFrame, box, cv::Scalar(color[2],color[1],color[0]), cv::FILLED);

       // add opacity
        cv::addWeighted(overlayFrame, 1-alpha, frame, alpha, 0, frame);
    }
    else{
        cv::Mat overlayFrame;
        frame.copyTo(overlayFrame);

        //Draw a rectangle displaying the bounding box
        cv::rectangle(overlayFrame, box, cv::Scalar(color[2],color[1],color[0]), thickness);

        if(display_text){
            //Get the label for the class name and its confidence
            std::string label = cv::format("%.2f", conf);
            if (!classNames.empty())
            {
                CV_Assert(classId < (int)classNames.size());
                label = classNames[classId] + ":" + label;
            }

            //Display the label at the top of the bounding box
            int baseLine;
            cv::Size labelSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

            double left = box.x;
            double top = std::max((int)box.y, labelSize.height);

            cv::rectangle(overlayFrame, cv::Point(left, top - round(1.025*labelSize.height)), cv::Point(left + round(1.025*labelSize.width), top + baseLine),
                            cv::Scalar(color[2],color[1],color[0]), cv::FILLED);
            putText(overlayFrame, label, cv::Point(left+1, top), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0,0,0),1);
        }
        // add opacity
        cv::addWeighted(overlayFrame, 1-alpha, frame, alpha, 0, frame);
    }
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

        // Just display selected classes
        if( display_classes.size() > 0 &&
            std::find(display_classes.begin(), display_classes.end(), classNames[detections.classIds.at(i)]) == display_classes.end()){
            continue;
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

    if (!root["class_filter"].isNull()){
        class_filter = root["class_filter"].asString();
        std::stringstream ss(class_filter);
        display_classes.clear();
        while( ss.good() )
        {
            // Parse comma separated string
            std::string substr;
            std::getline( ss, substr, ',' );
            display_classes.push_back( substr );
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
    Json::Value root;

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
    root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
    root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
    root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
    root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
    root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
    root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);
    root["confidence_threshold"] = add_property_json("Confidence Theshold", confidence_threshold, "float", "", NULL, 0, 1, false, requested_frame);
    root["class_filter"] = add_property_json("Class Filter", 0.0, "string", class_filter, NULL, -1, -1, false, requested_frame);

    root["display_box_text"] = add_property_json("Draw Box Text", display_box_text.GetValue(requested_frame), "int", "", &display_box_text, 0, 1.0, false, requested_frame);
    root["display_box_text"]["choices"].append(add_property_choice_json("Off", 1, display_box_text.GetValue(requested_frame)));
    root["display_box_text"]["choices"].append(add_property_choice_json("On", 0, display_box_text.GetValue(requested_frame)));

    // Return formatted string
    return root.toStyledString();
}
