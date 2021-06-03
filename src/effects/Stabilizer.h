/**
 * @file
 * @brief Header file for Stabilizer effect class
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

#ifndef OPENSHOT_STABILIZER_EFFECT_H
#define OPENSHOT_STABILIZER_EFFECT_H

#include "../EffectBase.h"

#include <cmath>
#include <stdio.h>
#include <memory>
#include "../Color.h"
#include "../Json.h"
#include "../KeyFrame.h"
#include "protobuf_messages/stabilizedata.pb.h"

// Store the relative transformation parameters between consecutive frames
struct EffectTransformParam
{
    EffectTransformParam() {}
    EffectTransformParam(double _dx, double _dy, double _da) {
        dx = _dx;
        dy = _dy;
        da = _da;
    }

    double dx;
    double dy;
    double da; // angle
};

// Stores the global camera trajectory for one frame
struct EffectCamTrajectory
{
    EffectCamTrajectory() {}
    EffectCamTrajectory(double _x, double _y, double _a) {
        x = _x;
        y = _y;
        a = _a;
    }

    double x;
    double y;
    double a; // angle
};


namespace openshot
{

    /**
     * @brief This class stabilizes a video clip to remove undesired shaking and jitter.
     *
     * Adding stabilization is useful to increase video quality overall, since it removes
     * from subtle to harsh unexpected camera movements.
     */
    class Stabilizer : public EffectBase
    {
    private:
        /// Init effect settings
        void init_effect_details();
        std::string protobuf_data_path;
        Keyframe zoom;

    public:
        std::string teste;
        std::map <size_t,EffectCamTrajectory> trajectoryData; // Save camera trajectory data
        std::map <size_t,EffectTransformParam> transformationData; // Save transormation data

        Stabilizer();

        Stabilizer(std::string clipTrackerDataPath);

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

        std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override {
            return GetFrame(std::make_shared<openshot::Frame>(), frame_number);
        };

        /// Load protobuf data file
        bool LoadStabilizedData(std::string inputFilePath);

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
