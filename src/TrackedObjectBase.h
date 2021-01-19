/**
 * @file
 * @brief Header file for the TrackedObjectBase class
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

#ifndef OPENSHOT_TRACKEDOBJECTBASE_H
#define OPENSHOT_TRACKEDOBJECTBASE_H

#include <iostream>
#include <iomanip>
#include <cmath>
#include <assert.h>
#include <vector>
#include <string>
#include "Exceptions.h"
#include "Fraction.h"
#include "Coordinate.h"
#include "Point.h"
#include "Json.h"
#include "ClipBase.h"


namespace openshot {
    /**
	 * @brief This abstract class is the base class of all Keyframes.
	 *
	 * A Keyframe is a collection of Point instances, which is used to vary a number or property over time.
	 *
	 * Keyframes are used to animate and interpolate values of properties over time.  For example, a single property
	 * can use a Keyframe instead of a constant value.  Assume you want to slide an image (from left to right) over
	 * a video.  You can create a Keyframe which will adjust the X value of the image over 100 frames (or however many
	 * frames the animation needs to last) from the value of 0 to 640.
	 */

    class TrackedObjectBase {
	private:
		std::string id;
		ClipBase* parentClip;

    public:
		
        /// Blank constructor
        TrackedObjectBase();

		/// Default constructor
		TrackedObjectBase(std::string _id);

        /// Get and set the id of this object
		std::string Id() const { return id; }
		void Id(std::string _id) { id = _id; }
        /// Get and set the parentClip of this object
		ClipBase* ParentClip() const { return parentClip; }
		void ParentClip(ClipBase* clip) { parentClip = clip; }

        /// Scale a property
        virtual void ScalePoints(double scale) { return; };
		/// Return the main properties of a TrackedObjectBBox instance using a pointer to this base class
		virtual std::map<std::string, float> GetBoxValues(int64_t frame_number) const { std::map<std::string, float> ret; return ret; };
        /// Return the main properties of the tracked object's parent clip
        virtual std::map<std::string, float> GetParentClipProperties(int64_t frame_number) const { std::map<std::string, float> ret; return ret; }
    
		/// Get and Set JSON methods
        virtual std::string Json() const = 0;                  ///< Generate JSON string of this object
        virtual Json::Value JsonValue() const = 0;             ///< Generate Json::Value for this object
        virtual void SetJson(const std::string value) = 0;     ///< Load JSON string into this object
        virtual void SetJsonValue(const Json::Value root) = 0; ///< Load Json::Value into this object

	
	};
} // Namespace openshot

#endif
