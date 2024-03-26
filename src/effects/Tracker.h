/**
 * @file
 * @brief Header file for Tracker effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_TRACKER_EFFECT_H
#define OPENSHOT_TRACKER_EFFECT_H

#include <string>
#include <memory>
#include <map>

#include "EffectBase.h"

#include "Json.h"
#include "KeyFrame.h"

#include "TrackedObjectBBox.h"

namespace openshot
{
    // Forwards decls
    class Frame;
    class TrackedObjectBBox;

    /**
     * @brief This class tracks a given object through the clip, draws a box around it and allow
     * the user to attach another clip (image or video) to the tracked object.
     *
     * Tracking is useful to better visualize, follow the movement of an object through video
     * and attach an image or video to it.
     */
    class Tracker : public EffectBase
    {
    private:
        /// Init effect settings
        void init_effect_details();

        Fraction BaseFPS;
        double TimeScale;

    public:
        std::string protobuf_data_path; ///< Path to the protobuf file that holds the bounding-box data
        std::shared_ptr<TrackedObjectBBox> trackedData; ///< Pointer to an object that holds the bounding-box data and it's Keyframes

        /// Default constructor
        Tracker();

        Tracker(std::string clipTrackerDataPath);

        /// @brief Apply this effect to an openshot::Frame
        ///
        /// @returns The modified openshot::Frame object
        /// @param frame The frame object that needs the effect applied to it
        /// @param frame_number The frame number (starting at 1) of the effect on the timeline.
        std::shared_ptr<Frame> GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number) override;

        std::shared_ptr<openshot::Frame>
        GetFrame(int64_t frame_number) override {
            return GetFrame(std::shared_ptr<Frame>(new Frame()), frame_number);
        }

        /// Get the indexes and IDs of all visible objects in the given frame
        std::string GetVisibleObjects(int64_t frame_number) const override;

        // Get and Set JSON methods

        /// Generate JSON string of this object
        std::string Json() const override;
        /// Load JSON string into this object
        void SetJson(const std::string value) override;
        /// Generate Json::Value for this object
        Json::Value JsonValue() const override;
        /// Load Json::Value into this object
        void SetJsonValue(const Json::Value root) override;

        /// Get all properties for a specific frame
        ///
        /// (perfect for a UI to display the current state
        /// of all properties at any time)
        std::string PropertiesJSON(int64_t requested_frame) const override;
        };

}

#endif
