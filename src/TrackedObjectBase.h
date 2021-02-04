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
#include "KeyFrame.h"
#include "Point.h"
#include "Json.h"
#include "ClipBase.h"


namespace openshot {
    /**
	 * @brief This abstract class is the base class of all Tracked Objects.
	 *
	 * A Tracked Object is an object or a desired set of pixels in a digital image
	 * which properties (such as position, width and height) can be detected and 
	 * predicted along the frames of a clip.
	 */
    class TrackedObjectBase {
	private:
		std::string id;
		ClipBase* parentClip;
		

    public:
		
		Keyframe visible;

        /// Blank constructor
        TrackedObjectBase();

		/// Default constructor
		TrackedObjectBase(std::string _id);

        /// Get the id of this object
		std::string Id() const { return id; }
		/// Set the id of this object
		void Id(std::string _id) { id = _id; }
        /// Get and set the parentClip of this object
		ClipBase* ParentClip() const { return parentClip; }
		void ParentClip(ClipBase* clip) { parentClip = clip; }

        /// Scale an object's property
        virtual void ScalePoints(double scale) { return; };
		/// Return the main properties of a TrackedObjectBBox instance - such as position, size and rotation
		virtual std::map<std::string, float> GetBoxValues(int64_t frame_number) const { std::map<std::string, float> ret; return ret; };
        /// Return the main properties of the tracked object's parent clip - such as position, size and rotation
        virtual std::map<std::string, float> GetParentClipProperties(int64_t frame_number) const { std::map<std::string, float> ret; return ret; }
		/// Add a bounding box to the tracked object's BoxVec map
		virtual void AddBox(int64_t _frame_num, float _cx, float _cy, float _width, float _height, float _angle) { return; };


		/// Get and Set JSON methods
        virtual std::string Json() const = 0;                  ///< Generate JSON string of this object
        virtual Json::Value JsonValue() const = 0;             ///< Generate Json::Value for this object
        virtual void SetJson(const std::string value) = 0;     ///< Load JSON string into this object
        virtual void SetJsonValue(const Json::Value root) = 0; ///< Load Json::Value into this object

		/// Get all properties for a specific frame (perfect for a UI to display the current state
		/// of all properties at any time)
		virtual Json::Value PropertiesJSON(int64_t requested_frame) const = 0;
	
	};
} // Namespace openshot

#endif
