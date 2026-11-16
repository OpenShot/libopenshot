/**
 * @file
 * @brief Source file for Stabilizer effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <fstream>
#include <iomanip>
#include <iostream>

#include "effects/Stabilizer.h"
#include "Exceptions.h"
#include "stabilizedata.pb.h"

#include <google/protobuf/util/time_util.h>

#if CV_VERSION_MAJOR >= 5
#define int64 opencv_broken_int
#define uint64 opencv_broken_uint
#include <opencv2/geometry/2d.hpp>
#undef uint64
#undef int64
#else
#include <opencv2/imgproc.hpp>
#endif

#ifdef OPENSHOT_ENABLE_GPU_FRAME
#include "VulkanCompositor.h"
#include "VulkanFrameImage.h"
#include "Logger.h"
#endif

using namespace std;
using namespace openshot;
using google::protobuf::util::TimeUtil;

/// Blank constructor, useful when using Json to load the effect properties
Stabilizer::Stabilizer(std::string clipStabilizedDataPath):protobuf_data_path(clipStabilizedDataPath)
{
	// Init effect properties
	init_effect_details();
	// Tries to load the stabilization data from protobuf
	LoadStabilizedData(clipStabilizedDataPath);
}

// Default constructor
Stabilizer::Stabilizer()
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
	protobuf_data_path = "";
	zoom = 1.0;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Stabilizer::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{

#ifdef OPENSHOT_ENABLE_GPU_FRAME
    if(VulkanRenderingEnabled()) {
        try {
            auto surface=frame->GetDeferredImage();
            if(!surface && ParentClip() && frame->has_image_data) surface=UploadVulkanImage(*frame->GetImage());
            if(surface) {
                const auto transform=transformationData.find(frame_number);
                if(transform==transformationData.end()) {
                    frame->AddDeferredImage(StabilizeVulkanImage(surface,{}));return frame;
                }
                const auto& t=transform->second;
                const int w=surface->GetWidth(),h=surface->GetHeight();
                const cv::Mat rotation=(cv::Mat_<double>(2,3)<<cos(t.da),-sin(t.da),t.dx*w,sin(t.da),cos(t.da),t.dy*h);
                const float scale=zoom.GetValue(frame_number);
                const auto zoom_matrix=cv::getRotationMatrix2D(cv::Point2f(w/2,h/2),0,scale);
                // OpenCV quantizes each axis independently before the 1/32-pixel interpolation.
                std::vector<std::vector<int32_t>> axes;
                for(const auto& forward:{rotation,zoom_matrix}) {
                    cv::Mat inverse;cv::invertAffineTransform(forward,inverse);
                    const double* m=inverse.ptr<double>();std::vector<int32_t> table(2*(w+h));
                    for(int x=0;x<w;++x) { table[2*x]=cv::saturate_cast<int>(m[0]*x*1024);table[2*x+1]=cv::saturate_cast<int>(m[3]*x*1024); }
                    for(int y=0;y<h;++y) { table[2*(w+y)]=cv::saturate_cast<int>((m[1]*y+m[2])*1024)+16;table[2*(w+y)+1]=cv::saturate_cast<int>((m[4]*y+m[5])*1024)+16; }
                    axes.push_back(std::move(table));
                }
                frame->AddDeferredImage(StabilizeVulkanImage(surface,axes));return frame;
            }
        } catch(const std::exception& e) { Logger::Instance()->Log(std::string("Vulkan Stabilizer CPU fallback: ")+e.what()); }
    }
#endif

	// Grab OpenCV Mat image
	cv::Mat frame_image = frame->GetImageCV();

	// If frame is NULL, return itself
	if(!frame_image.empty()){

		// Check if track data exists for the requested frame
		if(transformationData.find(frame_number) != transformationData.end()){

			float zoom_value = zoom.GetValue(frame_number);

			// Create empty rotation matrix
			cv::Mat T(2,3,CV_64F);

			// Set rotation matrix values
			T.at<double>(0,0) = cos(transformationData[frame_number].da);
			T.at<double>(0,1) = -sin(transformationData[frame_number].da);
			T.at<double>(1,0) = sin(transformationData[frame_number].da);
			T.at<double>(1,1) = cos(transformationData[frame_number].da);

			T.at<double>(0,2) = transformationData[frame_number].dx * frame_image.size().width;
			T.at<double>(1,2) = transformationData[frame_number].dy * frame_image.size().height;

			// Apply rotation matrix to image
			cv::Mat frame_stabilized;
			cv::warpAffine(frame_image, frame_stabilized, T, frame_image.size());

			// Scale up the image to remove black borders
			cv::Mat T_scale = cv::getRotationMatrix2D(cv::Point2f(frame_stabilized.cols/2, frame_stabilized.rows/2), 0, zoom_value);
			cv::warpAffine(frame_stabilized, frame_stabilized, T_scale, frame_stabilized.size());
			frame_image = frame_stabilized;
		}
	}
	// Set stabilized image to frame
	// If the input image is NULL or doesn't have tracking data, it's returned as it came
	frame->SetImageCV(frame_image);
	return frame;
}

// Load protobuf data file
bool Stabilizer::LoadStabilizedData(std::string inputFilePath){
	using std::ios;
	// Create stabilization message
	pb_stabilize::Stabilization stabilizationMessage;

	// Read the existing tracker message.
	std::fstream input(inputFilePath, ios::in | ios::binary);
	if (!stabilizationMessage.ParseFromIstream(&input)) {
		std::cerr << "Failed to parse protobuf message." << std::endl;
		return false;
	}

	// Make sure the data maps are empty
	transformationData.clear();
	trajectoryData.clear();

	// Iterate over all frames of the saved message and assign to the data maps
	for (size_t i = 0; i < stabilizationMessage.frame_size(); i++) {

		// Create stabilization message
		const pb_stabilize::Frame& pbFrameData = stabilizationMessage.frame(i);

		// Load frame number
		size_t id = pbFrameData.id();

		// Load camera trajectory data
		float x = pbFrameData.x();
		float y = pbFrameData.y();
		float a = pbFrameData.a();

		// Assign data to trajectory map
		trajectoryData[i] = EffectCamTrajectory(x,y,a);

		// Load transformation data
		float dx = pbFrameData.dx();
		float dy = pbFrameData.dy();
		float da = pbFrameData.da();

		// Assing data to transformation map
		transformationData[id] = EffectTransformParam(dx,dy,da);
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
	root["protobuf_data_path"] = protobuf_data_path;
	root["zoom"] = zoom.JsonValue();

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
	if (!root["protobuf_data_path"].isNull()){
		protobuf_data_path = (root["protobuf_data_path"].asString());

		if(!LoadStabilizedData(protobuf_data_path)){
			std::cout<<"Invalid protobuf data path";
			protobuf_data_path = "";
		}
	}
	if(!root["zoom"].isNull())
		zoom.SetJsonValue(root["zoom"]);
}

// Get all properties for a specific frame
std::string Stabilizer::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	root["zoom"] = add_property_json("Zoom", zoom.GetValue(requested_frame), "float", "", &zoom, 0.0, 2.0, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
