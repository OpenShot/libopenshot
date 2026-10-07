/**
 * @file
 * @brief Header file for CVObjectMask class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#define int64 opencv_broken_int
#define uint64 opencv_broken_uint
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>
#undef uint64
#undef int64

#include "Clip.h"
#include "Json.h"
#include "ProcessingController.h"

namespace pb_objdetect {
    class Frame;
}

namespace openshot
{
    struct CVObjectMaskFrameData {
        size_t frameId = 0;
        cv::Rect_<float> box;
        float score = 0.0f;
        int objectId = 1;
        int width = 0;
        int height = 0;
        std::vector<uint32_t> rle;

        bool HasMask() const { return width > 0 && height > 0 && !rle.empty(); }
    };

    struct CVObjectMaskPromptSet {
        std::vector<cv::Point2f> positivePoints;
        std::vector<cv::Point2f> negativePoints;
        std::vector<cv::Rect_<float>> positiveRects;
        std::vector<cv::Rect_<float>> negativeRects;

        bool HasPositivePrompt() const { return !positiveRects.empty() || !positivePoints.empty(); }
    };

    /**
     * @brief Preprocess a clip into EfficientSAM/Cutie object masks stored in the object-detection protobuf format.
     */
    class CVObjectMask
    {
    private:
        cv::dnn::Net efficientSam;

        std::string efficientSamModelPath;
        std::string cutieModelDir;
        std::string cutieEncodeKeyModelPath;
        std::string cutieEncodeValueModelPath;
        std::string cutieMemoryReadoutModelPath;
        std::string cutieDecodeModelPath;
        std::string protobufDataPath;
        std::string processingDevice = "CPU";

        std::map<size_t, CVObjectMaskPromptSet> promptKeyframes;
        int promptSlots = 6;
        float maskThreshold = 0.0f;
        int modelSize = 1024;

        size_t start = 0;
        size_t end = 0;
        bool error = false;

        ProcessingController* processingController;

        void SetProcessingDevice();
        cv::Mat CreateEfficientSAMSeedMask(const cv::Mat& frame, const CVObjectMaskPromptSet& prompts);
        void AddFrameDataToProto(pb_objdetect::Frame* pbFrameData, const CVObjectMaskFrameData& frameData);

    public:
        std::map<size_t, CVObjectMaskFrameData> masksData;

        CVObjectMask(std::string processInfoJson, ProcessingController& processingController);

        static std::string ValidateONNXModel(std::string modelPath);
        std::shared_ptr<Frame> PreviewSeedMask(std::shared_ptr<Frame> frame);

        void maskClip(openshot::Clip& video, size_t start = 0, size_t end = 0, bool process_interval = false);
        bool SaveObjMaskData();

        void SetJson(const std::string value);
        void SetJsonValue(const Json::Value root);
    };
}
