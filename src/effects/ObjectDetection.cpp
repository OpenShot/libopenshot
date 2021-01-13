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
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> ObjectDetection::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
    // Get the frame's image
	cv::Mat cv_image = frame->GetImageCV();
    std::cout<<"Frame number: "<<frame_number<<"\n\n";
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
            cv::Rect_<float> bb_nrml = detections.boxes.at(i);
            cv::Rect2d box((int)(bb_nrml.x*fw),
                           (int)(bb_nrml.y*fh),
                           (int)(bb_nrml.width*fw),
                           (int)(bb_nrml.height*fh));
            drawPred(detections.classIds.at(i), detections.confidences.at(i),
                     box, cv_image);
        }
    }

	// Set image with drawn box to frame
    // If the input image is NULL or doesn't have tracking data, it's returned as it came
	frame->SetImageCV(cv_image);

	return frame;
}

void ObjectDetection::drawPred(int classId, float conf, cv::Rect2d box, cv::Mat& frame)
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
    std::cout<<"X1: "<<box.x<<" Y1: "<<box.y<<" X2: "<<box.width + box.x<<" Y2: "<<box.height + box.y<<"\n";
    std::cout<<"Class: "<<label<<"\n\n";
}

// Load protobuf data file
bool ObjectDetection::LoadObjDetectdData(std::string inputFilePath){
    // Create tracker message
    libopenshotobjdetect::ObjDetect objMessage;

    {
        // Read the existing tracker message.
        fstream input(inputFilePath, ios::in | ios::binary);
        if (!objMessage.ParseFromIstream(&input)) {
            cerr << "Failed to parse protobuf message." << endl;
            return false;
        }
    }

    // Make sure classNames and detectionsData are empty
    classNames.clear();
    detectionsData.clear();

    // Seed to generate same random numbers
    std::srand(1);
    // Get all classes names and assign a color to them
    for(int i = 0; i < objMessage.classnames_size(); i++){
        classNames.push_back(objMessage.classnames(i));
        classesColor.push_back(cv::Scalar(std::rand()%205 + 50, std::rand()%205 + 50, std::rand()%205 + 50));
    }

    // Iterate over all frames of the saved message
    for (size_t i = 0; i < objMessage.frame_size(); i++) {
        // Create protobuf message reader
        const libopenshotobjdetect::Frame& pbFrameData = objMessage.frame(i);

        // Get frame Id
        size_t id = pbFrameData.id();

        // Load bounding box data
        const google::protobuf::RepeatedPtrField<libopenshotobjdetect::Frame_Box > &pBox = pbFrameData.bounding_box();

        // Construct data vectors related to detections in the current frame
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect_<float>> boxes;

        for(int i = 0; i < pbFrameData.bounding_box_size(); i++){
            // Get bounding box coordinates
            float x = pBox.Get(i).x();
            float y = pBox.Get(i).y();
            float w = pBox.Get(i).w();
            float h = pBox.Get(i).h();
            // Get class Id (which will be assign to a class name)
            int classId = pBox.Get(i).classid();
            // Get prediction confidence
            float confidence = pBox.Get(i).confidence();

            // Create OpenCV rectangle with the bouding box info
            cv::Rect_<float> box(x, y, w, h);

            // Push back data into vectors
            boxes.push_back(box);
            classIds.push_back(classId);
            confidences.push_back(confidence);
        }

        // Assign data to object detector map
        detectionsData[id] = DetectionData(classIds, confidences, boxes, id);
    }

    // Show the time stamp from the last update in object detector data file
    if (objMessage.has_last_updated())
        cout << "  Loaded Data. Saved Time Stamp: " << TimeUtil::ToString(objMessage.last_updated()) << endl;

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
}

// Get all properties for a specific frame
std::string ObjectDetection::PropertiesJSON(int64_t requested_frame) const {

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
