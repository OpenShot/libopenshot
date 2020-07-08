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

#include <cmath>
#include <stdio.h>
#include <memory>
#include "../Color.h"
#include "../Json.h"
#include "../KeyFrame.h"
#include "../CVTracker.h"
#include "../Clip.h"
#include "../trackerdata.pb.h"


namespace openshot
{

//     struct FrameData{
//   int frame_id = -1;
//   float rotation = 0;
//   int x1 = -1;
//   int y1 = -1;
//   int x2 = -1;
//   int y2 = -1;

//   // Keyframe kf_x1;
//   // Keyframe kf_y1;
//   // Keyframe kf_x2;
//   // Keyframe kf_y2;

//   // constructor
//   FrameData()
//   {}
//   FrameData( int _frame_id)
//   {frame_id = _frame_id;}

//   FrameData( int _frame_id , float _rotation, int _x1, int _y1, int _x2, int _y2)
//   {
//       frame_id = _frame_id;
//       rotation = _rotation;
//       x1 = _x1;
//       y1 = _y1;
//       x2 = _x2;
//       y2 = _y2;
      
//       // kf_x1.AddPoint(_frame_id, _x1);
//       // kf_y1.AddPoint(_frame_id, _y1);
//       // kf_x2.AddPoint(_frame_id, _x2);
//       // kf_y2.AddPoint(_frame_id, _y2);
//   }
// };


    //TODO: fix this
	/**
	 * @brief This class draws black bars around your video (from any side), and can be animated with
	 * openshot::Keyframe curves over time.
	 *
	 * Adding bars around your video can be done for cinematic reasons, and creates a fun way to frame
	 * in the focal point of a scene. The bars can be any color, and each side can be animated independently.
	 */
	class Tracker : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();


	public:
		Color color;		///< Color of bars
		Keyframe left;		///< Size of left bar
		Keyframe top;		///< Size of top bar
		Keyframe right;		///< Size of right bar
		Keyframe bottom;	///< Size of bottom bar

        std::map<int, FrameData> trackedDataById; 

		/// Blank constructor, useful when using Json to load the effect properties
		Tracker(std::string clipTrackerDataPath);

		/// Default constructor, which takes 4 curves and a color. These curves animated the bars over time.
		///
		/// @param color The curve to adjust the color of bars
		/// @param left The curve to adjust the left bar size (between 0 and 1)
		/// @param top The curve to adjust the top bar size (between 0 and 1)
		/// @param right The curve to adjust the right bar size (between 0 and 1)
		/// @param bottom The curve to adjust the bottom bar size (between 0 and 1)
		Tracker(Color color, Keyframe left, Keyframe top, Keyframe right, Keyframe bottom);

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

        bool LoadTrackedData(std::string inputFilePath);
        
        FrameData GetTrackedData(int frameId);

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
