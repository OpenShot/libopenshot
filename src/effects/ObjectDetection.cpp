/**
 * @file
 * @brief Source file for Object Detection effect class
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

#include <string>

#include "effects/ObjectDetection.h"
#include "effects/Tracker.h"

using namespace std;
using namespace openshot;


/// Blank constructor, useful when using Json to load the effect properties
ObjectDetection::ObjectDetection(std::string clipObDetectDataPath)
{
    // Init effect properties
	init_effect_details();

    // Tries to load the tracker data from protobuf
    LoadObjDetectdData(clipObDetectDataPath);
}

// Default constructor
ObjectDetection::ObjectDetection()
{
	// Init effect properties
	init_effect_details();

}

// Init effect settings
void ObjectDetection::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Object Detector";
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

    // Check if track data exists for the requested frame
    if (detectionsData.find(frame_number) != detectionsData.end()) {
        float fw = cv_image.size().width;
        float fh = cv_image.size().height;

        DetectionData detections = detectionsData[frame_number];
        for(int i = 0; i<detections.boxes.size(); i++){
            
            // Get the object id
            int objectId = detections.objectIds.at(i);

            // Search for the object in the trackedObjects map
            auto trackedObject_it = trackedObjects.find(objectId);

            // Cast the object as TrackedObjectBBox
            std::shared_ptr<TrackedObjectBBox> trackedObject = std::static_pointer_cast<TrackedObjectBBox>(trackedObject_it->second);

            // Check if the tracked object has data for this frame
            if (trackedObject->Contains(frame_number)){
                
                // Get the bounding-box of given frame
                BBox trackedBox = trackedObject->GetBox(frame_number);
                cv::Rect2d box(
                    (int)( (trackedBox.cx-trackedBox.width/2)*fw),
                    (int)( (trackedBox.cy-trackedBox.height/2)*fh),
                    (int)(  trackedBox.width*fw),
                    (int)(  trackedBox.height*fh)
                    );
                drawPred(detections.classIds.at(i), detections.confidences.at(i),
                    box, cv_image, detections.objectIds.at(i));
            } 
        }
    }

	// Set image with drawn box to frame
    // If the input image is NULL or doesn't have tracking data, it's returned as it came
	frame->SetImageCV(cv_image);

	return frame;
}

void ObjectDetection::drawPred(int classId, float conf, cv::Rect2d box, cv::Mat& frame, int objectNumber)
{

    //Draw a rectangle displaying the bounding box
    cv::rectangle(frame, box, classesColor[classId], 2);

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

    cv::rectangle(frame, cv::Point(left, top - round(1.025*labelSize.height)), cv::Point(left + round(1.025*labelSize.width), top + baseLine), classesColor[classId], cv::FILLED);
    putText(frame, label, cv::Point(left+1, top), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0,0,0),1);
}

// Load protobuf data file
bool ObjectDetection::LoadObjDetectdData(std::string inputFilePath){
    // Create tracker message
    pb_objdetect::ObjDetect objMessage;

    
    // Read the existing tracker message.
    fstream input(inputFilePath, ios::in | ios::binary);
    if (!objMessage.ParseFromIstream(&input)) {
        cerr << "Failed to parse protobuf message." << endl;
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
                TrackedObjectBBox trackedObj;
                trackedObj.AddBox(id, x+(w/2), y+(h/2), w, h, 0.0);
	            std::shared_ptr<TrackedObjectBBox> trackedObjPtr = std::make_shared<TrackedObjectBBox>(trackedObj);
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

// Get tracker info for the desired frame
DetectionData ObjectDetection::GetTrackedData(size_t frameId){

	// Check if the tracker info for the requested frame exists
    if ( detectionsData.find(frameId) == detectionsData.end() ) {
        return DetectionData();
    } else {
        return detectionsData[frameId];
    }

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
    
    // Add trackedObjects IDs to JSON
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
	if (!root["protobuf_data_path"].isNull()){
		protobuf_data_path = (root["protobuf_data_path"].asString());

		if(!LoadObjDetectdData(protobuf_data_path)){
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
}

// Get all properties for a specific frame
std::string ObjectDetection::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;

    // Add trackedObjects IDs to JSON
	for (auto const& trackedObject : trackedObjects){
		// Save the trackedObject Id on root
        Json::Value trackedObjectJSON = trackedObject.second->PropertiesJSON(requested_frame);
        root["box_id-"+to_string(trackedObject.first)] = trackedObjectJSON["box_id"];
        root["visible-"+to_string(trackedObject.first)] = trackedObjectJSON["visible"];
        
        // Add trackedObject's properties only if it's visible in this frame (performance boost)
        if (trackedObjectJSON["visible"]["value"].asBool()){
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
	}

	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
