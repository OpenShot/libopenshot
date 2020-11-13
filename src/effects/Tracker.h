/**
 * @file
 * @brief Header file for Tracker effect class
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

#ifndef OPENSHOT_TRACKER_EFFECT_H
#define OPENSHOT_TRACKER_EFFECT_H

#include "../EffectBase.h"

#include <google/protobuf/util/time_util.h>

#include <cmath>
#include <fstream>
#include <stdio.h>
#include <memory>
#include "../Color.h"
#include "../Json.h"
#include "../KeyFrame.h"
#include "../KeyFrameBBox.h"
#include "../Clip.h"
#include "trackerdata.pb.h"

using namespace std;
using google::protobuf::util::TimeUtil;


// Tracking info struct
struct EffectFrameData{
  size_t frame_id = -1;
  float rotation = 0;
  float x1 = -1;
  float y1 = -1;
  float x2 = -1;
  float y2 = -1;

  // Constructors
  EffectFrameData()
  {}

  EffectFrameData( int _frame_id)
  {frame_id = _frame_id;}

  EffectFrameData( int _frame_id , float _rotation, float _x1, float _y1, float _x2, float _y2)
  {
      frame_id = _frame_id;
      rotation = _rotation;
      x1 = _x1;
      y1 = _y1;
      x2 = _x2;
      y2 = _y2;
  }
};


namespace openshot
{
	/**
	 * @brief This class track a given object through the clip and, when called, draws a box surrounding it.
	 *
	 * Tracking is useful to better visualize and follow the movement of an object through video.
	 */
	class Tracker : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();
		std::string protobuf_data_path;
		Fraction BaseFPS;
	public:

        std::map<int, EffectFrameData> trackedDataById; // Save object tracking box data

		KeyFrameBBox trackedData;

		/// Blank constructor, useful when using Json to load the effect properties
		Tracker(std::string clipTrackerDataPath);

		/// Default constructor
		Tracker();

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
		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override { return GetFrame(std::shared_ptr<Frame> (new Frame()), frame_number); }
		
		// Load protobuf data file
        bool LoadTrackedData(std::string inputFilePath);
        
		// Get tracker info for the desired frame 
        BBox GetTrackedData(size_t frameId);

		/// Get and Set JSON methods
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
