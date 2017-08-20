/**
 * @file
 * @brief Header file for ClipBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

/// Do not include the juce unittest headers, because it collides with unittest++
#ifndef __JUCE_UNITTEST_JUCEHEADER__
	#define __JUCE_UNITTEST_JUCEHEADER__
#endif

#include <memory>
#include <sstream>
#include "Exceptions.h"
#include "Point.h"
#include "KeyFrame.h"
#include "Json.h"

using namespace std;

namespace openshot {

	/**
	 * @brief This abstract class is the base class, used by all clips in libopenshot.
	 *
	 * Clips are objects that attach to the timeline and can be layered and positioned
	 * together. There are 2 primary types of clips: Effects and Video/Audio Clips.
	 */
	class ClipBase {
	protected:
		string id; ///< ID Property for all derived Clip and Effect classes.
		float position; ///< The position on the timeline where this clip should start playing
		int layer; ///< The layer this clip is on. Lower clips are covered up by higher clips.
		float start; ///< The position in seconds to start playing (used to trim the beginning of a clip)
		float end; ///< The position in seconds to end playing (used to trim the ending of a clip)
		string previous_properties; ///< This string contains the previous JSON properties
		int max_width; ///< The maximum image width needed by this clip (used for optimizations)
		int max_height; ///< The maximium image height needed by this clip (used for optimizations)

		/// Generate JSON for a property
		Json::Value add_property_json(string name, float value, string type, string memo, Keyframe* keyframe, float min_value, float max_value, bool readonly, long int requested_frame);

		/// Generate JSON choice for a property (dropdown properties)
		Json::Value add_property_choice_json(string name, int value, int selected_value);

	public:

		/// Constructor for the base clip
		ClipBase() { max_width = 0; max_height = 0; };

		// Compare a clip using the Position() property
		bool operator< ( ClipBase& a) { return (Position() < a.Position()); }
		bool operator<= ( ClipBase& a) { return (Position() <= a.Position()); }
		bool operator> ( ClipBase& a) { return (Position() > a.Position()); }
		bool operator>= ( ClipBase& a) { return (Position() >= a.Position()); }

		/// Get basic properties
		string Id() { return id; } ///< Get the Id of this clip object
		float Position() { return position; } ///< Get position on timeline (in seconds)
		int Layer() { return layer; } ///< Get layer of clip on timeline (lower number is covered by higher numbers)
		float Start() { return start; } ///< Get start position (in seconds) of clip (trim start of video)
		float End() { return end; } ///< Get end position (in seconds) of clip (trim end of video)
		float Duration() { return end - start; } ///< Get the length of this clip (in seconds)

		/// Set basic properties
		void Id(string value) { id = value; } ///> Set the Id of this clip object
		void Position(float value) { position = value; } ///< Set position on timeline (in seconds)
		void Layer(int value) { layer = value; } ///< Set layer of clip on timeline (lower number is covered by higher numbers)
		void Start(float value) { start = value; } ///< Set start position (in seconds) of clip (trim start of video)
		void End(float value) { end = value; } ///< Set end position (in seconds) of clip (trim end of video)

		/// Set Max Image Size (used for performance optimization)
		void SetMaxSize(int width, int height) { max_width = width; max_height = height;  };

		/// Get and Set JSON methods
		virtual string Json() = 0; ///< Generate JSON string of this object
		virtual void SetJson(string value) throw(InvalidJSON) = 0; ///< Load JSON string into this object
		virtual Json::Value JsonValue() = 0; ///< Generate Json::JsonValue for this object
		virtual void SetJsonValue(Json::Value root) = 0; ///< Load Json::JsonValue into this object

		/// Get all properties for a specific frame (perfect for a UI to display the current state
		/// of all properties at any time)
		virtual string PropertiesJSON(long int requested_frame) = 0;

	};


}

#endif
