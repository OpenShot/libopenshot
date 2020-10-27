/**
 * @file
 * @brief Header file for ClipBase class
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

#ifndef OPENSHOT_CLIPBASE_H
#define OPENSHOT_CLIPBASE_H

#include <memory>
#include <sstream>
#include "CacheMemory.h"
#include "Exceptions.h"
#include "Frame.h"
#include "Point.h"
#include "KeyFrame.h"
#include "Json.h"
#include "TimelineBase.h"


namespace openshot {
	/**
	 * @brief This abstract class is the base class, used by all clips in libopenshot.
	 *
	 * Clips are objects that attach to the timeline and can be layered and positioned
	 * together. There are 2 primary types of clips: Effects and Video/Audio Clips.
	 */
	class ClipBase {
	protected:
		std::string id; ///< ID Property for all derived Clip and Effect classes.
		float position; ///< The position on the timeline where this clip should start playing
		int layer; ///< The layer this clip is on. Lower clips are covered up by higher clips.
		float start; ///< The position in seconds to start playing (used to trim the beginning of a clip)
		float end; ///< The position in seconds to end playing (used to trim the ending of a clip)
		std::string previous_properties; ///< This string contains the previous JSON properties
		openshot::TimelineBase* timeline; ///< Pointer to the parent timeline instance (if any)

		/// Generate JSON for a property
		Json::Value add_property_json(std::string name, float value, std::string type, std::string memo, const Keyframe* keyframe, float min_value, float max_value, bool readonly, int64_t requested_frame) const;

		/// Generate JSON choice for a property (dropdown properties)
		Json::Value add_property_choice_json(std::string name, int value, int selected_value) const;

	public:
		CacheMemory cache;

		/// Constructor for the base clip
		ClipBase() {
			// Initialize values
			position = 0.0;
			layer = 0;
			start = 0.0;
			end = 0.0;
			previous_properties = "";
			timeline = NULL;
		};

		// Compare a clip using the Position() property
		bool operator< ( ClipBase& a) { return (Position() < a.Position()); }
		bool operator<= ( ClipBase& a) { return (Position() <= a.Position()); }
		bool operator> ( ClipBase& a) { return (Position() > a.Position()); }
		bool operator>= ( ClipBase& a) { return (Position() >= a.Position()); }

		/// @brief This method is required for all derived classes of ClipBase, and returns a
		/// new openshot::Frame object. All Clip keyframes and effects are resolved into
		/// pixels.
		///
		/// @returns A new openshot::Frame object
		/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
		virtual std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) = 0;

		/// @brief This method is required for all derived classes of ClipBase, and returns a
		/// modified openshot::Frame object
		///
		/// The frame object is passed into this method and used as a starting point (pixels and audio).
		/// All Clip keyframes and effects are resolved into pixels.
		///
		/// @returns The modified openshot::Frame object
		/// @param frame The frame object that needs the clip or effect applied to it
		/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
		virtual std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) = 0;

		/// Get basic properties
		std::string Id() const { return id; } ///< Get the Id of this clip object
		float Position() const { return position; } ///< Get position on timeline (in seconds)
		int Layer() const { return layer; } ///< Get layer of clip on timeline (lower number is covered by higher numbers)
		float Start() const { return start; } ///< Get start position (in seconds) of clip (trim start of video)
		float End() const { return end; } ///< Get end position (in seconds) of clip (trim end of video)
		float Duration() const { return end - start; } ///< Get the length of this clip (in seconds)
		openshot::TimelineBase* ParentTimeline() { return timeline; } ///< Get the associated Timeline pointer (if any)

		/// Set basic properties
		void Id(std::string value) { id = value; } ///> Set the Id of this clip object
		void Position(float value) { position = value; } ///< Set position on timeline (in seconds)
		void Layer(int value) { layer = value; } ///< Set layer of clip on timeline (lower number is covered by higher numbers)
		void Start(float value) { start = value; } ///< Set start position (in seconds) of clip (trim start of video)
		void End(float value) { end = value; } ///< Set end position (in seconds) of clip (trim end of video)
		void ParentTimeline(openshot::TimelineBase* new_timeline) { timeline = new_timeline; } ///< Set associated Timeline pointer

		/// Get and Set JSON methods
		virtual std::string Json() const = 0; ///< Generate JSON string of this object
		virtual void SetJson(const std::string value) = 0; ///< Load JSON string into this object
		virtual Json::Value JsonValue() const = 0; ///< Generate Json::Value for this object
		virtual void SetJsonValue(const Json::Value root) = 0; ///< Load Json::Value into this object

		/// Get all properties for a specific frame (perfect for a UI to display the current state
		/// of all properties at any time)
		virtual std::string PropertiesJSON(int64_t requested_frame) const = 0;

		virtual ~ClipBase() = default;
	};


}

#endif
