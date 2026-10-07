/**
 * @file
 * @brief Header file for CVObjectDetection class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#define int64 opencv_broken_int
#define uint64 opencv_broken_uint
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#undef uint64
#undef int64
#include "Json.h"
#include "ProcessingController.h"
#include "Clip.h"

#include "sort_filter/sort.hpp"

// Forward decl
namespace pb_objdetect {
    class Frame;
}

namespace openshot
{
    struct CVObjectMaskData{
        int width = 0;
        int height = 0;
        std::vector<uint32_t> rle;

        bool HasData() const { return width > 0 && height > 0 && !rle.empty(); }
    };

    struct CVTrackedMaskData{
        size_t frameId = 0;
        CVObjectMaskData mask;
        cv::Rect_<float> box;
    };

    // Stores the detected object bounding boxes and its properties.
    struct CVDetectionData{
        CVDetectionData(){}
        CVDetectionData(
            std::vector<int> _classIds,
            std::vector<float> _confidences,
            std::vector<cv::Rect_<float>> _boxes,
            size_t _frameId,
            std::vector<int> _objectIds,
            std::vector<CVObjectMaskData> _masks = {})
        {
            classIds = _classIds;
            confidences = _confidences;
            boxes = _boxes;
            frameId = _frameId;
            objectIds = _objectIds;
            masks = _masks;
        }
        size_t frameId;
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect_<float>> boxes;
        std::vector<int> objectIds;
        std::vector<CVObjectMaskData> masks;
    };

    /**
     * @brief This class runs trought a clip to detect objects and returns the bounding boxes and its properties.
     *
     * Object detection is performed using a supported YOLO ONNX model with OpenCV DNN module.
     */
    class CVObjectDetection{

        private:

        cv::dnn::Net net;
        std::vector<std::string> classNames;
        float confThreshold, nmsThreshold;

        std::string classesFile;
        std::string modelPath;
        std::string processingDevice;
        std::string protobuf_data_path;
        int inpWidth;
        int inpHeight;
        bool generateMasks;

        SortTracker sort;
        std::map<int, CVTrackedMaskData> recentObjectMasks;

        uint progress;

        size_t start;
        size_t end;

        bool error = false;

        /// Will handle a Thread safely comutication between ClipProcessingJobs and the processing effect classes
        ProcessingController *processingController;

        void setProcessingDevice();

        // Detect onbects on a single frame
        void DetectObjects(const cv::Mat &frame, size_t frame_number);

        bool iou(cv::Rect pred_box, cv::Rect sort_box);

        // Remove the bounding boxes with low confidence using non-maxima suppression
        void postprocess(const cv::Size &frameDims, const std::vector<cv::Mat>& out, size_t frame_number);

        void NormalizeTrackedClasses();

        // Get the names of the output layers
        std::vector<cv::String> getOutputsNames(const cv::dnn::Net& net);

        public:

        std::map<size_t, CVDetectionData> detectionsData;

        CVObjectDetection(std::string processInfoJson, ProcessingController &processingController);

        static std::string ValidateONNXModel(std::string modelPath);

        // Iterate over a clip object and run inference for each video frame
        void detectObjectsClip(openshot::Clip &video, size_t start=0, size_t end=0, bool process_interval=false);

        CVDetectionData GetDetectionData(size_t frameId);

        /// Protobuf Save and Load methods
        // Save protobuf file
        bool SaveObjDetectedData();
        // Add frame object detection data into protobuf message.
        void AddFrameDataToProto(pb_objdetect::Frame* pbFrameData, CVDetectionData& dData);

        // Get and Set JSON methods
        void SetJson(const std::string value); ///< Load JSON string into this object
        void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object

        // Load protobuf file (ONLY FOR MAKE TEST)
        bool _LoadObjDetectdData();
    };

}
