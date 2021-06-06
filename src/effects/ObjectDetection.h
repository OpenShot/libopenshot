/**
 * @file
 * @brief Header file for Object Detection effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
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

#ifndef OPENSHOT_OBJECT_DETECTION_EFFECT_H
#define OPENSHOT_OBJECT_DETECTION_EFFECT_H

#include "../EffectBase.h"

#include <cmath>
#include <stdio.h>
#include <memory>
#include <opencv2/opencv.hpp>
#include "../Color.h"
#include "../Json.h"
#include "../KeyFrame.h"
#include "protobuf_messages/objdetectdata.pb.h"

// Struct that stores the detected bounding boxes for all the clip frames
struct DetectionData{
    DetectionData(){}
    DetectionData(
        std::vector<int> _classIds,
        std::vector<float> _confidences,
        std::vector<cv::Rect_<float>> _boxes,
        size_t _frameId,
        std::vector<int> _objectIds)
    {
        classIds = _classIds;
        confidences = _confidences;
        boxes = _boxes;
        frameId = _frameId;
        objectIds = _objectIds;
    }
    size_t frameId;
    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect_<float>> boxes;
    std::vector<int> objectIds;
};

namespace openshot
{
    /**
     * @brief This effect displays all the detected objects on a clip.
     */
    class ObjectDetection : public EffectBase
    {
    private:
        std::string protobuf_data_path;
        std::map<size_t, DetectionData> detectionsData;
        std::vector<std::string> classNames;

        std::vector<cv::Scalar> classesColor;
        
        /// Draw class name and confidence score on top of the bounding box
        Keyframe display_box_text;
        /// Minimum confidence value to display the detected objects
        float confidence_threshold = 0.5; 
        /// Contain the user selected classes for visualization
        std::vector<std::string> display_classes;
        std::string class_filter;

        /// Init effect settings
        void init_effect_details();
        /// Draw bounding box with class and score text 
        void drawPred(int classId, float conf, cv::Rect2d box, cv::Mat& frame, int objectNumber, std::vector<int> color, float alpha, 
                        int thickness, bool is_background, bool draw_text);
        /// Draw rotated rectangle with alpha channel
        void DrawRectangleRGBA(cv::Mat &frame_image, cv::RotatedRect box, std::vector<int> color, float alpha, int thickness, bool is_background);


    public:
        /// Index of the Tracked Object that was selected to modify it's properties
        int selectedObjectIndex;

        ObjectDetection(std::string clipTrackerDataPath);

        /// Default constructor
        ObjectDetection();

        /// @brief This method is required for all derived classes of EffectBase, and returns a
        /// modified openshot::Frame object
        ///
        /// The frame object is passed into this method, and a frame_number is passed in which
        /// tells the effect which settings to use from its keyframes (starting at 1).
        ///
        /// @returns The modified openshot::Frame object
        /// @param frame The frame object that needs the effect applied to it
        /// @param frame_number The frame number (starting at 1) of the effect on the timeline.
        std::shared_ptr<Frame> GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number) override;

        std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override { return GetFrame(std::make_shared<Frame>(), frame_number); }

        /// Load protobuf data file
        bool LoadObjDetectdData(std::string inputFilePath);

        /// Get the indexes and IDs of all visible objects in the given frame
        std::string GetVisibleObjects(int64_t frame_number) const override;

        // Get and Set JSON methods
        std::string Json() const override; ///< Generate JSON string of this object
        void SetJson(const std::string value) override; ///< Load JSON string into this object
        Json::Value JsonValue() const override; ///< Generate Json::Value for this object
        void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

        /// Get all properties for a specific frame (perfect for a UI to display the current state
        /// of all properties at any time)
        std::string PropertiesJSON(int64_t requested_frame) const override;
    };

}

#endif
