/**
 * @file
 * @brief Header file for the IKeyframe class
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

#ifndef OPENSHOT_BBOXKEYFRAME_H
#define OPENSHOT_BBOXKEYFRAME_H

#include <iostream>
#include <iomanip>
#include <cmath>
#include <assert.h>
#include <vector>
#include "Exceptions.h"
#include "Fraction.h"
#include "Coordinate.h"
#include "Point.h"
#include "Json.h"
#include "IKeyFrame.h"
#include "KeyFrame.h"


struct BBox{
    float cx = -1;
    float cy = -1;
    float width = -1;
    float height = -1;

    // Constructors
    BBox(){
        return;
    }

    BBox(float _cx, float _cy, float _width, float _height){
        //frame_num = _frame_num;
        cx = _cx;
        cy = _cy;
        width = _width;
        height = _height;
    }
};


namespace openshot {
	/**
	 * @brief A Keyframe is a collection of Point instances, which is used to vary a number or property over time.
	 *
	 * Keyframes are used to animate and interpolate values of properties over time.  For example, a single property
	 * can use a Keyframe instead of a constant value.  Assume you want to slide an image (from left to right) over
	 * a video.  You can create a Keyframe which will adjust the X value of the image over 100 frames (or however many
	 * frames the animation needs to last) from the value of 0 to 640.
	 *
	 * \endcode
	 */

    class KeyFrameBBox {
        private:
            bool visible;
            Fraction fps;
            std::map<double, BBox> BoxVec;
        public:
            Keyframe delta_x;
            Keyframe delta_y;
            Keyframe scale_x;
            Keyframe scale_y;
            
            KeyFrameBBox();
            
            void AddDisplacement(int64_t _frame_num, double _delta_x, double _delta_y);
            void AddScale(int64_t _frame_num, double _delta_x, double _delta_y);
            void AddBox(int64_t _frame_num, float _cx, float _cy, float _width, float _height);
            
            void SetFPS(Fraction fps);
            Fraction GetFPS();

            bool Contains(int64_t frame_number);
            //double GetDelta(int64_t index) const ;
            int64_t GetLength() const;
            
            /// Get and Set JSON methods
		    //std::string Json() const ; ///< Generate JSON string of this object
		    //Json::Value JsonValue() const ; ///< Generate Json::Value for this object
		    //void SetJson(const std::string value) ; ///< Load JSON string into this object
		    //void SetJsonValue(const Json::Value root) ; ///< Load Json::Value into this object

            /// Remove a points by frame_number
            void RemovePoint(int64_t frame_number);
            void RemoveDelta(int64_t frame_number);
            void RemoveScale(int64_t frame_number); 

            BBox GetValue(int64_t frame_number);

            /// Print collection of points
            void PrintParams();

            double FrameNToTime(int64_t frame_number);
            BBox InterpolateBoxes(double t1, double t2, BBox left, BBox right, double target);

    };

}

#endif