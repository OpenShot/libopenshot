/**
 * @file
 * @brief Header file for the TrackedObjectBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author Brenno Caldato <brenno.caldato@outlook.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_TRACKEDOBJECTBASE_H
#define OPENSHOT_TRACKEDOBJECTBASE_H

#include <vector>
#include <string>

#include "KeyFrame.h"
#include "Json.h"

namespace openshot {

	// Forward decls
	class ClipBase;

	/**
	 * @brief This abstract class is the base class of all Tracked Objects.
	 *
	 * A Tracked Object is an object or a desired set of pixels in a digital image
	 * which properties (such as position, width and height) can be detected and
	 * predicted along the frames of a clip.
	 */
	class TrackedObjectBase {
	protected:
		std::string id;
		ClipBase* parentClip;

	public:

	    /// Keyframe to track if a box is visible in the current frame (read-only)
		Keyframe visible;

	    /// Keyframe to determine if a specific box is drawn (or hidden)
		Keyframe draw_box;

		/// Default constructor
		TrackedObjectBase();

		/// Constructor which takes an object ID
		TrackedObjectBase(std::string _id);

		/// Destructor
		virtual ~TrackedObjectBase() = default;

		/// Get the id of this object
		std::string Id() const { return id; }
		/// Set the id of this object
		void Id(std::string _id) { id = _id; }
		/// Get and set the parentClip of this object
		ClipBase* ParentClip() const { return parentClip; }
		void ParentClip(ClipBase* clip) { parentClip = clip; }

		/// Check if there is data for the exact frame number
		virtual bool ExactlyContains(int64_t frame_number) const { return {}; };

		/// Scale an object's property
		virtual void ScalePoints(double scale) { return; };
		/// Return the main properties of a TrackedObjectBBox instance - such as position, size and rotation
		virtual std::map<std::string, float> GetBoxValues(int64_t frame_number) const { std::map<std::string, float> ret; return ret; };
		/// Add a bounding box to the tracked object's BoxVec map
		virtual void AddBox(int64_t _frame_num, float _cx, float _cy, float _width, float _height, float _angle) { return; };


		/// Get and Set JSON methods
		virtual std::string Json() const = 0;				  ///< Generate JSON string of this object
		virtual Json::Value JsonValue() const = 0;			 ///< Generate Json::Value for this object
		virtual void SetJson(const std::string value) = 0;	 ///< Load JSON string into this object
		virtual void SetJsonValue(const Json::Value root) = 0; ///< Load Json::Value into this object

		/// Get all properties for a specific frame (perfect for a UI to display the current state
		/// of all properties at any time)
		virtual Json::Value PropertiesJSON(int64_t requested_frame) const = 0;
		/// Generate JSON choice for a property (dropdown properties)
		Json::Value add_property_choice_json(std::string name, int value, int selected_value) const;
	};
}  // Namespace openshot

#endif
